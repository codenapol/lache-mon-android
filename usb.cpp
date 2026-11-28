// Lache mon Android ! - Téléphones branchés en USB
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "usb.h"
#include "outils.h"

#include <cctype>
#include <sstream>

namespace
{
    // États qu'adb peut afficher dans « adb devices » (le reste est ignoré : messages du serveur, erreurs)
    const char* const ETATS_CONNUS[] = {
        "device", "unauthorized", "offline", "no permissions", "authorizing", "connecting",
        "recovery", "sideload", "bootloader", "rescue", "host", "unknown",
    };

    bool etat_connu(const std::string& etat)
    {
        for (const char* e : ETATS_CONNUS)
            if (etat == e)
                return true;
        return false;
    }
}

bool AppareilUsb::connectable() const
{
    return etat == "device" && usb::serie_valide(serie);
}

std::string AppareilUsb::nom() const
{
    if (!modele.empty())
        return modele;
    return emulateur ? "Émulateur" : "Téléphone";
}

namespace usb
{
    bool serie_valide(const std::string& serie)
    {
        if (serie.empty() || serie.size() > 128 || serie[0] == '-') // « -x » serait lu comme une option d'adb
            return false;
        for (char ch : serie)
            if (!(std::isalnum((unsigned char)ch) || ch == '.' || ch == '-' || ch == '_'))
                return false;
        return true;
    }

    bool est_sans_fil(const std::string& serie)
    {
        return serie.find(':') != std::string::npos ||
               serie.find("._adb-tls-connect._tcp") != std::string::npos ||
               serie.find("._adb._tcp") != std::string::npos;
    }

    ListeUsb analyser(const std::string& sortie)
    {
        ListeUsb liste;
        bool entete_vu = false;
        std::istringstream flux(sortie);
        std::string ligne;
        while (std::getline(flux, ligne))
        {
            if (!ligne.empty() && ligne.back() == '\r')
                ligne.pop_back();
            if (ligne.rfind("List of devices attached", 0) == 0)
            {
                entete_vu = true;
                continue;
            }
            // Avant l'en-tête : messages du serveur (« * daemon started successfully », erreurs)
            if (!entete_vu || ligne.empty() || ligne[0] == '*')
                continue;

            std::istringstream mots(ligne);
            AppareilUsb appareil;
            if (!(mots >> appareil.serie >> appareil.etat))
                continue;
            if (appareil.etat == "no") // « no permissions (...) » : deux mots
            {
                std::string suite;
                mots >> suite;
                appareil.etat += " " + suite;
            }
            if (!etat_connu(appareil.etat) || est_sans_fil(appareil.serie))
                continue;

            // Détails « clé:valeur » (adb devices -l). Seul model: nous sert.
            std::string mot;
            while (mots >> mot)
                if (mot.rfind("model:", 0) == 0)
                {
                    appareil.modele = mot.substr(6);
                    for (char& ch : appareil.modele)
                        if (ch == '_')
                            ch = ' ';
                }
            appareil.emulateur = appareil.serie.rfind("emulator-", 0) == 0;
            liste.appareils.push_back(std::move(appareil));
        }
        if (!entete_vu)
            liste.erreur = nettoyer(sortie).empty() ? "adb n'a rien répondu." : nettoyer(sortie);
        return liste;
    }

    ListeUsb lister()
    {
        return analyser(executer("adb devices -l"));
    }

    std::string conseil(const AppareilUsb& appareil)
    {
        const std::string& etat = appareil.etat;
        if (appareil.connectable())
            return "";
        if (etat == "device") // autorisé, mais numéro de série refusé par serie_valide
            return "Numéro de série inhabituel : ce téléphone ne peut pas être analysé.";
        if (etat == "unauthorized")
            return "Déverrouille le téléphone et accepte la demande « Autoriser le débogage USB ».";
        if (etat == "authorizing" || etat == "connecting")
            return "Connexion en cours, patiente quelques secondes.";
        if (etat == "offline")
            return "Le téléphone ne répond pas : débranche puis rebranche le câble.";
        if (etat == "no permissions")
            return "Linux bloque l'accès au téléphone. Installe les règles udev "
                   "(sudo apt install android-sdk-platform-tools-common), ajoute-toi au groupe plugdev, "
                   "puis débranche et rebranche le câble.";
        if (etat == "recovery" || etat == "sideload" || etat == "bootloader" || etat == "rescue")
            return "Le téléphone n'est pas démarré normalement : redémarre-le sur Android.";
        return "Le téléphone n'est pas prêt : débranche puis rebranche le câble.";
    }

    std::string etat_court(const AppareilUsb& appareil)
    {
        const std::string& etat = appareil.etat;
        if (appareil.connectable())   return "Prêt";
        if (etat == "device")         return "Refusé";
        if (etat == "unauthorized")   return "Non autorisé";
        if (etat == "authorizing" || etat == "connecting") return "Connexion...";
        if (etat == "offline")        return "Hors ligne";
        if (etat == "no permissions") return "Accès refusé";
        if (etat == "recovery" || etat == "sideload" || etat == "bootloader" || etat == "rescue")
            return "Mode spécial";
        return "Indisponible";
    }
}
