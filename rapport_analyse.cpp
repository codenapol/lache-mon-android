// Lache mon Android ! - Rapport d'analyse du téléphone
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "rapport_analyse.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iterator>

#include "catalogue_apps.h" // nom_de
#include "securite.h"       // RapportSecurite, date_mise_a_jour
#include "uad.h"            // uad::nombre

// Date et heure locales au format « 27/11/2026 à 10:52 » (`pour_fichier` : « 2026-11-27_10h52 »)
std::string date_heure(bool pour_fichier)
{
    const std::time_t maintenant = std::time(nullptr);
    std::tm local{};
    localtime_r(&maintenant, &local);
    char tampon[32];
    std::strftime(tampon, sizeof(tampon), pour_fichier ? "%Y-%m-%d_%Hh%M" : "%d/%m/%Y à %H:%M", &local);
    return tampon;
}

// Nom du fichier de rapport : « Rapport_Ulefone_Armor_22_2026-11-27_10h52 » (caractères sûrs seulement)
std::string nom_fichier_rapport(const InfosAppareil& infos)
{
    std::string nom = "Rapport_" + infos.fabricant + "_" + infos.modele + "_" + date_heure(true);
    for (char& c : nom)
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_'))
            c = '_';
    return nom;
}

namespace
{
// « Nom lisible (paquet) », ou seulement le paquet si l'appli n'a pas de nom connu
std::string nom_et_paquet(const std::string& paquet)
{
    const std::string nom = nom_de(paquet);
    return nom == paquet ? paquet : nom + " (" + paquet + ")";
}
}

namespace
{
// Section « Réglages de sécurité » du rapport : chaque point contrôlé par securite::analyser,
// avec son résultat. Un point est en défaut quand son texte d'alerte (securite.cpp) est présent.
void ajouter_reglages_securite(Rapport& r, const RapportSecurite& s)
{
    struct Point
    {
        const char* nom;     // ce qui est contrôlé
        const char* role;    // à quoi il sert
        const char* alerte;  // texte exact de l'alerte dans securite.cpp
        const char* bon;     // résultat quand tout va bien
        const char* mauvais; // résultat en défaut
    };
    const Point points[] = {
        {"Google Play Protect", "analyse les applis installées à la recherche de logiciels malveillants",
         "Google Play Protect est désactivé.", "activé", "désactivé"},
        {"Analyse des applis inconnues", "Play Protect envoie à Google les applis inconnues pour les examiner",
         "Play Protect n'analyse plus les applis inconnues.", "activée", "désactivée"},
        {"Vérification des installations par ADB", "contrôle les applis installées depuis un ordinateur",
         "Les applis installées par ADB ne sont plus vérifiées.", "activée", "désactivée"},
        {"Accès root", "un accès root (su, Magisk) permet à une appli de contourner toutes les protections",
         "Accès root détecté (su ou Magisk présent).", "non détecté", "détecté"},
        {"Bootloader", "un bootloader déverrouillé permet de modifier le système",
         "Bootloader déverrouillé : le système a pu être modifié.", "aucun déverrouillage détecté", "déverrouillé"},
    };

    auto en_defaut = [&](const char* alerte)
    { return std::find(s.alertes.begin(), s.alertes.end(), alerte) != s.alertes.end(); };

    size_t defauts = 0;
    for (const Point& p : points)
        defauts += en_defaut(p.alerte);
    if (defauts == 0)
        r.ajouter(TypeBloc::TEXTE, "Aucun réglage affaibli détecté. Les " + std::to_string(std::size(points)) +
                                   " points suivants ont été contrôlés :");
    else
        r.ajouter(TypeBloc::ALERTE, std::to_string(defauts) + " réglage(s) affaibli(s) sur " +
                                    std::to_string(std::size(points)) + " points contrôlés :");

    for (const Point& p : points)
    {
        const bool defaut = en_defaut(p.alerte);
        r.ajouter(TypeBloc::PUCE, std::string(p.nom) + " : " + (defaut ? p.mauvais : p.bon) + (defaut ? " (à corriger)" : "") +
                                  ". Rôle : " + p.role + ".");
    }

    // Alerte éventuelle qui ne correspondrait à aucun point ci-dessus : jamais perdue
    for (const std::string& alerte : s.alertes)
        if (std::none_of(std::begin(points), std::end(points), [&](const Point& p) { return alerte == p.alerte; }))
            r.ajouter(TypeBloc::ALERTE, alerte);
}
}

// Rapport basique : résumé de l'analyse, suivi de ses limites.
// L'adresse IP n'y figure pas : le rapport peut être transmis à quelqu'un d'autre.
Rapport rapport_basique(const InfosAppareil& infos)
{
    const RapportSecurite& s = infos.securite;
    Rapport r;
    r.titre = "Rapport d'analyse " + infos.fabricant + " " + infos.modele;

    r.ajouter(TypeBloc::TITRE, "Rapport d'analyse du téléphone");
    r.ajouter(TypeBloc::DISCRET, "Généré le " + date_heure(false) + " par Lâche mon Android ! (rapport basique)");

    r.ajouter(TypeBloc::SECTION, "Appareil");
    r.ajouter(TypeBloc::PUCE, "Fabricant : " + infos.fabricant);
    r.ajouter(TypeBloc::PUCE, "Modèle : " + infos.modele);
    // Certains téléphones renvoient une valeur vide de sens (ex. ".") : on ne garde qu'un vrai nom
    if (std::any_of(infos.nom_commercial.begin(), infos.nom_commercial.end(),
                    [](char c) { return std::isalnum((unsigned char)c); }))
        r.ajouter(TypeBloc::PUCE, "Nom commercial : " + infos.nom_commercial);
    r.ajouter(TypeBloc::PUCE, "Version d'Android : " + infos.version_android);

    r.ajouter(TypeBloc::SECTION, "Déroulement de l'analyse");
    if (infos.incomplet)
        r.ajouter(TypeBloc::ALERTE, "Analyse incomplète : " + infos.raison_incomplet +
                                    ". Les résultats ci-dessous peuvent être faux ou partiels.");
    else
        r.ajouter(TypeBloc::OK, "Analyse complète : toutes les vérifications sont allées jusqu'au bout "
                                "et le téléphone répondait encore à la fin.");

    r.ajouter(TypeBloc::SECTION, "Logiciels espions");
    if (s.indicateurs_verifies == 0)
        r.ajouter(TypeBloc::ALERTE, "Base anti-espions absente : la recherche de logiciels espions connus n'a pas été faite.");
    else if (s.detections.empty() && s.traces.empty())
        r.ajouter(TypeBloc::TEXTE, "Aucun logiciel espion connu détecté.");
    else
    {
        r.ajouter(TypeBloc::DANGER, std::to_string(s.detections.size() + s.traces.size()) +
                                    " logiciel(s) espion(s) ou trace(s) détecté(s).");
        for (const Detection& d : s.detections)
            r.ajouter(TypeBloc::PUCE, d.menace + " : appli " + nom_et_paquet(d.paquet) + ", repérée par " +
                                      d.preuve + ". Source : " + d.source + ".");
        for (const Trace& t : s.traces)
            r.ajouter(TypeBloc::PUCE, t.menace + " : " + t.preuve + " « " + t.element + " ». Source : " + t.source + ".");
        if (!s.traces.empty())
            r.ajouter(TypeBloc::DISCRET, "Ces traces ne se suppriment pas depuis l'appli : fais analyser le téléphone "
                                         "(MVT, ligne d'aide Amnesty Tech ou Access Now).");
    }

    r.ajouter(TypeBloc::SECTION, "Accès sensibles à vérifier");
    if (s.a_surveiller.empty())
        r.ajouter(TypeBloc::TEXTE, "Aucune appli installée par l'utilisateur ne dispose d'un accès sensible.");
    else
    {
        r.ajouter(TypeBloc::DISCRET, "Pas forcément malveillantes : vérifie que chacune a été installée volontairement.");
        for (const AppSensible& a : s.a_surveiller)
        {
            std::string acces;
            auto ajouter = [&](bool actif, const char* texte)
            { if (actif) acces += (acces.empty() ? "" : ", ") + std::string(texte); };
            ajouter(a.accessibilite,  "accessibilité (lit l'écran, agit à la place de l'utilisateur)");
            ajouter(a.administrateur, "administrateur de l'appareil");
            ajouter(a.notifications,  "lecture de toutes les notifications");
            ajouter(a.hors_magasin,   "installée hors magasin d'applis");
            r.ajouter(TypeBloc::PUCE, nom_et_paquet(a.paquet) + " : " + acces + ".");
        }
    }

    r.ajouter(TypeBloc::SECTION, "Réglages de sécurité");
    ajouter_reglages_securite(r, s);

    r.ajouter(TypeBloc::SECTION, "Applications");
    r.ajouter(TypeBloc::PUCE, "Bloatwares actifs : " + std::to_string(infos.bloatwares.size()));
    r.ajouter(TypeBloc::PUCE, "Suppression avancée possible : " + std::to_string(infos.avancees.size()));
    r.ajouter(TypeBloc::PUCE, "Suppression experte possible : " + std::to_string(infos.expertes.size()));
    r.ajouter(TypeBloc::PUCE, "Applications problématiques : " + std::to_string(infos.problematiques.size()));
    r.ajouter(TypeBloc::PUCE, "Applications désactivées : " + std::to_string(infos.desactivees.size()));
    r.ajouter(TypeBloc::PUCE, "Applications désinstallées pour l'utilisateur : " + std::to_string(infos.desinstallees.size()));

    r.ajouter(TypeBloc::SECTION, "Bases utilisées");
    const std::string date_base = securite::date_mise_a_jour();
    r.ajouter(TypeBloc::PUCE, "Indicateurs de compromission (projet MVT) : " + std::to_string(s.indicateurs_verifies) +
                              (date_base.empty() ? "" : ", mis à jour le " + date_base));
    r.ajouter(TypeBloc::PUCE, "Liste de debloat UAD-ng : " + std::to_string(uad::nombre()) + " applis connues");

    r.ajouter(TypeBloc::SECTION, "Limites de l'analyse");
    r.ajouter(TypeBloc::PUCE, "Pas d'accès root : seules les informations lisibles par le débogage ADB sont examinées. "
                              "Les données privées des applis et une grande partie du système restent hors de portée.");
    r.ajouter(TypeBloc::PUCE, "Seuls les logiciels espions déjà connus sont détectés (indicateurs publiés par Amnesty "
                              "International, Echap, Citizen Lab... et rassemblés par MVT). Un logiciel espion récent "
                              "ou fait sur mesure peut passer inaperçu : « aucun logiciel espion connu détecté » ne "
                              "prouve pas que le téléphone est sain.");
    r.ajouter(TypeBloc::PUCE, "Faux positifs possibles dans les accès sensibles : des applis légitimes (gestionnaire de "
                              "mots de passe, lanceur, montre connectée, outil d'accessibilité) ont besoin de ces accès.");
    r.ajouter(TypeBloc::PUCE, "Noms de processus venant d'iOS écartés : beaucoup d'indicateurs de processus visent "
                              "l'iPhone. Quand un nom correspond à un programme du système Android (par exemple "
                              "gatekeeperd), il est écarté pour éviter une fausse alerte ; un logiciel espion qui "
                              "prendrait ce nom ne serait pas signalé.");
    r.ajouter(TypeBloc::PUCE, "Android tronque les noms de processus à 15 caractères : la comparaison porte alors "
                              "sur le début du nom.");
    r.ajouter(TypeBloc::PUCE, "Empreintes et certificats vérifiés seulement pour les applis installées par "
                              "l'utilisateur, pas pour les applis système.");
    r.ajouter(TypeBloc::PUCE, "Fichiers suspects recherchés seulement dans les dossiers lisibles sans root "
                              "(stockage partagé, /data/local/tmp).");
    r.ajouter(TypeBloc::PUCE, "Résultat valable au moment de l'analyse uniquement.");
    return r;
}
