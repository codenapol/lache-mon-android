// Lache mon Android ! - Conseils avant de supprimer un logiciel espion détecté
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "conseils_espion.h"

#include <algorithm>
#include <cctype>

namespace
{
    // Les mêmes adresses que dans le README
    const LienAide ECHAP     = {"Echap (France)", "https://echap.eu.org/"};
    const LienAide COALITION = {"Coalition Against Stalkerware", "https://stopstalkerware.org/fr/"};
    const LienAide ACCESS_NOW = {"Access Now Digital Security Helpline", "https://www.accessnow.org/help/"};

    const char* const APPAREIL_SUR =
        "Ouvre ces sites depuis ce PC ou un appareil sûr, pas depuis le téléphone surveillé.";

    std::string minuscules_ascii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }
}

namespace espion
{
    const Detection* detection_de(const RapportSecurite& r, const std::string& paquet)
    {
        if (paquet.empty())
            return nullptr;
        for (const Detection& d : r.detections)
            if (d.paquet == paquet)
                return &d;
        return nullptr;
    }

    bool est_stalkerware(const Detection& d)
    {
        // « Stalkerware Indicators of Compromise », ou le nom du fichier
        // « AssoEchap_stalkerware-indicators_... » si la liste des collections manque
        return minuscules_ascii(d.source).find("stalkerware") != std::string::npos;
    }

    AvertissementEspion avertissement(const Detection& d, bool desinstaller)
    {
        AvertissementEspion a;
        a.titre  = std::string(desinstaller ? "Désinstaller " : "Désactiver ") + d.menace + " ?";
        a.bouton = desinstaller ? "Désinstaller quand même" : "Désactiver quand même";

        const std::string preuves = desinstaller
            ? " La désinstallation efface aussi des preuves utiles : génère un rapport avant."
            : "";
        if (est_stalkerware(d))
        {
            a.texte = "Sa suppression prévient souvent la personne qui l'a installé, ce qui peut te "
                      "mettre en danger. Avant d'y toucher, contacte une association spécialisée pour "
                      "préparer ta sécurité." + preuves;
            a.liens = {ECHAP, COALITION};
        }
        else
        {
            a.texte = "Ce logiciel espion est utilisé contre des cibles précises (journalistes, "
                      "militants, avocats...). Le supprimer peut alerter ceux qui l'ont installé. "
                      "Fais d'abord analyser le téléphone par des spécialistes (Access Now ou "
                      "Amnesty Tech)." + preuves;
            a.liens = {ACCESS_NOW};
        }
        a.texte += std::string(" ") + APPAREIL_SUR;
        return a;
    }
}
