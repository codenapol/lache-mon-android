// Lache mon Android ! - Rapport d'analyse du téléphone
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once

#include <string>

#include "analyse.h" // InfosAppareil
#include "rapport.h" // Rapport

// Date et heure locales au format « 27/11/2026 à 10:52 » (`pour_fichier` : « 2026-11-27_10h52 »)
std::string date_heure(bool pour_fichier);

// Nom du fichier de rapport : « Rapport_Ulefone_Armor_22_2026-11-27_10h52 » (caractères sûrs seulement)
std::string nom_fichier_rapport(const InfosAppareil& infos);

// Rapport basique : résumé de l'analyse, suivi de ses limites.
// L'adresse IP n'y figure pas : le rapport peut être transmis à quelqu'un d'autre.
Rapport rapport_basique(const InfosAppareil& infos);
