// Lache mon Android ! - Détection de logiciels espions
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Détection de logiciels espions :
//  - comparaison avec les indicateurs de compromission (IOC) utilisés par MVT
//    (Amnesty International, Echap stalkerware-indicators, Citizen Lab, Google...) :
//    noms de paquets, empreintes SHA-256 des APK, certificats de signature des APK,
//    processus en cours, propriétés système, fichiers présents ;
//  - applis tierces disposant d'accès permettant d'espionner
//    (accessibilité, administrateur de l'appareil, lecture des notifications) ;
//  - réglages de sécurité affaiblis (Play Protect, root, bootloader).

#include <set>
#include <string>
#include <vector>

// Une appli installée correspond à un indicateur de compromission connu
struct Detection
{
    std::string paquet;
    std::string menace; // nom du logiciel espion (ex. "TheTruthSpy")
    std::string source; // collection d'indicateurs (ex. "Stalkerware Indicators of Compromise")
    std::string preuve; // ce qui a correspondu (nom du paquet, empreinte de l'APK, certificat)
};

// Trace d'un logiciel espion qui n'est pas une appli (processus, fichier, propriété système)
struct Trace
{
    std::string element; // nom du processus, chemin du fichier, nom de la propriété
    std::string menace;
    std::string source;
    std::string preuve;  // "processus en cours", "fichier présent", "propriété système"
};

// Appli tierce disposant d'accès sensibles : pas forcément malveillante, mais à vérifier
struct AppSensible
{
    std::string paquet;
    bool accessibilite   = false; // peut lire l'écran et agir à la place de l'utilisateur
    bool administrateur  = false; // administrateur de l'appareil (difficile à désinstaller)
    bool notifications   = false; // lit toutes les notifications (messages, codes...)
    bool hors_magasin    = false; // installée depuis un fichier APK ou par ADB
};

struct RapportSecurite
{
    std::vector<Detection>   detections;
    std::vector<Trace>       traces;
    std::vector<AppSensible> a_surveiller;
    std::vector<std::string> alertes;        // réglages affaiblis, root, bootloader...
    size_t indicateurs_verifies = 0;         // taille de la base au moment de l'analyse
};

namespace securite
{
    // (Re)charge la base d'indicateurs depuis le disque. Appelé au démarrage et après une mise à jour.
    // Lit notre dossier et, s'il existe, celui de MVT (~/.local/share/mvt/indicators).
    void charger_indicateurs();

    // Télécharge les indicateurs listés par le projet MVT (bloquant : à lancer dans un thread).
    // Renvoie true si au moins une collection a été téléchargée ; `message` décrit le résultat.
    bool mettre_a_jour(std::string& message);

    // Nombre d'indicateurs utilisables actuellement chargés
    size_t nombre_indicateurs();

    // Date de la dernière mise à jour (vide si jamais téléchargée)
    std::string date_mise_a_jour();

    // Analyse l'appareil. `adb` = "adb -s IP:PORT ", `installes` = paquets installés.
    RapportSecurite analyser(const std::string& adb, const std::set<std::string>& installes);
}
