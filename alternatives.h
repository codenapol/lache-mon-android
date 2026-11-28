// Lache mon Android ! - Alternatives libres (fenêtre « Remplacer »)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Catalogue des alternatives libres, avec leur dernière version et leur logo.
// Le programme n'installe rien lui-même : il ouvre la fiche de l'appli dans F-Droid sur le téléphone
// (ou son site officiel si elle n'est pas sur F-Droid). L'installation, la vérification et les
// mises à jour restent à la charge de F-Droid ou de l'éditeur.

#include <string>
#include <vector>

struct Alternative
{
    const char* nom;         // doit correspondre au nom utilisé dans LISTE_ALTERNATIVES (catalogue_apps.cpp)
    const char* paquet;      // paquet Android de l'alternative
    const char* description;

    // Applis absentes de F-Droid uniquement (nullptr sinon)
    const char* page_officielle; // ouverte sur le téléphone pour télécharger l'appli
    const char* url_logo;        // image, dans n'importe quel format courant
    const char* url_version;     // JSON donnant la dernière version (objet, ou tableau : 1er élément)
    const char* champ_version;   // champ de ce JSON qui contient la version
};

// Toutes les alternatives proposées
extern const std::vector<Alternative> CATALOGUE_ALTERNATIVES;

// Fiche d'une alternative à partir de son nom, ou nullptr
const Alternative* fiche_alternative(const std::string& nom);

// Paquets des clients F-Droid connus (F-Droid, F-Droid Basic, Droid-ify, Neo Store)
extern const std::vector<std::string> CLIENTS_FDROID;

// Téléchargement de F-Droid, ouvert dans le navigateur du téléphone
extern const char* const URL_APK_FDROID;

struct InfosAlternative
{
    bool trouve = false;              // false : version introuvable (hors ligne et pas de cache)
    std::string version;              // "0.29.3"
    std::vector<unsigned char> icone; // pixels RGBA, 4 octets par pixel (vide si pas de logo)
    int largeur = 0, hauteur = 0;
};

namespace alternatives
{
    // Dernière version et logo (bloquant : à lancer dans un thread).
    // Les réponses sont gardées dans ~/.local/share/lache_mon_android/alternatives/ pour le mode hors ligne.
    InfosAlternative chercher(const Alternative& alt);
}
