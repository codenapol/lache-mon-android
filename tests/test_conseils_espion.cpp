// Lache mon Android ! - Tests des conseils avant de supprimer un logiciel espion (conseils_espion.cpp)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lancer : make test

#include "../conseils_espion.h"

#include <iostream>
#include <string>

namespace
{
    int tests = 0, echecs = 0;

    void verifier(bool condition, const std::string& description)
    {
        ++tests;
        if (!condition)
        {
            ++echecs;
            std::cerr << "ÉCHEC : " << description << std::endl;
        }
    }

    template <typename T>
    void egal(const T& obtenu, const T& attendu, const std::string& description)
    {
        ++tests;
        if (!(obtenu == attendu))
        {
            ++echecs;
            std::cerr << "ÉCHEC : " << description << "\n  obtenu  : " << obtenu
                      << "\n  attendu : " << attendu << std::endl;
        }
    }

    bool contient(const std::string& texte, const std::string& motif)
    {
        return texte.find(motif) != std::string::npos;
    }

    Detection detection(const std::string& paquet, const std::string& menace, const std::string& source)
    {
        Detection d;
        d.paquet = paquet;
        d.menace = menace;
        d.source = source;
        d.preuve = "nom du paquet";
        return d;
    }

    // Sources réelles (sources.tsv)
    const std::string STALKERWARE = "Stalkerware Indicators of Compromise";
    const std::string PEGASUS     = "NSO Group Pegasus Indicators of Compromise";

    bool a_le_lien(const AvertissementEspion& a, const std::string& url)
    {
        for (const LienAide& l : a.liens)
            if (l.url == url)
                return true;
        return false;
    }

    void tests_detection_de()
    {
        RapportSecurite r;
        verifier(espion::detection_de(r, "com.exemple") == nullptr, "rapport vide");

        r.detections.push_back(detection("com.espion.un", "TheTruthSpy", STALKERWARE));
        r.detections.push_back(detection("com.espion.deux", "Pegasus", PEGASUS));
        AppSensible sensible;
        sensible.paquet = "com.gestionnaire.motsdepasse";
        sensible.accessibilite = true;
        r.a_surveiller.push_back(sensible);

        const Detection* d = espion::detection_de(r, "com.espion.deux");
        verifier(d != nullptr && d->menace == "Pegasus", "paquet détecté retrouvé");
        verifier(espion::detection_de(r, "com.whatsapp") == nullptr, "paquet ordinaire");
        verifier(espion::detection_de(r, "com.gestionnaire.motsdepasse") == nullptr,
                 "accès sensibles seulement : pas un logiciel espion");
        verifier(espion::detection_de(r, "") == nullptr, "paquet vide");
        verifier(espion::detection_de(r, "com.espion") == nullptr, "début de nom seulement");
    }

    void tests_est_stalkerware()
    {
        verifier(espion::est_stalkerware(detection("p", "m", STALKERWARE)), "collection Echap");
        verifier(espion::est_stalkerware(detection("p", "m", "AssoEchap_stalkerware-indicators_generated_stalkerware")),
                 "nom du fichier Echap (sources.tsv absent)");
        verifier(espion::est_stalkerware(detection("p", "m", "STALKERWARE")), "majuscules");
        verifier(!espion::est_stalkerware(detection("p", "m", PEGASUS)), "Pegasus");
        verifier(!espion::est_stalkerware(detection("p", "m", "Predator Spyware Indicators of Compromise")),
                 "Predator");
        verifier(!espion::est_stalkerware(detection("p", "m", "")), "source vide");
    }

    void tests_avertissement_stalkerware()
    {
        const Detection d = detection("com.espion.un", "TheTruthSpy", STALKERWARE);

        const AvertissementEspion des = espion::avertissement(d, true);
        egal(des.titre, std::string("Désinstaller TheTruthSpy ?"), "titre désinstallation");
        egal(des.bouton, std::string("Désinstaller quand même"), "bouton désinstallation");
        verifier(contient(des.texte, "prévient"), "explique que l'auteur peut être prévenu");
        verifier(contient(des.texte, "preuves"), "désinstallation : preuves effacées");
        verifier(contient(des.texte, "pas depuis le téléphone surveillé"), "conseil d'appareil sûr");
        verifier(a_le_lien(des, "https://echap.eu.org/"), "lien Echap");
        verifier(a_le_lien(des, "https://stopstalkerware.org/fr/"), "lien Coalition");
        verifier(!a_le_lien(des, "https://www.accessnow.org/help/"), "pas de lien Access Now");

        const AvertissementEspion dsa = espion::avertissement(d, false);
        egal(dsa.titre, std::string("Désactiver TheTruthSpy ?"), "titre désactivation");
        egal(dsa.bouton, std::string("Désactiver quand même"), "bouton désactivation");
        verifier(contient(dsa.texte, "prévient"), "désactivation : l'auteur peut aussi être prévenu");
        verifier(!contient(dsa.texte, "preuves"), "désactivation : pas de preuves effacées");
    }

    void tests_avertissement_etatique()
    {
        const AvertissementEspion a = espion::avertissement(detection("com.espion.deux", "Pegasus", PEGASUS), true);
        egal(a.titre, std::string("Désinstaller Pegasus ?"), "titre");
        verifier(contient(a.texte, "alerter"), "explique le risque d'alerter");
        verifier(contient(a.texte, "Amnesty Tech"), "mentionne Amnesty Tech");
        verifier(a_le_lien(a, "https://www.accessnow.org/help/"), "lien Access Now");
        verifier(!a_le_lien(a, "https://echap.eu.org/"), "pas de lien Echap");
        verifier(contient(a.texte, "preuves"), "désinstallation : preuves effacées");
    }

    void tests_liens_surs()
    {
        for (const std::string& source : {STALKERWARE, PEGASUS})
            for (bool desinstaller : {false, true})
            {
                const AvertissementEspion a = espion::avertissement(detection("p", "m", source), desinstaller);
                verifier(!a.liens.empty(), "au moins un lien (" + source + ")");
                for (const LienAide& l : a.liens)
                {
                    verifier(l.url.rfind("https://", 0) == 0, "lien en https : " + l.url);
                    verifier(!l.nom.empty(), "lien avec un nom : " + l.url);
                }
            }
    }
}

int main()
{
    tests_detection_de();
    tests_est_stalkerware();
    tests_avertissement_stalkerware();
    tests_avertissement_etatique();
    tests_liens_surs();

    std::cout << (tests - echecs) << "/" << tests << " tests réussis" << std::endl;
    return echecs == 0 ? 0 : 1;
}
