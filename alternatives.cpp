// Lache mon Android ! - Alternatives libres (fenêtre « Remplacer »)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "alternatives.h"
#include "json.h"
#include "outils.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <cctype>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;

const std::vector<Alternative> CATALOGUE_ALTERNATIVES = {
    // Sur F-Droid : version et logo viennent de f-droid.org
    {"NewPipe",              "org.schabi.newpipe",                 "Lecteur YouTube léger, sans publicité ni compte Google."},
    {"Fossify Gallery",      "org.fossify.gallery",                "Galerie photo hors ligne, sans publicité."},
    {"Organic Maps",         "app.organicmaps",                    "Cartes et GPS hors ligne (OpenStreetMap), sans pistage."},
    {"Thunderbird",          "net.thunderbird.android",            "Messagerie e-mail, compatible avec Gmail et les autres boîtes."},
    {"Nextcloud",            "com.nextcloud.client",               "Synchronise tes fichiers avec un serveur Nextcloud."},
    {"VLC",                  "org.videolan.vlc",                   "Lecteur vidéo et audio qui lit tous les formats."},
    {"PixelDroid",           "org.pixeldroid.app",                 "Partage de photos sur Pixelfed, un réseau décentralisé sans publicité."},
    {"Mastodon",             "org.joinmastodon.android",           "Réseau social décentralisé, sans algorithme ni publicité."},
    {"KOReader",             "org.koreader.launcher.fdroid",       "Liseuse de livres numériques (EPUB, PDF, ...)."},
    {"Fedilab",              "fr.gouv.etalab.mastodon",            "Réseaux sociaux décentralisés : Friendica, Mastodon, Pixelfed, ..."},
    {"Fossify Phone",        "org.fossify.phone",                  "Téléphone avec blocage d'appels, sans envoyer tes contacts."},
    {"Breezy Weather",       "org.breezyweather",                  "Météo sans publicité ni revente de ta position."},
    {"Fossify File Manager", "org.fossify.filemanager",            "Gestionnaire de fichiers simple, sans publicité."},
    {"LocalSend",            "org.localsend.localsend_app",        "Partage de fichiers entre appareils sur le même Wi-Fi, sans passer par Internet."},
    {"Open Video Editor",    "io.github.devhyper.openvideoeditor", "Montage vidéo simple, sans filigrane."},

    // Hors F-Droid : site officiel, logo et version publiés par l'éditeur
    {"Signal", "org.thoughtcrime.securesms",
     "Messagerie chiffrée de bout en bout, gérée par une fondation à but non lucratif.",
     "https://signal.org/android/apk/",
     "https://signal.org/assets/images/favicon/apple-touch-icon.png",
     "https://updates.signal.org/android/latest.json", "versionName"},
    {"IronFox", "org.ironfoxoss.ironfox",
     "Navigateur basé sur Firefox, renforcé pour la vie privée.",
     "https://ironfoxoss.org/",
     "https://ironfoxoss.org/apple-touch-icon.png",
     "https://gitlab.com/api/v4/projects/ironfox-oss%2FIronFox/releases?per_page=1", "tag_name"},
};

const std::vector<std::string> CLIENTS_FDROID = {
    "org.fdroid.fdroid", "org.fdroid.basic", "com.looker.droidify", "com.machiav3lli.fdroid",
};

const char* const URL_APK_FDROID = "https://f-droid.org/F-Droid.apk";

const Alternative* fiche_alternative(const std::string& nom)
{
    for (const Alternative& alt : CATALOGUE_ALTERNATIVES)
        if (nom == alt.nom)
            return &alt;
    return nullptr;
}

namespace
{

fs::path dossier_cache() { return dossier_programme() / "alternatives"; }

// "v160.0.1" -> "160.0.1"
std::string sans_prefixe_v(const std::string& version)
{
    if (version.size() > 1 && (version[0] == 'v' || version[0] == 'V') && std::isdigit((unsigned char)version[1]))
        return version.substr(1);
    return version;
}

// API F-Droid : {"suggestedVersionCode": 1017, "packages": [{"versionName": "0.29.3", "versionCode": 1017}, ...]}
bool lire_version_fdroid(const std::string& contenu, std::string& version, std::string& code)
{
    Json racine;
    if (!LecteurJson(contenu).lire(racine) || racine.type != Json::OBJET)
        return false;
    const Json* suggere = racine.champ("suggestedVersionCode");
    const Json* versions = racine.champ("packages");
    if (!suggere || suggere->type != Json::NOMBRE || !versions || versions->type != Json::TABLEAU)
        return false;

    code = suggere->texte;
    for (const Json& v : versions->tableau)
        if (const Json* c = v.champ("versionCode"); c && c->texte == code)
        {
            version = v.texte_de("versionName");
            return !version.empty();
        }
    return false;
}

// JSON de l'éditeur : objet {"<champ>": "8.37.3", ...}, ou tableau dont on prend le 1er élément
bool lire_version_editeur(const std::string& contenu, const std::string& champ, std::string& version)
{
    Json racine;
    if (!LecteurJson(contenu).lire(racine))
        return false;
    const Json* objet = &racine;
    if (racine.type == Json::TABLEAU)
        objet = racine.tableau.empty() ? nullptr : &racine.tableau.front();
    if (!objet || objet->type != Json::OBJET)
        return false;
    version = sans_prefixe_v(objet->texte_de(champ));
    return !version.empty();
}

// Décode une image en pixels RGBA. SDL2_image reconnaît le format au contenu (PNG, WebP, JPEG,
// GIF, BMP, ICO, SVG, AVIF, ...), quelle que soit l'extension du fichier.
bool decoder_image(const std::string& donnees, InfosAlternative& infos)
{
    if (donnees.empty())
        return false;
    SDL_RWops* flux = SDL_RWFromConstMem(donnees.data(), (int)donnees.size());
    SDL_Surface* brute = flux ? IMG_Load_RW(flux, 1) : nullptr; // 1 : IMG_Load_RW ferme le flux
    if (!brute)
        return false;
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(brute, SDL_PIXELFORMAT_ABGR8888, 0); // octets R, G, B, A
    SDL_FreeSurface(brute);
    if (!rgba)
        return false;

    const size_t ligne = (size_t)rgba->w * 4;
    infos.icone.resize(ligne * rgba->h);
    for (int y = 0; y < rgba->h; ++y)
        std::memcpy(&infos.icone[y * ligne], (const unsigned char*)rgba->pixels + (size_t)y * rgba->pitch, ligne);
    infos.largeur = rgba->w;
    infos.hauteur = rgba->h;
    SDL_FreeSurface(rgba);
    return true;
}

// Logo depuis le cache, sinon depuis la première adresse qui donne une image lisible
void charger_logo(const fs::path& fichier, const std::vector<std::string>& urls, InfosAlternative& infos)
{
    if (decoder_image(lire_fichier(fichier), infos))
        return;
    for (const std::string& url : urls)
        if (telecharger(url, fichier) && decoder_image(lire_fichier(fichier), infos))
            return;
    std::error_code ec;
    fs::remove(fichier, ec); // rien de lisible : ne pas garder un fichier inutilisable
    std::clog << "[ALTERNATIVES] " << fichier.filename().string() << " : logo introuvable" << std::endl;
}

} // namespace

namespace alternatives
{

InfosAlternative chercher(const Alternative& alt)
{
    InfosAlternative infos;
    const std::string paquet = alt.paquet;
    if (!paquet_valide(paquet))
        return infos;

    std::error_code ec;
    fs::create_directories(dossier_cache(), ec);

    // Version : retéléchargée à chaque fois, l'ancienne réponse sert si on est hors ligne
    const bool sur_fdroid = alt.url_version == nullptr;
    const fs::path fichier_version = dossier_cache() / (paquet + ".json");
    const std::string url_version = sur_fdroid ? "https://f-droid.org/api/v1/packages/" + paquet
                                               : std::string(alt.url_version);
    if (!telecharger(url_version, fichier_version))
        std::clog << "[ALTERNATIVES] " << paquet << " : pas de réponse, utilisation du cache" << std::endl;

    if (sur_fdroid)
    {
        std::string code;
        infos.trouve = lire_version_fdroid(lire_fichier(fichier_version), infos.version, code);
        // Logo de la version suggérée (une icône "icons-640/*.png" est parfois au format WebP)
        if (infos.trouve)
            charger_logo(dossier_cache() / (paquet + "." + code + ".logo"),
                         {"https://f-droid.org/repo/icons-640/" + paquet + "." + code + ".png",
                          "https://f-droid.org/repo/" + paquet + "/en-US/icon.png"},
                         infos);
    }
    else
    {
        infos.trouve = lire_version_editeur(lire_fichier(fichier_version),
                                            alt.champ_version ? alt.champ_version : "", infos.version);
        // Le logo ne dépend pas de la version : il est cherché même si la version est introuvable
        if (alt.url_logo)
            charger_logo(dossier_cache() / (paquet + ".logo"), {alt.url_logo}, infos);
    }
    return infos;
}

} // namespace alternatives
