// Lache mon Android ! - Analyse du téléphone (modèle, applis, sécurité)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "analyse.h"

#include <algorithm>
#include <iostream>

#include "alternatives.h"   // CLIENTS_FDROID
#include "catalogue_apps.h" // listes d'applis connues, nom_de, alternative_de
#include "outils.h"         // executer, nettoyer, suivi_commandes_*
#include "uad.h"            // liste de debloat UAD-ng

// Premier client F-Droid installé et activé parmi `actifs`, ou "" s'il n'y en a pas
std::string client_fdroid(const std::set<std::string>& actifs)
{
    for (const std::string& client : CLIENTS_FDROID)
        if (actifs.count(client))
            return client;
    return "";
}

namespace
{
// Nom lisible tiré de la 1re ligne de la description UAD-ng ("Files by Google (https://...)"),
// seulement si elle ressemble à un nom et pas à une phrase
std::string nom_depuis_description(const std::string& description)
{
    std::string ligne = nettoyer(description.substr(0, description.find('\n')));
    if (size_t lien = ligne.find(" (http"); lien != std::string::npos)
        ligne = nettoyer(ligne.substr(0, lien));
    if (ligne.empty() || ligne.size() > 40 || ligne.find(". ") != std::string::npos ||
        ligne.back() == '.' || ligne.back() == ':' || ligne.back() == ',')
        return "";
    return ligne;
}
}

// Construit une AppTrouvee : nom de nos listes, sinon de UAD-ng, sinon le paquet ;
// description et dépendances de UAD-ng si le paquet y figure
AppTrouvee creer_app(const std::string& paquet, std::string nom)
{
    AppTrouvee app{nom.empty() ? nom_de(paquet) : nom, paquet, alternative_de(paquet), "", ""};
    if (const auto entree = uad::chercher(paquet))
    {
        app.description = entree->description;
        for (const std::string& p : entree->requis_par)
            app.requis_par += (app.requis_par.empty() ? "" : ", ") + p;
        if (app.nom == paquet)
            if (const std::string n = nom_depuis_description(entree->description); !n.empty())
                app.nom = n;
    }
    return app;
}

// Lance une commande "pm list packages ..." et renvoie l'ensemble des noms de paquets
std::set<std::string> lister_paquets(const std::string& commande)
{
    std::set<std::string> paquets;
    std::string liste = executer(commande);
    size_t pos = 0;
    while (pos < liste.size())
    {
        size_t fin = liste.find('\n', pos);
        if (fin == std::string::npos) fin = liste.size();
        std::string ligne = nettoyer(liste.substr(pos, fin - pos));
        if (ligne.rfind("package:", 0) == 0)
            paquets.insert(ligne.substr(8));
        pos = fin + 1;
    }
    return paquets;
}

namespace
{
// Range une liste : applis connues (nom lisible) d'abord, puis par ordre alphabétique
void trier(std::vector<AppTrouvee>& apps)
{
    std::sort(apps.begin(), apps.end(), [](const AppTrouvee& a, const AppTrouvee& b)
    {
        const bool a_connue = a.nom != a.paquet, b_connue = b.nom != b.paquet;
        if (a_connue != b_connue) return a_connue;
        return a.nom < b.nom;
    });
}
}

namespace
{
// Contrôle final de l'analyse : le téléphone doit encore répondre (adb get-state) et chaque
// commande doit avoir renvoyé son marqueur de fin. Sinon, tout le résultat est marqué incomplet.
void controle_final(const std::string& adb, InfosAppareil& infos)
{
    const size_t interrompues = suivi_commandes_incompletes();
    const std::string etat = nettoyer(executer(adb + "get-state"));
    if (etat != "device")
        infos.raison_incomplet = "le téléphone a disparu pendant l'analyse";
    else if (interrompues > 0)
        infos.raison_incomplet = std::to_string(interrompues) +
            (interrompues > 1 ? " commandes ne sont pas allées jusqu'au bout"
                              : " commande n'est pas allée jusqu'au bout");
    infos.incomplet = !infos.raison_incomplet.empty();
    if (infos.incomplet)
        std::clog << "[CONTROLE] Analyse incomplète : " << infos.raison_incomplet
                  << " (get-state : " << etat << ")" << std::endl;
}
}

// Récupère le modèle de l'appareil et repère les bloatwares, applis problématiques,
// applis désactivées et applis désinstallées (pour l'utilisateur)
InfosAppareil detection_info(const std::string& cible)
{
    InfosAppareil infos;
    const std::string adb = "adb -s " + cible + " ";

    // L'appareil doit être autorisé (popup "Autoriser le débogage" sur le téléphone)
    std::string etat = nettoyer(executer(adb + "get-state"));
    if (etat != "device")
    {
        infos.erreur = etat.find("unauthorized") != std::string::npos
            ? "Autorise le débogage sur le téléphone, puis réessaie."
            : "Appareil non disponible : " + etat;
        return infos;
    }
    suivi_commandes_remettre_a_zero(); // seules les commandes de cette analyse comptent

    infos.fabricant       = nettoyer(executer(adb + "shell getprop ro.product.manufacturer"));
    infos.modele          = nettoyer(executer(adb + "shell getprop ro.product.model"));
    infos.version_android = nettoyer(executer(adb + "shell getprop ro.build.version.release"));

    // Nom commercial : propriétés constructeur, sinon nom de l'appareil dans les paramètres
    for (const char* prop : {"ro.product.marketname", "ro.product.vendor.marketname", "ro.config.marketing_name"})
    {
        infos.nom_commercial = nettoyer(executer(adb + "shell getprop " + prop));
        if (!infos.nom_commercial.empty()) break;
    }
    if (infos.nom_commercial.empty())
        infos.nom_commercial = nettoyer(executer(adb + "shell settings get global device_name"));
    if (infos.nom_commercial == "null")
        infos.nom_commercial.clear();

    std::set<std::string> installes = lister_paquets(adb + "shell pm list packages");     // installés
    std::set<std::string> actifs    = lister_paquets(adb + "shell pm list packages -e");  // installés + activés
    std::set<std::string> desactives= lister_paquets(adb + "shell pm list packages -d");  // installés + désactivés
    std::set<std::string> tous      = lister_paquets(adb + "shell pm list packages -u");  // + désinstallés
    infos.actifs        = actifs;
    infos.client_fdroid = client_fdroid(actifs);

    // Bloatwares et applis problématiques : seulement celles encore actives
    std::set<std::string> deja_classees;
    for (const AppConnue& app : LISTE_BLOATWARES)
        if (actifs.count(app.paquet) && deja_classees.insert(app.paquet).second)
            infos.bloatwares.push_back(creer_app(app.paquet, app.nom));

    for (const AppConnue& app : LISTE_PROBLEMATIQUES)
        if (actifs.count(app.paquet) && deja_classees.insert(app.paquet).second)
            infos.problematiques.push_back(creer_app(app.paquet, app.nom));

    // Autres applis actives connues de UAD-ng, rangées selon le risque de leur suppression
    // (le niveau "Unsafe" n'est jamais proposé : il peut empêcher le téléphone de démarrer)
    for (const std::string& p : actifs)
    {
        if (deja_classees.count(p))
            continue;
        const auto entree = uad::chercher(p);
        if (!entree)
            continue;
        switch (entree->niveau)
        {
            case NiveauUAD::RECOMMANDE: infos.bloatwares.push_back(creer_app(p)); break;
            case NiveauUAD::AVANCE:     infos.avancees.push_back(creer_app(p));   break;
            case NiveauUAD::EXPERT:     infos.expertes.push_back(creer_app(p));   break;
            default: break;
        }
    }

    // Applis désactivées (réactivables)
    for (const std::string& p : desactives)
        infos.desactivees.push_back(creer_app(p));

    // Applis désinstallées pour l'utilisateur : présentes dans -u mais plus installées
    for (const std::string& p : tous)
        if (!installes.count(p))
            infos.desinstallees.push_back(creer_app(p));

    trier(infos.bloatwares);
    trier(infos.avancees);
    trier(infos.expertes);
    trier(infos.desactivees);
    trier(infos.desinstallees);

    // Recherche de logiciels espions (indicateurs MVT + accès sensibles + réglages)
    infos.securite = securite::analyser(adb, installes);

    controle_final(adb, infos);

    std::clog << "[DETECTION] " << infos.fabricant << " " << infos.modele
              << " | " << infos.bloatwares.size() << " bloatwares, "
              << infos.problematiques.size() << " applis problématiques, "
              << infos.desactivees.size() << " désactivées, "
              << infos.desinstallees.size() << " désinstallées" << std::endl;
    return infos;
}
