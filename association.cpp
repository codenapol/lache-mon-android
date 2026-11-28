// Lache mon Android ! - Association du PC au téléphone (adb pair)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "association.h"

#include <iostream>

#include "decouverte.h" // ipv4_valide
#include "outils.h"     // executer_avec_marqueur, nettoyer

namespace
{
    // adb abandonne seul après 10 s si le port ne répond pas ; ce délai couvre le reste
    // (téléphone qui accepte la connexion puis ne répond plus)
    const int DELAI_PAIR_S = 30;
    const int CODE_DELAI_DEPASSE = 124; // code de retour de « timeout »

    const char* const SUCCES = "Successfully paired to"; // identique d'adb 30 à 37

    const char* const CONSEIL_FENETRE =
        "Sur le téléphone, garde la fenêtre « Associer l'appareil avec un code » ouverte : "
        "le port et le code changent à chaque ouverture.";
}

namespace association
{
    bool code_valide(const std::string& code)
    {
        if (code.size() != 6)
            return false;
        for (char c : code)
            if (c < '0' || c > '9')
                return false;
        return true;
    }

    ResultatAssociation analyser(const std::string& sortie, int code_retour)
    {
        ResultatAssociation r;
        r.detail = nettoyer(sortie);
        if (sortie.find(SUCCES) != std::string::npos)
        {
            r.reussi = true;
            return r;
        }

        if (code_retour == CODE_DELAI_DEPASSE)
            r.erreur = std::string("Le téléphone ne répond pas. ") + CONSEIL_FENETRE;
        else if (sortie.find("Wrong password") != std::string::npos)
            r.erreur = std::string("Code incorrect. ") + CONSEIL_FENETRE;
        else if (sortie.find("Unable to start pairing client") != std::string::npos)
            r.erreur = std::string("Le téléphone ne répond pas sur ce port. ") + CONSEIL_FENETRE;
        else // message inconnu ou « protocol fault » : on ne sait pas lequel des cas s'applique
            r.erreur = std::string("Association refusée : code incorrect, port d'association erroné "
                                   "ou fenêtre fermée sur le téléphone. ") + CONSEIL_FENETRE;
        return r;
    }

    ResultatAssociation associer(const std::string& ip, int port, const std::string& code)
    {
        ResultatAssociation r;
        if (!decouverte::ipv4_valide(ip))
            r.erreur = "« " + ip + " » n'est pas une adresse IP valide (exemple : 192.168.1.42).";
        else if (port < 1 || port > 65535)
            r.erreur = "Le port d'association doit être compris entre 1 et 65535.";
        else if (!code_valide(code))
            r.erreur = "Le code doit comporter exactement 6 chiffres.";
        if (!r.erreur.empty())
            return r;

        // Entrées vérifiées : chiffres et points seulement. Le code n'est jamais écrit dans le journal.
        const std::string adresse = ip + ":" + std::to_string(port);
        std::clog << "[ASSOCIATION] adb pair " << adresse << std::endl;
        const ResultatCommande c = executer_avec_marqueur(
            "timeout " + std::to_string(DELAI_PAIR_S) + " adb pair " + adresse + " " + code);
        r = analyser(c.sortie, c.marqueur_recu ? c.code_retour : -1);
        std::clog << "[ASSOCIATION] " << (r.reussi ? "réussie" : "échec : " + r.detail) << std::endl;
        return r;
    }

    bool cause_possible(const std::string& sortie_connect)
    {
        if (sortie_connect.find("failed to authenticate to") != std::string::npos)
            return true;
        // « failed to connect to 'IP:PORT': Connection refused » = erreur réseau (avec la cause) ;
        // « failed to connect to IP:PORT » seul = le port a répondu, mais pas comme un appareil autorisé
        const std::string motif = "failed to connect to ";
        const size_t pos = sortie_connect.find(motif);
        return pos != std::string::npos &&
               sortie_connect.compare(pos + motif.size(), 1, "'") != 0;
    }
}
