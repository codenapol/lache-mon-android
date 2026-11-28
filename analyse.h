// Lache mon Android ! - Analyse du téléphone (modèle, applis, sécurité)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once

#include <set>
#include <string>
#include <vector>

#include "securite.h" // RapportSecurite

struct AppTrouvee
{
    std::string nom;
    std::string paquet;
    std::string alternative; // vide = pas de bouton "Remplacer"
    std::string description; // description UAD-ng (en anglais), vide si inconnue
    std::string requis_par;  // paquets qui en dépendent (UAD-ng), séparés par des virgules
};

struct InfosAppareil
{
    std::string fabricant;
    std::string modele;
    std::string version_android;
    std::string nom_commercial; // nom affiché dans les paramètres (ex. "Galaxy S26")
    std::vector<AppTrouvee> bloatwares;     // nos listes + UAD-ng "Recommended"
    std::vector<AppTrouvee> avancees;       // UAD-ng "Advanced"
    std::vector<AppTrouvee> expertes;       // UAD-ng "Expert"
    std::vector<AppTrouvee> problematiques;
    std::vector<AppTrouvee> desactivees;
    std::vector<AppTrouvee> desinstallees;
    std::set<std::string> actifs; // paquets installés et activés (fenêtre « Remplacer »)
    std::string client_fdroid;    // client F-Droid installé ("" si aucun)
    RapportSecurite securite; // logiciels espions, accès sensibles, réglages affaiblis
    std::string erreur; // vide si tout s'est bien passé
    bool incomplet = false;       // téléphone disparu ou commande interrompue pendant l'analyse
    std::string raison_incomplet; // pourquoi le résultat est incomplet
};

// Premier client F-Droid installé et activé parmi `actifs`, ou "" s'il n'y en a pas
std::string client_fdroid(const std::set<std::string>& actifs);

// Lance une commande "pm list packages ..." et renvoie l'ensemble des noms de paquets
std::set<std::string> lister_paquets(const std::string& commande);

// Construit une AppTrouvee : nom de nos listes, sinon de UAD-ng, sinon le paquet ;
// description et dépendances de UAD-ng si le paquet y figure
AppTrouvee creer_app(const std::string& paquet, std::string nom = "");

// Récupère le modèle de l'appareil et repère les bloatwares, applis problématiques,
// applis désactivées et applis désinstallées (pour l'utilisateur)
InfosAppareil detection_info(const std::string& cible);
