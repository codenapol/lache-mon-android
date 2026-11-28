// Lache mon Android ! - Conseils avant de supprimer un logiciel espion détecté
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Désactiver ou désinstaller un logiciel espion prévient souvent la personne qui l'a installé,
// et la désinstallation efface des preuves. Avant d'y toucher, on explique qui contacter.
// Fonctions pures (aucune commande, aucun affichage) : testables sans téléphone.

#include <string>
#include <vector>

#include "securite.h" // Detection, RapportSecurite

struct LienAide
{
    std::string nom;
    std::string url;
};

struct AvertissementEspion
{
    std::string titre;           // « Désinstaller TheTruthSpy ? »
    std::string texte;           // pourquoi attendre, quoi faire
    std::vector<LienAide> liens; // organisations à contacter
    std::string bouton;          // « Désinstaller quand même » ou « Désactiver quand même »
};

namespace espion
{
    // Détection correspondant à `paquet` dans le rapport, ou nullptr si ce n'est pas un logiciel espion détecté
    const Detection* detection_de(const RapportSecurite& r, const std::string& paquet);

    // La détection vient des indicateurs de stalkerwares (Echap), et non d'un logiciel espion
    // étatique ou mercenaire (Pegasus, Predator...). Fondé sur le nom de la collection.
    bool est_stalkerware(const Detection& d);

    // Message à afficher avant de désactiver (`desinstaller` = false) ou de désinstaller `d`
    AvertissementEspion avertissement(const Detection& d, bool desinstaller);
}
