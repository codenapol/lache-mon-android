// Lache mon Android ! - Applications connues (bloatwares, problématiques, alternatives)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "catalogue_apps.h"

#include "alternatives.h" // fiche_alternative


// Applications préinstallées inutiles les plus courantes (Google, constructeurs, partenaires)
const std::vector<AppConnue> LISTE_BLOATWARES = {
    {"com.google.android.youtube",               "YouTube"},
    {"com.google.android.apps.youtube.music",    "YouTube Music"},
    {"com.google.android.videos",                "Google TV"},
    {"com.google.android.apps.tachyon",          "Google Meet (Duo)"},
    {"com.google.android.apps.photos",           "Google Photos"},
    {"com.google.android.apps.docs",             "Google Drive"},
    {"com.google.android.apps.maps",             "Google Maps"},
    {"com.google.android.gm",                    "Gmail"},
    {"com.google.android.googlequicksearchbox",  "Application Google"},
    {"com.google.android.apps.googleassistant",  "Google Assistant"},
    {"com.google.ar.lens",                       "Google Lens"},
    {"com.google.android.apps.magazines",        "Google Actualités"},
    {"com.google.android.apps.podcasts",         "Google Podcasts"},
    {"com.google.android.apps.subscriptions.red","Google One"},
    {"com.google.android.apps.wellbeing",        "Bien-être numérique"},
    {"com.google.android.feedback",              "Google Feedback"},
    {"com.google.android.music",                 "Google Play Musique"},
    {"com.google.android.apps.restore",          "Google Restore"},
    {"com.facebook.appmanager",                  "Meta App Manager"},
    {"com.facebook.services",                    "Meta Services"},
    {"com.facebook.system",                      "Meta App Installer"},
    {"com.netflix.partner.activation",           "Netflix (activation partenaire)"},
    {"com.microsoft.skydrive",                   "OneDrive"},
    {"com.microsoft.office.officehubrow",        "Microsoft 365"},
    {"com.linkedin.android",                     "LinkedIn"},
    {"com.booking",                              "Booking.com"},
    {"com.opera.preinstall",                     "Opera (préinstallé)"},
    {"com.samsung.android.bixby.agent",          "Bixby"},
    {"com.samsung.android.app.spage",            "Samsung Free"},
    {"com.samsung.android.game.gamehome",        "Game Launcher"},
    {"com.samsung.android.arzone",               "AR Zone"},
    {"com.sec.android.app.sbrowser",             "Samsung Internet"},
    {"com.miui.analytics",                       "MIUI Analytics"},
    {"com.miui.msa.global",                      "MIUI MSA (pubs)"},
    {"com.miui.videoplayer",                     "Mi Vidéo"},
    {"com.mi.globalbrowser",                     "Mi Browser"},
    {"com.xiaomi.glgm",                          "Xiaomi Games"},
    {"com.debug.loggerui",                       "MTK Logger"},
};

// Applications problématiques (vie privée, pistage, publicité)
const std::vector<AppConnue> LISTE_PROBLEMATIQUES = {
    {"com.whatsapp",                             "WhatsApp"},
    {"com.whatsapp.w4b",                         "WhatsApp Business"},
    {"com.facebook.katana",                      "Facebook"},
    {"com.facebook.lite",                        "Facebook Lite"},
    {"com.facebook.orca",                        "Messenger"},
    {"com.facebook.mlite",                       "Messenger Lite"},
    {"com.instagram.android",                    "Instagram"},
    {"com.instagram.barcelona",                  "Threads"},
    {"com.amazon.mShop.android.shopping",        "Amazon Shopping"},
    {"in.amazon.mShop.android.shopping",         "Amazon Shopping"},
    {"com.amazon.avod.thirdpartyclient",         "Prime Video"},
    {"com.amazon.mp3",                           "Amazon Music"},
    {"com.amazon.kindle",                        "Kindle"},
    {"com.amazon.dee.app",                       "Alexa"},
    {"com.zhiliaoapp.musically",                 "TikTok"},
    {"com.ss.android.ugc.trill",                 "TikTok"},
    {"com.snapchat.android",                     "Snapchat"},
    {"com.twitter.android",                      "X (Twitter)"},
    {"com.einnovation.temu",                     "Temu"},
    {"com.zzkko",                                "Shein"},
    {"com.alibaba.aliexpresshd",                 "AliExpress"},

    // Meta (autres applis)
    {"com.instagram.lite",                       "Instagram Lite"},
    {"com.facebook.talk",                        "Messenger Kids"},

    // ByteDance / vidéo courte (collecte massive, transferts hors UE)
    {"com.zhiliaoapp.musically.go",              "TikTok Lite"},
    {"com.ss.android.ugc.aweme",                 "Douyin"},
    {"com.lemon.lvoverseas",                     "CapCut"},
    {"com.kwai.video",                           "Kwai"},
    {"video.like",                               "Likee"},
    {"sg.bigo.live",                             "BIGO LIVE"},

    // Réseaux sociaux / messageries
    {"com.tencent.mm",                           "WeChat"},
    {"com.vkontakte.android",                    "VK"},
    {"com.discord",                              "Discord"},

    // Rencontres (données sensibles partagées avec des régies publicitaires)
    {"com.tinder",                               "Tinder"},
    {"com.bumble.app",                           "Bumble"},
    {"com.badoo.mobile",                         "Badoo"},
    {"com.grindrapp.android",                    "Grindr"},

    // Annuaire / identification d'appel (envoie le carnet de contacts)
    {"com.truecaller",                           "Truecaller"},

    // Localisation revendue à des courtiers en données
    {"com.life360.android.safetymapd",           "Life360"},
    {"com.weather.Weather",                      "The Weather Channel"},
    {"com.accuweather.android",                  "AccuWeather"},

    // "Nettoyeurs", navigateurs et outils connus pour pistage ou failles
    {"com.cleanmaster.mguard",                   "Clean Master"},
    {"com.cleanmaster.security",                 "CM Security"},
    {"com.lenovo.anyshare.gps",                  "SHAREit"},
    {"com.estrongs.android.pop",                 "ES Explorateur de fichiers"},
    {"com.UCMobile.intl",                        "UC Browser"},
    {"com.opera.mini.native",                    "Opera Mini"},
    {"com.yandex.browser",                       "Yandex Browser"},

    // Shopping (pistage publicitaire agressif)
    {"com.contextlogic.wish",                    "Wish"},
    {"com.alibaba.intl.android.apps.poseidon",   "Alibaba.com"},
    {"com.amazon.venezia",                       "Amazon Appstore"},
};

// Applications qui ont une alternative libre / respectueuse de la vie privée (bouton "Remplacer")
const AppConnue LISTE_ALTERNATIVES[] = {
    // {paquet à remplacer, alternative proposée}
    {"com.google.android.youtube",          "NewPipe"},
    {"com.google.android.apps.youtube.music","NewPipe"},
    {"com.google.android.apps.photos",      "Fossify Gallery"},
    {"com.google.android.apps.maps",        "Organic Maps"},
    {"com.google.android.gm",               "Thunderbird"},
    {"com.google.android.apps.docs",        "Nextcloud"},
    {"com.sec.android.app.sbrowser",        "IronFox"},
    {"com.mi.globalbrowser",                "IronFox"},
    {"com.opera.preinstall",                "IronFox"},
    {"com.miui.videoplayer",                "VLC"},
    {"com.whatsapp",                        "Signal"},
    {"com.whatsapp.w4b",                    "Signal"},
    {"com.facebook.orca",                   "Signal"},
    {"com.facebook.mlite",                  "Signal"},
    {"com.instagram.android",               "PixelDroid"},
    {"com.twitter.android",                 "Mastodon"},
    {"com.instagram.barcelona",             "Mastodon"},
    {"com.amazon.kindle",                   "KOReader"},
    {"com.tencent.mm",                      "Signal"},
    {"com.facebook.katana",                 "Fedilab"},
    {"com.facebook.lite",                   "Fedilab"},
    {"com.instagram.lite",                  "PixelDroid"},
    {"com.truecaller",                      "Fossify Phone"},
    {"com.weather.Weather",                 "Breezy Weather"},
    {"com.accuweather.android",             "Breezy Weather"},
    {"com.estrongs.android.pop",            "Fossify File Manager"},
    {"com.lenovo.anyshare.gps",             "LocalSend"},
    {"com.UCMobile.intl",                   "IronFox"},
    {"com.opera.mini.native",               "IronFox"},
    {"com.yandex.browser",                  "IronFox"},
    {"com.lemon.lvoverseas",                "Open Video Editor"},
};

// Renvoie l'alternative d'un paquet, ou "" s'il n'y en a pas (ou si elle n'a pas de fiche)
std::string alternative_de(const std::string& paquet)
{
    for (const AppConnue& alt : LISTE_ALTERNATIVES)
        if (paquet == alt.paquet)
            return fiche_alternative(alt.nom) ? alt.nom : "";
    return "";
}

// Nom lisible d'un paquet (depuis nos listes), sinon le nom du paquet lui-même
std::string nom_de(const std::string& paquet)
{
    for (const AppConnue& app : LISTE_BLOATWARES)
        if (paquet == app.paquet) return app.nom;
    for (const AppConnue& app : LISTE_PROBLEMATIQUES)
        if (paquet == app.paquet) return app.nom;
    return paquet;
}
