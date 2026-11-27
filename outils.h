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

// Résultat détaillé d'une commande. Un marqueur de fin (avec le code de retour) est ajouté
// à chaque commande : s'il n'arrive pas, la commande n'est pas allée jusqu'au bout.
struct ResultatCommande
{
    std::string sortie;          // sortie de la commande, sans le marqueur
    bool marqueur_recu = false;  // le shell a atteint la fin de la commande
    int  code_retour   = -1;     // code de retour (valable si marqueur_recu)
    bool coupure_adb   = false;  // adb signale que l'appareil a disparu (offline, introuvable...)

    bool complete() const { return marqueur_recu && !coupure_adb; }
};

// Versions détaillées de executer et executer_binaire (utilisées par elles)
ResultatCommande executer_avec_marqueur(const std::string& commande);
ResultatCommande executer_binaire_avec_marqueur(const std::string& commande);

// Suivi des commandes incomplètes du thread courant (une analyse tourne dans un seul thread) :
// remise à zéro au début de l'analyse, lecture à la fin.
void   suivi_commandes_remettre_a_zero();
size_t suivi_commandes_incompletes();

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
