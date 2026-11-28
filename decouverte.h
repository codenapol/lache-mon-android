// Lache mon Android ! - Découverte automatique du port du débogage sans fil
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Le débogage sans fil d'Android choisit un port au hasard à chaque activation.
// On le retrouve à partir de l'adresse IP :
//  1. annonce mDNS du téléphone (« adb mdns services ») : rapide et exact ;
//  2. sinon, scan TCP des ports où Android peut le placer, puis vérification que le port
//     ouvert répond bien comme adb (message CNXN du protocole adb, sans rien changer
//     sur le téléphone).

#include <string>
#include <vector>

struct ResultatDecouverte
{
    int port = 0;                    // 0 si aucun port n'a pu être identifié
    std::string erreur;              // explication à afficher si port == 0
    std::vector<int> ports_ouverts;  // ports ouverts trouvés par le scan (pour le message d'erreur)
};

// Un service annoncé en mDNS, lu dans « adb mdns services »
struct ServiceMdns
{
    std::string instance; // adb-XXXX-yyyy
    std::string type;     // _adb-tls-connect._tcp, _adb-tls-pairing._tcp ou _adb._tcp
    std::string ip;       // IPv4 valide
    int port = 0;         // 1 à 65535
};

namespace decouverte
{
    // Adresse IPv4 écrite en entier (ex. 192.168.1.42)
    bool ipv4_valide(const std::string& ip);

    // Lit la sortie de « adb mdns services » (fonction pure, testable sans adb).
    // Les lignes mal formées (IP invalide, port non numérique ou hors limites) sont ignorées.
    std::vector<ServiceMdns> analyser_mdns(const std::string& sortie);

    // Port d'association (« Associer l'appareil avec un code ») annoncé en mDNS par `ip`.
    // 0 si aucun, ou si plusieurs ports sont annoncés (on ne devine pas). Bloquant.
    int port_association(const std::string& ip);

    // Cherche le port du débogage sans fil sur `ip` (bloquant : à lancer dans un thread)
    ResultatDecouverte trouver_port(const std::string& ip);
}
