// Lache mon Android ! - Commandes ADB de base (adb embarqué, connexion, commandes)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once

#include <string>

// Dossier de l'exécutable (avec "/" final), ou "" si inconnu. Dans une AppImage : .../usr/bin/
std::string dossier_executable();

// Si un adb est fourni à côté de l'exécutable (AppImage), il passe avant celui du système
void utiliser_adb_embarque();

// Vérifie que adb est installé et répond
bool ADB_VERIF();

// L'IP est insérée dans une commande shell : on n'accepte que des caractères sûrs
bool ip_valide(const std::string& ip);

// Connexion ADB sans fil. Renvoie true si connecté, sinon message d'erreur dans `erreur`.
bool connexion_adb(const std::string& cible, std::string& erreur);

// Lance une commande "am start ..." sur le téléphone. Renvoie "" si tout va bien, sinon la sortie d'erreur.
std::string commande_telephone(const std::string& cible, const std::string& commande_shell);
