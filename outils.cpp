// Lache mon Android ! - Fonctions partagées (shell, fichiers, téléchargement)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "outils.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::string executer(const std::string& commande)
{
    std::string sortie;
    FILE* flux = popen((commande + " 2>&1").c_str(), "r");
    if (!flux)
        return sortie;
    char tampon[512];
    while (fgets(tampon, sizeof(tampon), flux))
        sortie += tampon;
    pclose(flux);
    return sortie;
}

std::string executer_binaire(const std::string& commande)
{
    std::string sortie;
    FILE* flux = popen((commande + " 2>/dev/null").c_str(), "r");
    if (!flux)
        return sortie;
    char tampon[65536];
    size_t lus;
    while ((lus = fread(tampon, 1, sizeof(tampon), flux)) > 0)
        sortie.append(tampon, lus);
    pclose(flux);
    return sortie;
}

std::string nettoyer(const std::string& s)
{
    const char* blancs = " \t\r\n";
    size_t debut = s.find_first_not_of(blancs);
    if (debut == std::string::npos)
        return "";
    size_t fin = s.find_last_not_of(blancs);
    return s.substr(debut, fin - debut + 1);
}

std::string minuscules(std::string s)
{
    for (char& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

bool paquet_valide(const std::string& paquet)
{
    if (paquet.empty())
        return false;
    for (char ch : paquet)
        if (!(std::isalnum((unsigned char)ch) || ch == '.' || ch == '_'))
            return false;
    return true;
}

std::string entre_quotes(const std::string& s)
{
    std::string r = "'";
    for (char ch : s)
        r += (ch == '\'') ? std::string("'\\''") : std::string(1, ch);
    return r + "'";
}

fs::path dossier_donnees()
{
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
        return fs::path(xdg);
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : ".") / ".local" / "share";
}

fs::path dossier_programme()
{
    return dossier_donnees() / "lache_mon_android";
}

std::string lire_fichier(const fs::path& chemin)
{
    std::ifstream f(chemin, std::ios::binary);
    std::ostringstream contenu;
    contenu << f.rdbuf();
    return contenu.str();
}

bool telecharger(const std::string& url, const fs::path& destination)
{
    const fs::path temporaire = destination.string() + ".tmp";
    const std::string commande = "curl -sSfL --connect-timeout 10 --max-time 120 -o " +
                                 entre_quotes(temporaire.string()) + " " + entre_quotes(url) +
                                 " > /dev/null 2>&1";
    std::error_code ec;
    if (std::system(commande.c_str()) != 0)
    {
        fs::remove(temporaire, ec);
        return false;
    }
    fs::rename(temporaire, destination, ec);
    return !ec;
}
