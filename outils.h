// Lache mon Android ! - Fonctions partagées (shell, fichiers, téléchargement)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Petites fonctions partagées entre les modules

#include <filesystem>
#include <string>

// Exécute une commande shell et renvoie sa sortie (stdout + stderr)
std::string executer(const std::string& commande);

// Comme executer, mais garde les octets tels quels (fichiers binaires) et ignore stderr
std::string executer_binaire(const std::string& commande);

// Retire les espaces / retours à la ligne au début et à la fin
std::string nettoyer(const std::string& s);

std::string minuscules(std::string s);

// Un nom de paquet Android ne contient que lettres, chiffres, '.' et '_' (protège les commandes shell)
bool paquet_valide(const std::string& paquet);

// Entoure de guillemets simples pour le shell (gère les ' dans le texte)
std::string entre_quotes(const std::string& s);

// Dossier des données utilisateur ($XDG_DATA_HOME ou ~/.local/share)
std::filesystem::path dossier_donnees();

// Dossier de nos données (~/.local/share/lache_mon_android)
std::filesystem::path dossier_programme();

// Contenu complet d'un fichier ("" s'il n'existe pas)
std::string lire_fichier(const std::filesystem::path& chemin);

// Télécharge `url` dans `destination` avec curl (fichier temporaire puis renommage)
bool telecharger(const std::string& url, const std::filesystem::path& destination);
