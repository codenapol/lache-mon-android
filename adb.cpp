// Lache mon Android ! - Commandes ADB de base (adb embarqué, connexion, commandes)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "adb.h"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <unistd.h> // access

#include <SDL2/SDL.h> // SDL_GetBasePath

#include "outils.h" // executer, nettoyer

// Dossier de l'exécutable (avec "/" final), ou "" si inconnu. Dans une AppImage : .../usr/bin/
std::string dossier_executable()
{
    std::string dossier;
    if (char* base = SDL_GetBasePath())
    {
        dossier = base;
        SDL_free(base);
    }
    return dossier;
}

// Si un adb est fourni à côté de l'exécutable (AppImage), il passe avant celui du système :
// sa version est connue et assez récente pour le débogage sans fil.
void utiliser_adb_embarque()
{
    const std::string dossier = dossier_executable();
    if (dossier.empty() || access((dossier + "adb").c_str(), X_OK) != 0)
        return;
    const char* path = std::getenv("PATH");
    std::string nouveau = dossier.substr(0, dossier.size() - 1); // sans le "/" final
    if (path && *path)
        nouveau += std::string(":") + path;
    setenv("PATH", nouveau.c_str(), 1);
    std::clog << "[ADB-VERIF] adb embarqué utilisé : " << dossier << "adb" << std::endl;
}

/* Vérifie que adb (Android Debug Bridge) est installé et accessible dans le PATH.
 Retourne true si adb est trouvé et répond, false sinon.*/ 
bool ADB_VERIF()
{
    int resultat = std::system("adb version > /dev/null 2>&1");

    if (resultat == 0)
    {
        std::clog << "[ADB-VERIF] adb est installé." << std::endl;
        return true;
    }

    std::cerr << "[ADB-VERIF] adb est introuvable ! Installe-le avec : sudo apt install adb" << std::endl;
    return false;
}

// L'IP est insérée dans une commande shell : on n'accepte que des caractères sûrs
bool ip_valide(const std::string& ip)
{
    if (ip.empty())
        return false;
    for (char ch : ip)
        if (!(std::isalnum((unsigned char)ch) || ch == '.' || ch == '-' || ch == ':'))
            return false;
    return true;
}

// Connexion ADB sans fil. Renvoie true si connecté, sinon message d'erreur dans `erreur`.
bool connexion_adb(const std::string& cible, std::string& erreur)
{
    std::string sortie = nettoyer(executer("adb connect " + cible));
    std::clog << "[ADB] " << sortie << std::endl;

    // "connected to x" ou "already connected to x" = succès ; "failed to connect to x" = échec
    if (sortie.find("connected to") != std::string::npos && sortie.find("failed") == std::string::npos)
        return true;

    erreur = sortie.empty() ? "Connexion impossible." : sortie;
    return false;
}

// Lance une commande "am start ..." sur le téléphone. Renvoie "" si tout va bien, sinon la sortie d'erreur.
std::string commande_telephone(const std::string& cible, const std::string& commande_shell)
{
    const std::string sortie = nettoyer(executer("adb -s " + cible + " shell \"" + commande_shell + "\""));
    std::clog << "[REMPLACER] " << commande_shell << " -> " << sortie << std::endl;
    const bool echec = sortie.find("Error")          != std::string::npos ||
                       sortie.find("Exception")      != std::string::npos ||
                       sortie.find("No activities")  != std::string::npos ||
                       sortie.find("monkey aborted") != std::string::npos;
    return echec ? sortie : "";
}
