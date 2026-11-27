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
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

// ---------- Marqueur de fin ----------
// Le shell affiche « \n<MARQUEUR><code de retour>\n » une fois la commande terminée.
namespace
{
    const std::string MARQUEUR_FIN = "@@LACHE_MON_ANDROID_FIN@@";

    thread_local size_t commandes_incompletes = 0;

    // Ajoute l'affichage du marqueur après la commande (sous-shell : un « exit » ne l'empêche pas).
    // `redirection` reste juste après la commande, comme avant, pour ne rien changer à son comportement.
    std::string avec_marqueur(const std::string& commande, const char* redirection)
    {
        return "( " + commande + " " + redirection + " ) ; printf '\\n" + MARQUEUR_FIN + "%d\\n' \"$?\"";
    }

    // Retire le marqueur de la fin de `brut` et remplit `r` (sortie, marqueur_recu, code_retour)
    void extraire_marqueur(std::string& brut, ResultatCommande& r)
    {
        const size_t pos = brut.rfind("\n" + MARQUEUR_FIN);
        if (pos != std::string::npos)
        {
            const std::string code = nettoyer(brut.substr(pos + 1 + MARQUEUR_FIN.size()));
            if (!code.empty() && code.find_first_not_of("0123456789") == std::string::npos)
            {
                r.marqueur_recu = true;
                r.code_retour   = std::atoi(code.c_str());
                brut.erase(pos);
            }
        }
        r.sortie = std::move(brut);
    }

    // Messages d'adb quand l'appareil n'est plus joignable (sur stderr, avec un code ≠ 0).
    // Seules les lignes écrites par adb lui-même comptent (« adb: ... » ou « error: ... ») :
    // un « sh: unzip: not found » venant du téléphone n'est pas une coupure.
    bool message_coupure_adb(const std::string& sortie)
    {
        std::istringstream lignes(sortie);
        std::string ligne;
        while (std::getline(lignes, ligne))
        {
            if (ligne.rfind("adb: ", 0) != 0 && ligne.rfind("error: ", 0) != 0)
                continue;
            for (const char* motif : {"device offline", "not found", "no devices/emulators found",
                                      "closed", "protocol fault", "unauthorized"})
                if (ligne.find(motif) != std::string::npos)
                    return true;
        }
        return false;
    }

    // Une commande incomplète est comptée pour l'analyse en cours et notée dans le journal
    void noter_si_incomplete(const std::string& commande, const ResultatCommande& r)
    {
        if (r.complete())
            return;
        ++commandes_incompletes;
        std::clog << "[COMMANDE] incomplète (" << (r.marqueur_recu ? "appareil injoignable" : "marqueur absent")
                  << ") : " << commande << std::endl;
    }
}

ResultatCommande executer_avec_marqueur(const std::string& commande)
{
    ResultatCommande r;
    std::string brut;
    FILE* flux = popen(avec_marqueur(commande, "2>&1").c_str(), "r");
    if (flux)
    {
        char tampon[512];
        while (fgets(tampon, sizeof(tampon), flux))
            brut += tampon;
        pclose(flux);
    }
    extraire_marqueur(brut, r);
    r.coupure_adb = r.marqueur_recu && r.code_retour != 0 && message_coupure_adb(r.sortie);
    return r;
}

ResultatCommande executer_binaire_avec_marqueur(const std::string& commande)
{
    ResultatCommande r;
    std::string brut;
    FILE* flux = popen(avec_marqueur(commande, "2>/dev/null").c_str(), "r");
    if (flux)
    {
        char tampon[65536];
        size_t lus;
        while ((lus = fread(tampon, 1, sizeof(tampon), flux)) > 0)
            brut.append(tampon, lus);
        pclose(flux);
    }
    extraire_marqueur(brut, r);
    // stderr est ignoré ici : « adb exec-out » ne renvoie un code ≠ 0 que si adb lui-même échoue
    r.coupure_adb = r.marqueur_recu && r.code_retour != 0 && commande.find("exec-out") != std::string::npos;
    return r;
}

void suivi_commandes_remettre_a_zero()
{
    commandes_incompletes = 0;
}

size_t suivi_commandes_incompletes()
{
    return commandes_incompletes;
}

std::string executer(const std::string& commande)
{
    const ResultatCommande r = executer_avec_marqueur(commande);
    noter_si_incomplete(commande, r);
    return r.sortie;
}

std::string executer_binaire(const std::string& commande)
{
    const ResultatCommande r = executer_binaire_avec_marqueur(commande);
    noter_si_incomplete(commande, r);
    return r.sortie;
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
