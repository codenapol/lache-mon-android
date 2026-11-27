// Lache mon Android ! - Découverte automatique du port du débogage sans fil
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "decouverte.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <set>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

#include "outils.h" // executer, nettoyer

namespace
{
    // Ports où Android peut placer le débogage sans fil : plage des ports éphémères du noyau
    // (32768 à 60999), élargie vers le bas, plus 5555 (« adb tcpip » classique)
    const int PORT_MIN = 30000;
    const int PORT_MAX = 60999;
    const int PORT_ADB_CLASSIQUE = 5555;

    const size_t SOCKETS_PAR_LOT   = 512;  // reste loin de la limite habituelle de 1024 fichiers ouverts
    const int    DELAI_LOT_MS      = 300;  // réseau local : un port fermé répond en quelques ms
    const int    LOTS_SANS_REPONSE = 3;    // au-delà, le téléphone est considéré injoignable
    const int    DELAI_SONDE_MS    = 1500;
    const size_t PORTS_MAX_SONDES  = 20;   // au-delà, trop de ports ouverts pour les départager

    // ---------- Protocole adb (message de 24 octets, entiers little-endian) ----------
    const uint32_t ADB_CNXN = 0x4e584e43; // "CNXN"
    const uint32_t ADB_AUTH = 0x48545541; // "AUTH"
    const uint32_t ADB_STLS = 0x534c5453; // "STLS" (débogage sans fil, passage en TLS)

    void ecrire_u32(unsigned char* p, uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            p[i] = (unsigned char)(v >> (8 * i));
    }

    uint32_t lire_u32(const unsigned char* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    sockaddr_in adresse(const std::string& ip, int port)
    {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port   = htons((uint16_t)port);
        inet_pton(AF_INET, ip.c_str(), &a.sin_addr);
        return a;
    }

    // Socket non bloquante dont la connexion est lancée (-1 en cas d'erreur immédiate)
    int lancer_connexion(const std::string& ip, int port)
    {
        const int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (s < 0)
            return -1;
        const sockaddr_in a = adresse(ip, port);
        if (connect(s, (const sockaddr*)&a, sizeof(a)) == 0 || errno == EINPROGRESS)
            return s;
        close(s);
        return -1;
    }

    // Attend qu'une socket soit prête (POLLIN ou POLLOUT) ; false si délai dépassé
    bool attendre(int s, short evenement, int delai_ms)
    {
        pollfd p{s, evenement, 0};
        return poll(&p, 1, delai_ms) == 1 && (p.revents & evenement);
    }

    // Vérifie que `port` répond comme adb : on envoie CNXN, adb répond CNXN, AUTH ou STLS.
    // Aucune autorisation n'est demandée sur le téléphone : la connexion est fermée aussitôt.
    bool repond_comme_adb(const std::string& ip, int port)
    {
        const int s = lancer_connexion(ip, port);
        if (s < 0)
            return false;
        bool adb = false;
        int err = 0;
        socklen_t l = sizeof(err);
        if (attendre(s, POLLOUT, DELAI_SONDE_MS) && getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &l) == 0 && err == 0)
        {
            const char charge[] = "host::"; // envoyé avec son '\0' final
            const uint32_t taille = sizeof(charge);
            uint32_t somme = 0;
            for (char c : charge) somme += (unsigned char)c;

            unsigned char message[24 + sizeof(charge)];
            ecrire_u32(message + 0,  ADB_CNXN);
            ecrire_u32(message + 4,  0x01000001); // version du protocole
            ecrire_u32(message + 8,  256 * 1024); // taille maximale des données
            ecrire_u32(message + 12, taille);
            ecrire_u32(message + 16, somme);
            ecrire_u32(message + 20, ADB_CNXN ^ 0xffffffff);
            std::memcpy(message + 24, charge, taille);

            if (send(s, message, sizeof(message), MSG_NOSIGNAL) == (ssize_t)sizeof(message))
            {
                unsigned char reponse[24];
                size_t recus = 0;
                while (recus < sizeof(reponse) && attendre(s, POLLIN, DELAI_SONDE_MS))
                {
                    const ssize_t n = recv(s, reponse + recus, sizeof(reponse) - recus, 0);
                    if (n <= 0)
                        break;
                    recus += (size_t)n;
                }
                if (recus == sizeof(reponse))
                {
                    const uint32_t commande = lire_u32(reponse);
                    const uint32_t magique  = lire_u32(reponse + 20);
                    adb = (commande == ADB_CNXN || commande == ADB_AUTH || commande == ADB_STLS) &&
                          magique == (commande ^ 0xffffffff);
                }
            }
        }
        close(s);
        return adb;
    }

    // Ports annoncés en mDNS par le téléphone `ip` (débogage sans fil ou adb tcpip)
    std::set<int> ports_mdns(const std::string& ip)
    {
        std::set<int> ports;
        // « adb-XXXX-yyyy	_adb-tls-connect._tcp	192.168.1.42:37123 »
        std::istringstream lignes(executer("adb mdns services"));
        std::string ligne;
        while (std::getline(lignes, ligne))
        {
            if (ligne.find("_adb-tls-connect._tcp") == std::string::npos &&
                ligne.find("_adb._tcp") == std::string::npos)
                continue; // « _adb-tls-pairing._tcp » est le port d'association, pas de connexion
            const std::string adresse_port = nettoyer(ligne.substr(ligne.find_last_of(" \t") + 1));
            const size_t deux_points = adresse_port.rfind(':');
            if (deux_points == std::string::npos || adresse_port.substr(0, deux_points) != ip)
                continue;
            const int port = std::atoi(adresse_port.c_str() + deux_points + 1);
            if (port > 0 && port <= 65535)
                ports.insert(port);
        }
        return ports;
    }

    // Scan TCP de `ports` par lots. `joignable` passe à false si le téléphone ne répond jamais.
    std::vector<int> scanner(const std::string& ip, const std::vector<int>& ports, bool& joignable)
    {
        std::vector<int> ouverts;
        bool une_reponse = false; // au moins un port a répondu (ouvert ou refusé)
        int lots_muets = 0;

        for (size_t debut = 0; debut < ports.size(); debut += SOCKETS_PAR_LOT)
        {
            const size_t fin = std::min(debut + SOCKETS_PAR_LOT, ports.size());
            std::vector<pollfd> attente;
            std::vector<int>    port_de;
            for (size_t k = debut; k < fin; ++k)
            {
                const int s = lancer_connexion(ip, ports[k]);
                if (s < 0)
                    continue;
                attente.push_back({s, POLLOUT, 0});
                port_de.push_back(ports[k]);
            }

            bool reponse_lot = false;
            const auto limite = std::chrono::steady_clock::now() + std::chrono::milliseconds(DELAI_LOT_MS);
            size_t restants = attente.size();
            while (restants > 0)
            {
                const int reste_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                    limite - std::chrono::steady_clock::now()).count();
                if (reste_ms <= 0 || poll(attente.data(), attente.size(), reste_ms) <= 0)
                    break;
                for (size_t k = 0; k < attente.size(); ++k)
                {
                    if (attente[k].fd < 0 || attente[k].revents == 0)
                        continue;
                    int err = 0;
                    socklen_t l = sizeof(err);
                    getsockopt(attente[k].fd, SOL_SOCKET, SO_ERROR, &err, &l);
                    if (err == 0)
                        ouverts.push_back(port_de[k]);
                    if (err == 0 || err == ECONNREFUSED)
                        reponse_lot = true;
                    close(attente[k].fd);
                    attente[k].fd = -1; // ignorée par poll désormais
                    --restants;
                }
            }
            for (pollfd& p : attente)
                if (p.fd >= 0)
                    close(p.fd);

            une_reponse = une_reponse || reponse_lot;
            if (!une_reponse && ++lots_muets >= LOTS_SANS_REPONSE)
                break; // aucune réponse du tout : inutile de scanner le reste
        }
        joignable = une_reponse;
        return ouverts;
    }

    std::string liste_ports(const std::vector<int>& ports)
    {
        std::string texte;
        for (size_t k = 0; k < ports.size() && k < 10; ++k)
            texte += (k ? ", " : "") + std::to_string(ports[k]);
        if (ports.size() > 10)
            texte += "...";
        return texte;
    }
}

namespace decouverte
{
    bool ipv4_valide(const std::string& ip)
    {
        in_addr a{};
        if (inet_pton(AF_INET, ip.c_str(), &a) != 1) // n'accepte que a.b.c.d en décimal
            return false;
        const uint32_t hote = ntohl(a.s_addr);
        const uint32_t premier_octet = hote >> 24;
        // 0.x.x.x (« ce réseau »), multicast et adresses réservées / diffusion exclus
        return premier_octet != 0 && premier_octet < 224;
    }

    ResultatDecouverte trouver_port(const std::string& ip)
    {
        ResultatDecouverte r;
        if (!ipv4_valide(ip))
        {
            r.erreur = "« " + ip + " » n'est pas une adresse IP valide (exemple : 192.168.1.42).";
            return r;
        }

        // 1. Annonce mDNS
        std::vector<int> confirmes;
        for (int port : ports_mdns(ip))
            if (repond_comme_adb(ip, port))
                confirmes.push_back(port);
        std::clog << "[DECOUVERTE] mDNS : " << confirmes.size() << " port(s) adb pour " << ip << std::endl;
        if (confirmes.size() == 1)
        {
            r.port = confirmes[0];
            return r;
        }

        // 2. Scan TCP
        std::vector<int> ports{PORT_ADB_CLASSIQUE};
        for (int p = PORT_MIN; p <= PORT_MAX; ++p)
            ports.push_back(p);
        bool joignable = false;
        r.ports_ouverts = scanner(ip, ports, joignable);
        std::clog << "[DECOUVERTE] scan : " << r.ports_ouverts.size() << " port(s) ouvert(s)" << std::endl;

        if (!joignable)
        {
            r.erreur = "Le téléphone ne répond pas à l'adresse " + ip + ". Vérifie l'adresse IP, "
                       "que le téléphone est allumé, écran déverrouillé, et sur le même réseau Wi-Fi que ce PC.";
            return r;
        }
        if (r.ports_ouverts.empty())
        {
            r.erreur = "Aucun port de débogage sans fil trouvé sur " + ip + ". Vérifie que le débogage "
                       "sans fil est activé (Options pour les développeurs) : son port s'affiche aussi à cet endroit.";
            return r;
        }
        if (r.ports_ouverts.size() > PORTS_MAX_SONDES)
        {
            r.erreur = std::to_string(r.ports_ouverts.size()) + " ports ouverts sur " + ip +
                       " : trop pour reconnaître celui du débogage sans fil. Recopie le port affiché "
                       "dans Options pour les développeurs > Débogage sans fil.";
            return r;
        }

        confirmes.clear();
        for (int port : r.ports_ouverts)
            if (repond_comme_adb(ip, port))
                confirmes.push_back(port);
        std::clog << "[DECOUVERTE] " << confirmes.size() << " port(s) adb confirmé(s)" << std::endl;

        if (confirmes.size() == 1)
            r.port = confirmes[0];
        else if (confirmes.empty())
            r.erreur = "Port(s) ouvert(s) sur " + ip + " (" + liste_ports(r.ports_ouverts) + "), mais aucun "
                       "ne répond comme le débogage sans fil. Vérifie qu'il est bien activé sur le téléphone.";
        else
            r.erreur = "Plusieurs ports adb trouvés sur " + ip + " (" + liste_ports(confirmes) + ") : "
                       "impossible de savoir lequel utiliser. Recopie le port affiché dans "
                       "Options pour les développeurs > Débogage sans fil.";
        return r;
    }
}
