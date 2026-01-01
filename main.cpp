// Lache mon Android ! - Interface, connexion ADB et debloat
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include <iostream>
#include <string>
#include <cstdlib>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h" // InputText avec std::string

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "outils.h"   // executer, nettoyer, paquet_valide
#include "securite.h" // détection de logiciels espions
#include "uad.h"      // liste de debloat UAD-ng

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

struct AppConnue
{
    const char* paquet;
    const char* nom;
};

// Applications préinstallées inutiles les plus courantes (Google, constructeurs, partenaires)
const AppConnue LISTE_BLOATWARES[] = {
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
const AppConnue LISTE_PROBLEMATIQUES[] = {
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
    {"com.instagram.android",               "Pixelfed"},
    {"com.twitter.android",                 "Mastodon"},
    {"com.instagram.barcelona",             "Mastodon"},
    {"com.amazon.kindle",                   "KOReader"},
};

// Renvoie l'alternative d'un paquet, ou "" s'il n'y en a pas
std::string alternative_de(const std::string& paquet)
{
    for (const AppConnue& alt : LISTE_ALTERNATIVES)
        if (paquet == alt.paquet)
            return alt.nom;
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

struct AppTrouvee
{
    std::string nom;
    std::string paquet;
    std::string alternative; // vide = pas de bouton "Remplacer"
    std::string description; // description UAD-ng (en anglais), vide si inconnue
    std::string requis_par;  // paquets qui en dépendent (UAD-ng), séparés par des virgules
};

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

// Construit une AppTrouvee : nom de nos listes, sinon de UAD-ng, sinon le paquet ;
// description et dépendances de UAD-ng si le paquet y figure
AppTrouvee creer_app(const std::string& paquet, std::string nom = "")
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

struct InfosAppareil
{
    std::string fabricant;
    std::string modele;
    std::string version_android;
    std::string nom_commercial; // nom affiché dans les paramètres (ex. "Galaxy S26")
    std::vector<AppTrouvee> bloatwares;     // nos listes + UAD-ng "Recommended"
    std::vector<AppTrouvee> avancees;       // UAD-ng "Advanced"
    std::vector<AppTrouvee> expertes;       // UAD-ng "Expert"
    std::vector<AppTrouvee> problematiques;
    std::vector<AppTrouvee> desactivees;
    std::vector<AppTrouvee> desinstallees;
    RapportSecurite securite; // logiciels espions, accès sensibles, réglages affaiblis
    std::string erreur; // vide si tout s'est bien passé
};

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

    std::clog << "[DETECTION] " << infos.fabricant << " " << infos.modele
              << " | " << infos.bloatwares.size() << " bloatwares, "
              << infos.problematiques.size() << " applis problématiques, "
              << infos.desactivees.size() << " désactivées, "
              << infos.desinstallees.size() << " désinstallées" << std::endl;
    return infos;
}

// Action demandée par un clic dans une liste
enum TypeAction { AUCUNE, DESACTIVER, DESINSTALLER, REACTIVER, REINSTALLER };
struct Action
{
    TypeAction type = AUCUNE;
    std::string paquet;
    std::string nom;
};

// Titre de section : police grasse + couleur d'accent (style Fossify)
void titre_section(ImFont* police_grasse, const char* titre, int nombre,
                   ImVec4 couleur = ImVec4(0.40f, 0.60f, 0.95f, 1.0f))
{
    ImGui::Dummy(ImVec2(0, 6));
    if (police_grasse) ImGui::PushFont(police_grasse);
    ImGui::PushStyleColor(ImGuiCol_Text, couleur);
    ImGui::Text("%s", titre);
    ImGui::PopStyleColor();
    if (police_grasse) ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("%d", nombre);
}

// Petit bouton discret (couleur des champs, peu de marge)
bool petit_bouton(const char* texte, float largeur, bool destructif = false)
{
    const ImGuiStyle& s = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(s.FramePadding.x * 0.5f, s.FramePadding.y));
    ImGui::PushStyleColor(ImGuiCol_Button,        s.Colors[ImGuiCol_FrameBg]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s.Colors[ImGuiCol_FrameBgHovered]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  s.Colors[ImGuiCol_FrameBgActive]);
    if (destructif)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.55f, 0.55f, 1.0f));
    const bool clic = ImGui::Button(texte, ImVec2(largeur, 0));
    ImGui::PopStyleColor(destructif ? 4 : 3);
    ImGui::PopStyleVar();
    return clic;
}

// Infobulle d'une appli : paquet, description UAD-ng, applis qui en dépendent
void infobulle_app(const AppTrouvee& app)
{
    if (!ImGui::BeginItemTooltip())
        return;
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
    ImGui::TextDisabled("%s", app.paquet.c_str());
    if (!app.description.empty())
        ImGui::TextUnformatted(app.description.c_str());
    if (!app.requis_par.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.35f, 1.0f), "Nécessaire à : %s", app.requis_par.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// Titre de section repliable (fermé par défaut), même style que titre_section
bool titre_repliable(ImFont* police_grasse, const char* titre, int nombre, const char* id)
{
    ImGui::Dummy(ImVec2(0, 6));
    if (police_grasse) ImGui::PushFont(police_grasse);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.60f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    const std::string etiquette = std::string(titre) + "  (" + std::to_string(nombre) + ")###" + id;
    const bool ouvert = ImGui::TreeNodeEx(etiquette.c_str(),
        ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    ImGui::PopStyleColor(2);
    if (police_grasse) ImGui::PopFont();
    return ouvert;
}

// Liste bloatwares / applis problématiques : nom | Remplacer | Désactiver | Désinstaller
void afficher_liste_apps(const char* id, const std::vector<AppTrouvee>& apps, Action& action)
{
    if (apps.empty())
    {
        ImGui::TextDisabled("Aucune");
        return;
    }
    const float marge = ImGui::GetStyle().FramePadding.x;
    const float l_remplacer   = ImGui::CalcTextSize("Remplacer").x    + marge;
    const float l_desactiver  = ImGui::CalcTextSize("Désactiver").x   + marge;
    const float l_desinstaller= ImGui::CalcTextSize("Désinstaller").x + marge;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 3));
    if (ImGui::BeginTable(id, 4, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("nom",          ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("remplacer",    ImGuiTableColumnFlags_WidthFixed, l_remplacer);
        ImGui::TableSetupColumn("desactiver",   ImGuiTableColumnFlags_WidthFixed, l_desactiver);
        ImGui::TableSetupColumn("desinstaller", ImGuiTableColumnFlags_WidthFixed, l_desinstaller);

        for (const AppTrouvee& app : apps)
        {
            ImGui::PushID(app.paquet.c_str());
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(app.nom.c_str());
            infobulle_app(app); // paquet, description UAD-ng, dépendances

            ImGui::TableNextColumn();
            if (!app.alternative.empty())
            {
                if (petit_bouton("Remplacer", -FLT_MIN))
                {
                    // TODO : remplacer app.paquet par app.alternative
                }
                ImGui::SetItemTooltip("par %s", app.alternative.c_str());
            }

            ImGui::TableNextColumn();
            if (petit_bouton("Désactiver", -FLT_MIN))
                action = {DESACTIVER, app.paquet, app.nom};

            ImGui::TableNextColumn();
            if (petit_bouton("Désinstaller", -FLT_MIN, true))
                action = {DESINSTALLER, app.paquet, app.nom};

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// Liste avec un seul bouton de restauration (Réactiver / Réinstaller)
void afficher_liste_restauration(const char* id, const std::vector<AppTrouvee>& apps,
                                 const char* texte_bouton, TypeAction type, Action& action)
{
    if (apps.empty())
    {
        ImGui::TextDisabled("Aucune");
        return;
    }
    const float l_bouton = ImGui::CalcTextSize(texte_bouton).x + ImGui::GetStyle().FramePadding.x;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 3));
    if (ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("nom",    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthFixed, l_bouton);

        for (const AppTrouvee& app : apps)
        {
            ImGui::PushID(app.paquet.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(app.nom.c_str());
            infobulle_app(app);
            ImGui::TableNextColumn();
            if (petit_bouton(texte_bouton, -FLT_MIN))
                action = {type, app.paquet, app.nom};
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ================== Sécurité (logiciels espions) ==================

const ImVec4 COULEUR_DANGER  = ImVec4(0.90f, 0.45f, 0.45f, 1.0f);
const ImVec4 COULEUR_ALERTE  = ImVec4(0.95f, 0.70f, 0.35f, 1.0f);
const ImVec4 COULEUR_OK      = ImVec4(0.45f, 0.75f, 0.50f, 1.0f);

// Ligne discrète : état de la base d'indicateurs + lien de mise à jour. Renvoie true si clic.
bool ligne_base_indicateurs(bool maj_en_cours, const std::string& message_maj)
{
    bool clic = false;
    if (maj_en_cours)
        ImGui::TextDisabled("Mise à jour de la base anti-espions...");
    else
    {
        const size_t n = securite::nombre_indicateurs();
        if (n == 0)
            ImGui::TextColored(COULEUR_ALERTE, "Base anti-espions absente");
        else
        {
            const std::string date = securite::date_mise_a_jour();
            const std::string texte = "Base anti-espions : " + std::to_string(n) + " indicateurs" +
                                      (date.empty() ? "" : " · " + date);
            ImGui::TextDisabled("%s", texte.c_str());
        }
        ImGui::SameLine();
        clic = ImGui::TextLink(n == 0 ? "Télécharger" : "Mettre à jour");
        ImGui::SetItemTooltip("Indicateurs de compromission du projet MVT\n"
                              "(Amnesty International, Echap, Citizen Lab, Google...)");
    }
    if (!maj_en_cours)
    {
        const size_t n_uad = uad::nombre();
        if (n_uad == 0)
            ImGui::TextColored(COULEUR_ALERTE, "Liste de debloat UAD-ng absente : clique sur « Mettre à jour » ci-dessus");
        else
            ImGui::TextDisabled("Liste de debloat UAD-ng : %zu applis", n_uad);
    }
    if (!message_maj.empty())
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", message_maj.c_str());
        ImGui::PopTextWrapPos();
    }
    return clic;
}

// Liste d'applis avec Désactiver / Désinstaller ; `decrire` dessine la colonne nom
template <typename T, typename Fonction>
void table_securite(const char* id, const std::vector<T>& apps, Action& action, Fonction decrire)
{
    const float marge = ImGui::GetStyle().FramePadding.x;
    const float l_desactiver   = ImGui::CalcTextSize("Désactiver").x   + marge;
    const float l_desinstaller = ImGui::CalcTextSize("Désinstaller").x + marge;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 3));
    if (ImGui::BeginTable(id, 3, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("nom",          ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("desactiver",   ImGuiTableColumnFlags_WidthFixed, l_desactiver);
        ImGui::TableSetupColumn("desinstaller", ImGuiTableColumnFlags_WidthFixed, l_desinstaller);

        for (const T& app : apps)
        {
            ImGui::PushID(app.paquet.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            const std::string nom = decrire(app);

            ImGui::TableNextColumn();
            if (petit_bouton("Désactiver", -FLT_MIN))
                action = {DESACTIVER, app.paquet, nom};
            ImGui::TableNextColumn();
            if (petit_bouton("Désinstaller", -FLT_MIN, true))
                action = {DESINSTALLER, app.paquet, nom};
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// Section « Sécurité » : logiciels espions connus, accès sensibles, réglages affaiblis
void afficher_securite(ImFont* police_grasse, const RapportSecurite& r, Action& action)
{
    // --- Logiciels espions connus ---
    if (!r.detections.empty() || !r.traces.empty())
    {
        titre_section(police_grasse, "Logiciels espions détectés",
                      (int)(r.detections.size() + r.traces.size()), COULEUR_DANGER);

        // Applis : on peut les désactiver / désinstaller
        table_securite("##espions", r.detections, action, [](const Detection& d)
        {
            ImGui::TextColored(COULEUR_DANGER, "%s", d.menace.c_str());
            ImGui::SetItemTooltip("Paquet : %s\nCorrespondance : %s\nSource : %s",
                                  d.paquet.c_str(), d.preuve.c_str(), d.source.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", d.paquet.c_str());
            return d.menace;
        });

        // Traces (processus, fichiers, propriétés) : rien à désinstaller, on informe
        for (const Trace& t : r.traces)
        {
            ImGui::TextColored(COULEUR_DANGER, "%s", t.menace.c_str());
            ImGui::SetItemTooltip("Correspondance : %s\nSource : %s", t.preuve.c_str(), t.source.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s : %s", t.preuve.c_str(), t.element.c_str());
        }
        if (!r.traces.empty())
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("Ces traces ne se suppriment pas d'ici : fais analyser le téléphone "
                                "(MVT, ligne d'aide Amnesty Tech ou Access Now).");
            ImGui::PopTextWrapPos();
        }
    }
    else
    {
        titre_section(police_grasse, "Sécurité", 0);
        if (r.indicateurs_verifies > 0)
            ImGui::TextColored(COULEUR_OK, "Aucun logiciel espion connu détecté");
        else
            ImGui::TextColored(COULEUR_ALERTE, "Télécharge la base anti-espions pour lancer la recherche");
    }

    // --- Réglages affaiblis ---
    for (const std::string& alerte : r.alertes)
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(COULEUR_ALERTE, "• %s", alerte.c_str());
        ImGui::PopTextWrapPos();
    }

    // --- Applis tierces avec des accès sensibles ---
    if (!r.a_surveiller.empty())
    {
        titre_section(police_grasse, "Accès sensibles à vérifier", (int)r.a_surveiller.size());
        table_securite("##sensibles", r.a_surveiller, action, [](const AppSensible& a)
        {
            const std::string nom = nom_de(a.paquet);
            ImGui::TextUnformatted(nom.c_str());

            std::string acces, aide;
            auto ajouter = [&](bool actif, const char* etiquette, const char* explication)
            {
                if (!actif) return;
                acces += (acces.empty() ? "" : " · ") + std::string(etiquette);
                aide  += std::string("\n• ") + explication;
            };
            ajouter(a.accessibilite,  "accessibilité",  "Accessibilité : peut lire l'écran et agir à ta place");
            ajouter(a.administrateur, "administrateur", "Administrateur : retire-lui ce droit avant de la désinstaller "
                                                        "(Paramètres > Sécurité > Applis d'administration)");
            ajouter(a.notifications,  "notifications",  "Notifications : lit tous tes messages et codes reçus");
            ImGui::SetItemTooltip("%s\nPas forcément malveillante : vérifie que tu l'as installée toi-même.%s",
                                  a.paquet.c_str(), aide.c_str());

            ImGui::SameLine();
            ImGui::TextDisabled("%s", acces.c_str());
            if (a.hors_magasin)
            {
                ImGui::SameLine();
                ImGui::TextColored(COULEUR_ALERTE, "· hors magasin");
                ImGui::SetItemTooltip("Installée depuis un fichier APK ou par ADB, "
                                      "comme le sont souvent les logiciels espions.");
            }
            return nom;
        });
    }
}

// ================== Photos des modèles ==================

// Un modèle = une photo dans "photo de telephone/" + des mots-clés pour le reconnaître
struct ModeleTel
{
    const char* nom;      // affiché dans le menu déroulant
    const char* fichier;  // BMP 32 bits (avec transparence)
    std::vector<const char*> cles; // comparés au modèle / nom commercial (sans majuscules)
};

// Le premier est le modèle par défaut (utilisé si aucun autre ne correspond)
const std::vector<ModeleTel> CATALOGUE = {
    {"Ulefone Armor 22",   "telephone.bmp",  {"armor 22"}},
    {"Google Pixel 6",     "pixel_6.bmp",    {"pixel 6"}},
    {"Google Pixel 11",    "pixel_11.bmp",   {"pixel 11"}},
    {"Samsung Galaxy S26", "galaxy_s26.bmp", {"galaxy s26"}},
};


// "pixel 6" doit reconnaître "Pixel 6" et "Pixel 6a", mais pas "Pixel 60"
bool contient_cle(const std::string& texte, const std::string& cle)
{
    size_t pos = texte.find(cle);
    while (pos != std::string::npos)
    {
        const size_t fin = pos + cle.size();
        if (fin >= texte.size() || !std::isdigit((unsigned char)texte[fin]))
            return true;
        pos = texte.find(cle, pos + 1);
    }
    return false;
}

// Index du modèle correspondant dans CATALOGUE, ou 0 (Ulefone) par défaut
int trouver_modele(const InfosAppareil& infos)
{
    const std::string texte = minuscules(infos.fabricant + " " + infos.modele + " " + infos.nom_commercial);
    for (size_t i = 0; i < CATALOGUE.size(); ++i)
        for (const char* cle : CATALOGUE[i].cles)
            if (contient_cle(texte, cle))
                return (int)i;
    return 0;
}

struct Photo
{
    GLuint texture = 0;
    int l = 0, h = 0;
};

// Charge un BMP 32 bits en texture OpenGL (texture = 0 si échec)
Photo charger_photo(const std::string& chemin)
{
    Photo photo;
    SDL_Surface* brute = SDL_LoadBMP(chemin.c_str());
    if (!brute)
    {
        std::cerr << "Image introuvable : " << chemin << std::endl;
        return photo;
    }
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(brute, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(brute);
    if (!rgba)
        return photo;

    photo.l = rgba->w;
    photo.h = rgba->h;
    glGenTextures(1, &photo.texture);
    glBindTexture(GL_TEXTURE_2D, photo.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rgba->pitch / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, photo.l, photo.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    SDL_FreeSurface(rgba);
    return photo;
}

int main()
{
    //Variable de base
    std::string IP_Cible{""}; //IP du telephone
    int Port{0};

    std::clog << "Démarrage du programme !" << std::endl;

    bool adb_ok = ADB_VERIF();// on verifie que ADB est installé

    // État de la connexion (modifié par un thread pour ne pas figer la fenêtre)
    enum Etat { DECONNECTE, CONNEXION_EN_COURS, CONNECTE };
    std::atomic<int> etat{DECONNECTE};
    std::mutex       verrou;        // protège les variables ci-dessous
    InfosAppareil    infos;         // résultat de detection_info
    std::string      message_erreur;
    std::string      message_action;  // résultat de la dernière action (désactiver, ...)
    std::string      cible_connectee; // "IP:Port" de l'appareil connecté
    std::thread      travail;
    std::atomic<bool> action_en_cours{false};
    Action           a_confirmer;     // désinstallation en attente de confirmation

    // Base d'indicateurs de compromission (logiciels espions) et liste de debloat UAD-ng
    securite::charger_indicateurs();
    uad::charger();
    std::thread       thread_maj;           // téléchargement de la base
    std::atomic<bool> maj_en_cours{false};
    std::string       message_maj;          // résultat du dernier téléchargement (protégé par verrou)

    // Télécharge la base, la recharge, puis relance l'analyse si un téléphone est connecté
    auto lancer_mise_a_jour = [&]()
    {
        if (maj_en_cours)
            return;
        if (thread_maj.joinable())
            thread_maj.join();
        maj_en_cours = true;
        thread_maj = std::thread([&]()
        {
            std::string message, message_uad;
            securite::mettre_a_jour(message);
            securite::charger_indicateurs();
            uad::mettre_a_jour(message_uad);
            uad::charger();
            message += " " + message_uad;

            std::string cible;
            {
                std::lock_guard<std::mutex> garde(verrou);
                message_maj = message;
                cible = cible_connectee;
            }
            if (etat == CONNECTE && !action_en_cours)
            {
                InfosAppareil nouvelles = detection_info(cible);
                std::lock_guard<std::mutex> garde(verrou);
                if (nouvelles.erreur.empty() && etat == CONNECTE)
                    infos = nouvelles;
            }
            maj_en_cours = false;
        });
    };

    //initialisation du GUI

    // --- Fenêtre SDL2 + contexte OpenGL ---
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        std::cerr << "Erreur SDL : " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_Window* fenetre = SDL_CreateWindow("lache mon android !",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1100, 640,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_SetWindowMinimumSize(fenetre, 900, 500);

    // Icône de la fenêtre (logo.bmp, à côté de l'exécutable)
    {
        std::string chemin_logo = "logo.bmp";
        if (char* base = SDL_GetBasePath())
        {
            chemin_logo = std::string(base) + chemin_logo;
            SDL_free(base);
        }
        if (SDL_Surface* icone = SDL_LoadBMP(chemin_logo.c_str()))
        {
            SDL_SetWindowIcon(fenetre, icone);
            SDL_FreeSurface(icone);
        }
        else
            std::cerr << "Logo introuvable : " << chemin_logo << std::endl;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(fenetre);
    SDL_GL_MakeCurrent(fenetre, gl_context);
    SDL_GL_SetSwapInterval(1); // VSync

    // --- ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // pas de fichier imgui.ini

    // Police sobre (Noto Sans) si disponible, sinon police par défaut
    if (!io.Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", 18.0f))
        io.Fonts->AddFontDefault();
    // Police grasse pour les titres de section (nullptr si absente : police normale)
    ImFont* police_grasse = io.Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf", 18.0f);

    // Style minimaliste, inspiré de Fossify / IronFox : sombre, plat, arrondi, un seul accent
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowPadding    = ImVec2(24, 24);
    style.FramePadding     = ImVec2(12, 9);
    style.ItemSpacing      = ImVec2(10, 12);
    style.WindowRounding   = 0.0f;
    style.ChildRounding    = 12.0f;
    style.FrameRounding    = 8.0f;
    style.PopupRounding    = 12.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 0.0f;
    style.FrameBorderSize  = 0.0f;

    const ImVec4 fond       = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
    const ImVec4 surface    = ImVec4(0.16f, 0.16f, 0.17f, 1.0f);
    const ImVec4 champ      = ImVec4(0.21f, 0.21f, 0.23f, 1.0f);
    const ImVec4 champ_hov  = ImVec4(0.25f, 0.25f, 0.27f, 1.0f);
    const ImVec4 accent     = ImVec4(0.40f, 0.60f, 0.95f, 1.0f);
    const ImVec4 accent_hov = ImVec4(0.48f, 0.67f, 1.00f, 1.0f);
    const ImVec4 texte      = ImVec4(0.92f, 0.92f, 0.93f, 1.0f);
    const ImVec4 texte_sec  = ImVec4(0.60f, 0.60f, 0.63f, 1.0f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]       = fond;
    c[ImGuiCol_ChildBg]        = surface;
    c[ImGuiCol_Text]           = texte;
    c[ImGuiCol_TextDisabled]   = texte_sec;
    c[ImGuiCol_FrameBg]        = champ;
    c[ImGuiCol_FrameBgHovered] = champ_hov;
    c[ImGuiCol_FrameBgActive]  = champ_hov;
    c[ImGuiCol_Button]         = accent;
    c[ImGuiCol_ButtonHovered]  = accent_hov;
    c[ImGuiCol_ButtonActive]   = accent;
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.40f, 0.60f, 0.95f, 0.35f);
    c[ImGuiCol_NavHighlight]   = accent;
    c[ImGuiCol_Separator]      = champ;
    c[ImGuiCol_Header]         = champ;      // éléments sélectionnés (listes, menus)
    c[ImGuiCol_HeaderHovered]  = champ_hov;
    c[ImGuiCol_HeaderActive]   = champ_hov;
    c[ImGuiCol_PopupBg]        = surface;

    ImGui_ImplSDL2_InitForOpenGL(fenetre, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    // --- Chargement des photos de tous les modèles du catalogue ---
    std::string dossier_photos = "photo de telephone/";
    if (char* base = SDL_GetBasePath()) // dossier de l'exécutable
    {
        dossier_photos = std::string(base) + dossier_photos;
        SDL_free(base);
    }
    std::vector<Photo> photos;
    for (const ModeleTel& m : CATALOGUE)
        photos.push_back(charger_photo(dossier_photos + m.fichier));

    int modele_choisi = -1; // -1 = automatique (détecté), sinon index dans CATALOGUE

    // --- Boucle principale ---
    bool en_cours = true;
    while (en_cours)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                en_cours = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Une seule fenêtre ImGui qui occupe toute la fenêtre SDL
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("principal", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        const ImVec2 dispo = ImGui::GetContentRegionAvail();
        const float  gouttiere = style.ItemSpacing.x * 2.0f;
        const float  largeur_gauche = (dispo.x - gouttiere) * 0.4f; // 40 % image, 60 % panneau
        const ImGuiWindowFlags flags_panneau = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        // ---------- Panneau gauche : image du téléphone ----------
        ImGui::BeginChild("gauche", ImVec2(largeur_gauche, dispo.y), ImGuiChildFlags_None, flags_panneau);
        {
            // Modèle détecté (Ulefone par défaut tant qu'aucun téléphone n'est connecté)
            int modele_detecte = 0;
            if (etat == CONNECTE)
            {
                std::lock_guard<std::mutex> garde(verrou);
                modele_detecte = trouver_modele(infos);
            }
            const int modele = (modele_choisi >= 0) ? modele_choisi : modele_detecte;
            const Photo& photo = photos[modele];

            // Place réservée en bas pour le menu déroulant
            const ImVec2 zone = ImGui::GetContentRegionAvail();
            const float hauteur_menu = ImGui::GetFrameHeight();
            const float marge = 24.0f;

            if (photo.texture)
            {
                // Mise à l'échelle en gardant les proportions, centrée, sans déborder
                float echelle = std::min((zone.x - 2 * marge) / photo.l,
                                         (zone.y - hauteur_menu - 2 * marge) / photo.h);
                if (echelle < 0.0f) echelle = 0.0f;
                const ImVec2 taille(photo.l * echelle, photo.h * echelle);
                ImGui::SetCursorPos(ImVec2((zone.x - taille.x) * 0.5f,
                                           (zone.y - hauteur_menu - taille.y) * 0.5f));
                ImGui::Image((ImTextureID)(intptr_t)photo.texture, taille);
            }
            else
                ImGui::TextDisabled("Image du téléphone introuvable");

            // Menu déroulant discret, centré sous le téléphone (si ce n'est pas le bon modèle)
            const float largeur_menu = std::min(240.0f, zone.x - 2 * marge);
            ImGui::SetCursorPos(ImVec2((zone.x - largeur_menu) * 0.5f, zone.y - hauteur_menu - 4.0f));
            ImGui::SetNextItemWidth(largeur_menu);
            ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, champ);
            ImGui::PushStyleColor(ImGuiCol_Button,         ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  champ);
            ImGui::PushStyleColor(ImGuiCol_Text,           texte_sec);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
            const std::string apercu = CATALOGUE[modele].nom;
            if (ImGui::BeginCombo("##modele", apercu.c_str(), ImGuiComboFlags_HeightRegular))
            {
                ImGui::PopStyleColor(); // texte normal dans la liste
                const std::string automatique = std::string("Automatique (") + CATALOGUE[modele_detecte].nom + ")";
                if (ImGui::Selectable(automatique.c_str(), modele_choisi < 0))
                    modele_choisi = -1;
                for (int i = 0; i < (int)CATALOGUE.size(); ++i)
                    if (ImGui::Selectable(CATALOGUE[i].nom, modele_choisi == i))
                        modele_choisi = i;
                ImGui::EndCombo();
                ImGui::PushStyleColor(ImGuiCol_Text, texte_sec);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(5);
            ImGui::SetItemTooltip("Pas le bon modèle ? Choisis-le ici.");
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, gouttiere);

        // ---------- Panneau droit ----------
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 28));
        ImGui::BeginChild("droite", ImVec2(0, dispo.y), ImGuiChildFlags_AlwaysUseWindowPadding, flags_panneau);
        if (etat == CONNECTE)
        {
            // ===== Informations sur l'appareil =====
            // Copie locale pour ne pas bloquer le thread pendant l'affichage
            InfosAppareil vue;
            std::string   msg_action, cible, msg_maj;
            {
                std::lock_guard<std::mutex> garde(verrou);
                vue        = infos;
                msg_action = message_action;
                cible      = cible_connectee;
                msg_maj    = message_maj;
            }

            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextUnformatted((vue.fabricant + " " + vue.modele).c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextDisabled("Android %s  ·  %s", vue.version_android.c_str(), cible.c_str());

            ImGui::Dummy(ImVec2(0, 4));
            ImGui::Separator();

            // Zone défilante pour les listes ; message + bouton restent en bas
            Action action;
            const bool occupe = action_en_cours || maj_en_cours;
            const float hauteur_bas = ImGui::GetFrameHeight() + ImGui::GetTextLineHeight() + style.ItemSpacing.y * 2;
            ImGui::BeginChild("listes", ImVec2(0, -hauteur_bas), ImGuiChildFlags_None);
            ImGui::BeginDisabled(occupe);
            {
                afficher_securite(police_grasse, vue.securite, action);
                if (ligne_base_indicateurs(maj_en_cours, msg_maj))
                    lancer_mise_a_jour();

                titre_section(police_grasse, "Bloatwares", (int)vue.bloatwares.size());
                afficher_liste_apps("##bloat", vue.bloatwares, action);

                // Niveaux UAD-ng plus risqués : repliés par défaut, cachés s'ils sont vides
                if (!vue.avancees.empty() &&
                    titre_repliable(police_grasse, "Suppression avancée", (int)vue.avancees.size(), "avancees"))
                {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("Peut retirer une fonction utile. Lis la description (survol) avant.");
                    ImGui::PopTextWrapPos();
                    afficher_liste_apps("##avancees", vue.avancees, action);
                }
                if (!vue.expertes.empty() &&
                    titre_repliable(police_grasse, "Suppression experte", (int)vue.expertes.size(), "expertes"))
                {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextColored(COULEUR_ALERTE, "Peut casser une partie du système. "
                                       "Préfère « Désactiver » : c'est réversible depuis « Applications désactivées ».");
                    ImGui::PopTextWrapPos();
                    afficher_liste_apps("##expertes", vue.expertes, action);
                }

                titre_section(police_grasse, "Applications problématiques", (int)vue.problematiques.size());
                afficher_liste_apps("##prob", vue.problematiques, action);

                titre_section(police_grasse, "Applications désactivées", (int)vue.desactivees.size());
                afficher_liste_restauration("##desact", vue.desactivees, "Réactiver", REACTIVER, action);

                titre_section(police_grasse, "Applications désinstallées", (int)vue.desinstallees.size());
                afficher_liste_restauration("##desinst", vue.desinstallees, "Réinstaller", REINSTALLER, action);
            }
            ImGui::EndDisabled();
            ImGui::EndChild();

            // Ligne d'état de la dernière action
            if (occupe)
                ImGui::TextDisabled("En cours...");
            else
                ImGui::TextDisabled("%s", msg_action.c_str());

            ImGui::BeginDisabled(occupe);
            if (ImGui::Button("Se déconnecter", ImVec2(-FLT_MIN, 0)))
            {
                executer("adb disconnect " + cible);
                {
                    std::lock_guard<std::mutex> garde(verrou);
                    message_action.clear();
                }
                etat = DECONNECTE;
            }
            ImGui::EndDisabled();

            // La désinstallation supprime les données : on demande confirmation
            if (action.type == DESINSTALLER)
            {
                a_confirmer = action;
                action = Action();
                ImGui::OpenPopup("Confirmer");
            }
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(420, 0));
            if (ImGui::BeginPopupModal("Confirmer", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
            {
                if (police_grasse) ImGui::PushFont(police_grasse);
                ImGui::Text("Désinstaller %s ?", a_confirmer.nom.c_str());
                if (police_grasse) ImGui::PopFont();
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("Les données de l'application seront supprimées. "
                                    "Tu pourras la réinstaller depuis « Applications désinstallées ».");
                if (const AppTrouvee app = creer_app(a_confirmer.paquet); !app.requis_par.empty())
                    ImGui::TextColored(COULEUR_ALERTE, "Attention, nécessaire à : %s", app.requis_par.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Dummy(ImVec2(0, 4));

                const float l = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f;
                if (petit_bouton("Annuler", l))
                    ImGui::CloseCurrentPopup();
                ImGui::SameLine();
                if (petit_bouton("Désinstaller", l, true))
                {
                    action = a_confirmer;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // Exécution de l'action dans un thread, puis nouvelle détection
            if (action.type != AUCUNE && !occupe && paquet_valide(action.paquet))
            {
                std::string commande;
                const char* verbe = "";
                switch (action.type)
                {
                    case DESACTIVER:   commande = "shell pm disable-user --user 0 ";       verbe = "désactivé";   break;
                    case DESINSTALLER: commande = "shell pm uninstall --user 0 ";          verbe = "désinstallé"; break;
                    case REACTIVER:    commande = "shell pm enable --user 0 ";             verbe = "réactivé";    break;
                    case REINSTALLER:  commande = "shell cmd package install-existing ";   verbe = "réinstallé";  break;
                    default: break;
                }
                commande = "adb -s " + cible + " " + commande + action.paquet;

                if (travail.joinable())
                    travail.join();
                action_en_cours = true;
                travail = std::thread([&, commande, action, verbe, cible]()
                {
                    std::string sortie = nettoyer(executer(commande));
                    std::clog << "[ACTION] " << commande << " -> " << sortie << std::endl;

                    // pm renvoie "Success", "Package ... new state: disabled-user", "installed for user: 0", ...
                    const bool ok = sortie.find("Exception") == std::string::npos &&
                                    sortie.find("Failure")   == std::string::npos &&
                                    sortie.find("Error")     == std::string::npos &&
                                    sortie.find("error")     == std::string::npos;

                    InfosAppareil nouvelles = detection_info(cible);
                    {
                        std::lock_guard<std::mutex> garde(verrou);
                        if (nouvelles.erreur.empty())
                            infos = nouvelles;
                        message_action = ok ? action.nom + " : " + verbe
                                            : action.nom + " : échec (" + sortie + ")";
                    }
                    action_en_cours = false;
                });
            }
        }
        else
        {
            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextUnformatted("Connexion ADB");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("Connecte ton téléphone via le débogage sans fil.");
            ImGui::PopTextWrapPos();

            ImGui::Dummy(ImVec2(0, 4));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 4));

            // Statut adb : texte discret vert / rouge
            const ImVec4 couleur_etat = adb_ok ? ImVec4(0.45f, 0.75f, 0.50f, 1.0f)
                                               : ImVec4(0.85f, 0.45f, 0.45f, 1.0f);
            ImGui::TextColored(couleur_etat, "%s", adb_ok ? "adb détecté" : "adb introuvable");
            if (!adb_ok)
                ImGui::TextDisabled("Installe-le : sudo apt install adb");

            {
                std::string msg_maj;
                {
                    std::lock_guard<std::mutex> garde(verrou);
                    msg_maj = message_maj;
                }
                if (ligne_base_indicateurs(maj_en_cours, msg_maj))
                    lancer_mise_a_jour();
            }

            ImGui::Dummy(ImVec2(0, 8));

            // Champs reliés directement aux variables IP_Cible et Port
            ImGui::TextDisabled("Adresse IP");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##ip", "192.168.1.42", &IP_Cible,
                                     ImGuiInputTextFlags_CharsNoBlank);

            ImGui::TextDisabled("Port");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##port", &Port, 0, 0))
            {
                if (Port < 0)     Port = 0;
                if (Port > 65535) Port = 65535;
            }

            ImGui::Dummy(ImVec2(0, 8));

            // Bouton de connexion : lance adb connect + detection_info dans un thread
            const bool en_connexion = (etat == CONNEXION_EN_COURS);
            ImGui::BeginDisabled(!adb_ok || IP_Cible.empty() || Port == 0 || en_connexion);
            if (ImGui::Button(en_connexion ? "Connexion..." : "Se connecter", ImVec2(-FLT_MIN, 0)))
            {
                if (!ip_valide(IP_Cible))
                {
                    std::lock_guard<std::mutex> garde(verrou);
                    message_erreur = "Adresse IP invalide.";
                }
                else
                {
                    if (travail.joinable())
                        travail.join();
                    {
                        std::lock_guard<std::mutex> garde(verrou);
                        message_erreur.clear();
                        cible_connectee = IP_Cible + ":" + std::to_string(Port);
                    }
                    etat = CONNEXION_EN_COURS;
                    const std::string cible = IP_Cible + ":" + std::to_string(Port);
                    travail = std::thread([&, cible]()
                    {
                        std::string erreur;
                        if (connexion_adb(cible, erreur))
                        {
                            InfosAppareil resultat = detection_info(cible);
                            std::lock_guard<std::mutex> garde(verrou);
                            if (resultat.erreur.empty())
                            {
                                infos = resultat;
                                etat = CONNECTE;
                                return;
                            }
                            message_erreur = resultat.erreur;
                        }
                        else
                        {
                            std::lock_guard<std::mutex> garde(verrou);
                            message_erreur = erreur;
                        }
                        etat = DECONNECTE;
                    });
                }
            }
            ImGui::EndDisabled();

            // Message d'erreur éventuel
            {
                std::lock_guard<std::mutex> garde(verrou);
                if (!message_erreur.empty())
                {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.45f, 1.0f), "%s", message_erreur.c_str());
                    ImGui::PopTextWrapPos();
                }
            }

            // Lien discret vers le tuto (pas encore fonctionnel), centré sous le bouton
            ImGui::Dummy(ImVec2(0, 2));
            const char* texte_tuto = "Lancer le tuto";
            const float largeur_tuto = ImGui::CalcTextSize(texte_tuto).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - largeur_tuto) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_TextLink, texte_sec);
            if (ImGui::TextLink(texte_tuto))
            {
                // TODO : lancer le tuto
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::End();

        // --- Rendu ---
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(fond.x, fond.y, fond.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(fenetre);
    }

    // --- Nettoyage ---
    if (travail.joinable())
        travail.join();
    if (thread_maj.joinable())
        thread_maj.join();
    for (Photo& photo : photos)
        if (photo.texture)
            glDeleteTextures(1, &photo.texture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(fenetre);
    SDL_Quit();

    return 0;
}
