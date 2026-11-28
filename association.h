// Lache mon Android ! - Association du PC au téléphone (adb pair)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Avant la première connexion sans fil, le PC doit être associé au téléphone : sur le téléphone,
// « Débogage sans fil > Associer l'appareil avec un code » affiche un port d'association
// (différent du port de connexion) et un code à 6 chiffres, puis « adb pair IP:PORT CODE ».
//
// Le texte des erreurs d'adb n'est pas fiable (adb 34 et 37 répondent « protocol fault » là où
// le code source prévoit « Failed: ... ») : seul le succès est reconnu avec certitude.

#include <string>

struct ResultatAssociation
{
    bool reussi = false;
    std::string erreur;  // explication à afficher si !reussi
    std::string detail;  // sortie brute d'adb (pour le diagnostic), peut être vide
};

namespace association
{
    // Exactement 6 chiffres ASCII (protège aussi la commande shell)
    bool code_valide(const std::string& code);

    // Lit la sortie de « adb pair » et son code de retour (fonction pure, testable sans adb)
    ResultatAssociation analyser(const std::string& sortie, int code_retour);

    // Associe le PC au téléphone `ip` (port d'association `port`, code à 6 chiffres).
    // Les entrées sont vérifiées avant toute commande. Bloquant (30 s au plus) : à lancer dans un thread.
    ResultatAssociation associer(const std::string& ip, int port, const std::string& code);

    // Sortie de « adb connect » en échec : le téléphone a répondu mais refuse la connexion,
    // ce PC n'est donc peut-être pas encore associé (faux pour les erreurs réseau)
    bool cause_possible(const std::string& sortie_connect);
}
