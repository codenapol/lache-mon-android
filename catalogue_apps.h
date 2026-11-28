// Lache mon Android ! - Applications connues (bloatwares, problématiques, alternatives)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once

#include <string>
#include <vector>

struct AppConnue
{
    const char* paquet;
    const char* nom;
};

// Applications préinstallées inutiles les plus courantes (Google, constructeurs, partenaires)
extern const std::vector<AppConnue> LISTE_BLOATWARES;
// Applications problématiques (vie privée, pistage, publicité)
extern const std::vector<AppConnue> LISTE_PROBLEMATIQUES;

// Renvoie l'alternative d'un paquet, ou "" s'il n'y en a pas (ou si elle n'a pas de fiche)
std::string alternative_de(const std::string& paquet);

// Nom lisible d'un paquet (depuis nos listes), sinon le nom du paquet lui-même
std::string nom_de(const std::string& paquet);
