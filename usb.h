// Lache mon Android ! - Téléphones branchés en USB
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Liste des téléphones vus par adb en USB (« adb devices -l »), sans ceux connectés sans fil.
// Le numéro de série d'un appareil sert ensuite de cible : « adb -s <numéro> ... ».

#include <string>
#include <vector>

struct AppareilUsb
{
    std::string serie;     // numéro de série (cible pour « adb -s »)
    std::string etat;      // device, unauthorized, offline, no permissions, ...
    std::string modele;    // « Pixel 7 » (champ model:, « _ » remplacés par des espaces), peut être vide
    bool emulateur = false; // emulator-5554 : utile pour tester sans téléphone

    // Prêt à être analysé : autorisé et numéro de série sans caractère dangereux pour le shell
    bool connectable() const;
    // Nom à afficher : modèle, sinon « Émulateur » ou « Téléphone »
    std::string nom() const;
};

struct ListeUsb
{
    std::vector<AppareilUsb> appareils;
    std::string erreur;    // adb n'a pas répondu correctement (vide sinon)
};

namespace usb
{
    // Numéro de série utilisable dans une commande shell : lettres, chiffres, '.', '-', '_'
    bool serie_valide(const std::string& serie);

    // Appareil connecté sans fil (« IP:port » ou annonce mDNS « adb-xxx._adb-tls-connect._tcp »)
    bool est_sans_fil(const std::string& serie);

    // Lit la sortie de « adb devices -l » (fonction pure, testable sans adb)
    ListeUsb analyser(const std::string& sortie);

    // Lance « adb devices -l » et renvoie les appareils USB (bloquant : à lancer dans un thread)
    ListeUsb lister();

    // Explication à afficher pour un appareil qui ne peut pas être connecté (vide s'il est prêt)
    std::string conseil(const AppareilUsb& appareil);

    // État en quelques mots pour la liste (« Prêt », « Non autorisé », ...)
    std::string etat_court(const AppareilUsb& appareil);
}
