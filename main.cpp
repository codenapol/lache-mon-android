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
#include <unistd.h> // access

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_image.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h" // InputText avec std::string

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "adb.h"            // adb embarqué, connexion, commandes sur le téléphone
#include "alternatives.h" // alternatives libres : catalogue, version et logo
#include "analyse.h"        // analyse du téléphone (InfosAppareil, detection_info)
#include "catalogue_apps.h" // applications connues (nom_de, alternative_de)
#include "decouverte.h"   // découverte automatique du port du débogage sans fil
#include "rapport.h"      // rapports d'analyse (PDF ou HTML)
#include "rapport_analyse.h" // contenu du rapport basique
#include "outils.h"   // executer, nettoyer, paquet_valide
#include "securite.h" // détection de logiciels espions
#include "uad.h"      // liste de debloat UAD-ng
#include "usb.h"      // téléphones branchés en USB

// Action demandée par un clic dans une liste
enum TypeAction { AUCUNE, DESACTIVER, DESINSTALLER, REACTIVER, REINSTALLER, REMPLACER };
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

// Avertissement en haut des résultats quand l'analyse n'a pas pu aller jusqu'au bout
void avertissement_incomplet(const InfosAppareil& infos)
{
    if (!infos.incomplet)
        return;
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.35f, 1.0f), "Analyse incomplète : %s.", infos.raison_incomplet.c_str());
    ImGui::TextDisabled("Les listes ci-dessous peuvent être fausses ou partielles. "
                        "Déconnecte-toi puis reconnecte le téléphone pour relancer l'analyse.");
    ImGui::PopTextWrapPos();
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

// Petit bouton à droite d'un en-tête (titre + ligne discrète), centré sur ses deux lignes :
// « Générer un rapport » une fois connecté, « Connexion USB » avant.
// `haut_entete` = position Y du début de l'en-tête.
bool bouton_entete(const char* texte, float haut_entete)
{
    const ImVec2 suite = ImGui::GetCursorPos(); // position après l'en-tête, rétablie à la fin
    const ImGuiStyle& s = ImGui::GetStyle();
    const float largeur = ImGui::CalcTextSize(texte).x + s.FramePadding.x * 2.0f;
    const float hauteur_entete = suite.y - s.ItemSpacing.y - haut_entete;
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - largeur,
                               haut_entete + (hauteur_entete - ImGui::GetFrameHeight()) * 0.5f));
    const bool clic = petit_bouton(texte, largeur);
    ImGui::SetCursorPos(suite);
    return clic;
}

// Fenêtre d'erreur modale (même style que la confirmation de désinstallation)
void fenetre_erreur(ImFont* police_grasse, const char* id, const std::string& titre, const std::string& message)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0));
    if (!ImGui::BeginPopupModal(id, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        return;
    if (police_grasse) ImGui::PushFont(police_grasse);
    ImGui::TextUnformatted(titre.c_str());
    if (police_grasse) ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", message.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 4));
    if (petit_bouton("Fermer", -FLT_MIN))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
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
                    action = {REMPLACER, app.paquet, app.nom};
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
// Fenêtre « Générer un rapport » : type (basique, complet bientôt) et format (PDF, HTML)
struct ChoixRapport
{
    int         format = 0;      // 0 = PDF (par défaut), 1 = HTML
    std::string message;         // résultat de la dernière génération
    bool        reussi = false;
};

void fenetre_rapport(ImFont* police_grasse, const InfosAppareil& infos, ChoixRapport& choix)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 0));
    if (!ImGui::BeginPopupModal("Rapport", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        return;

    if (police_grasse) ImGui::PushFont(police_grasse);
    ImGui::TextUnformatted("Générer un rapport");
    if (police_grasse) ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);

    // --- Type de rapport ---
    int type = 0; // seul le rapport basique existe pour l'instant
    ImGui::RadioButton("Basique", &type, 0);
    ImGui::Indent();
    ImGui::TextDisabled("Résumé de l'analyse : appareil, logiciels espions, accès sensibles, "
                        "réglages, nombre d'applis, et les limites de l'analyse.");
    ImGui::Unindent();

    ImGui::BeginDisabled();
    ImGui::RadioButton("Complet", &type, 1);
    ImGui::EndDisabled();
    ImGui::Indent();
    ImGui::TextDisabled("Pour une analyse par un expert, ou pour servir de pièce à conviction en justice. "
                        "Bientôt disponible.");
    ImGui::Unindent();

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();

    // --- Format ---
    ImGui::TextDisabled("Format");
    ImGui::RadioButton("PDF", &choix.format, 0);
    ImGui::SameLine(0.0f, 24.0f);
    ImGui::RadioButton("HTML", &choix.format, 1);
    std::string dossier = rapport::dossier_documents().string(); // affiché avec « ~ » pour le dossier personnel
    if (const char* home = std::getenv("HOME"); home && *home && dossier.rfind(home, 0) == 0)
        dossier = "~" + dossier.substr(std::string(home).size());
    ImGui::TextDisabled("Enregistré dans %s", dossier.c_str());

    if (infos.incomplet)
        ImGui::TextColored(COULEUR_ALERTE, "L'analyse est incomplète : le rapport le signalera.");
    if (!choix.message.empty())
        ImGui::TextColored(choix.reussi ? COULEUR_OK : COULEUR_ALERTE, "%s", choix.message.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 4));

    // --- Boutons ---
    const float l = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (petit_bouton(choix.reussi ? "Fermer" : "Annuler", l))
        ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (petit_bouton("Générer", l))
    {
        std::filesystem::path chemin;
        std::string erreur;
        const FormatRapport format = choix.format == 0 ? FormatRapport::PDF : FormatRapport::HTML;
        choix.reussi = rapport::enregistrer(rapport_basique(infos), format, nom_fichier_rapport(infos), chemin, erreur);
        // Le dossier est affiché au-dessus : le message ne donne que le nom du fichier
        choix.message = choix.reussi ? "Rapport enregistré : " + chemin.filename().string() : erreur;
        std::clog << "[RAPPORT] " << (choix.reussi ? "Rapport enregistré : " + chemin.string() : erreur) << std::endl;
    }
    ImGui::EndPopup();
}

// Fenêtre « Connexion USB » : liste des téléphones branchés (mise à jour par un thread),
// l'utilisateur choisit le sien. Renvoie true quand il demande la connexion : `choisi`
// contient alors le numéro de série, déjà vérifié par AppareilUsb::connectable.
bool fenetre_usb(ImFont* police_grasse, const ListeUsb& liste, bool liste_prete, std::string& choisi)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 0));
    if (!ImGui::BeginPopupModal("Choix USB", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        return false;

    const ImGuiStyle& style = ImGui::GetStyle();
    if (police_grasse) ImGui::PushFont(police_grasse);
    ImGui::TextUnformatted("Connexion USB");
    if (police_grasse) ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Branche le téléphone par câble, active « Débogage USB » dans les options "
                        "pour les développeurs, puis choisis-le.");
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 2));

    // Le choix doit toujours désigner un appareil prêt de la liste ; s'il n'y en a qu'un, il est choisi d'office
    const AppareilUsb* selection = nullptr;
    int prets = 0;
    const AppareilUsb* seul_pret = nullptr;
    for (const AppareilUsb& a : liste.appareils)
    {
        if (!a.connectable())
            continue;
        ++prets;
        seul_pret = &a;
        if (a.serie == choisi)
            selection = &a;
    }
    if (!selection)
        selection = prets == 1 ? seul_pret : nullptr;
    choisi = selection ? selection->serie : "";

    // --- Liste (deux lignes par appareil : nom, puis numéro de série) ---
    bool connecter = false;
    const float hauteur_ligne = ImGui::GetTextLineHeight() * 2.0f + style.FramePadding.y * 2.0f;
    // Trois lignes si la fenêtre de l'appli est assez haute, sinon ce qui reste (au moins une ligne et demie)
    const float hauteur_aide = ImGui::GetTextLineHeightWithSpacing() * 4.0f;
    const float reste = ImGui::GetMainViewport()->Size.y - 40.0f            // marge autour de la fenêtre
                      - (ImGui::GetCursorPosY() + style.WindowPadding.y)   // titre et description
                      - hauteur_aide - ImGui::GetTextLineHeightWithSpacing()
                      - ImGui::GetFrameHeightWithSpacing() - 4.0f - style.ItemSpacing.y * 3.0f;
    const float hauteur_liste = std::max(hauteur_ligne * 1.5f + style.WindowPadding.y * 2.0f,
        std::min(reste, hauteur_ligne * 3.0f + style.ItemSpacing.y * 2.0f + style.WindowPadding.y * 2.0f));
    // Fond de la liste = couleur des champs : la ligne choisie prend l'accent bleu, le survol s'éclaircit
    ImGui::PushStyleColor(ImGuiCol_ChildBg,       style.Colors[ImGuiCol_FrameBg]);
    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.40f, 0.60f, 0.95f, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.29f, 0.29f, 0.31f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.40f, 0.60f, 0.95f, 0.40f));
    ImGui::BeginChild("##appareils_usb", ImVec2(0, hauteur_liste), ImGuiChildFlags_Borders);
    if (liste.appareils.empty())
    {
        const char* texte = liste_prete ? "Aucun téléphone détecté" : "Recherche des téléphones...";
        const ImVec2 taille = ImGui::CalcTextSize(texte);
        const ImVec2 zone = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - taille.x) * 0.5f,
                                   ImGui::GetCursorPosY() + (zone.y - taille.y) * 0.5f));
        ImGui::TextDisabled("%s", texte);
    }
    for (const AppareilUsb& a : liste.appareils)
    {
        const bool pret = a.connectable();
        ImGui::PushID(a.serie.c_str());
        ImGui::BeginDisabled(!pret);
        if (ImGui::Selectable("##appareil", selection == &a, ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(0, hauteur_ligne)))
        {
            choisi = a.serie;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                connecter = true;
        }
        ImGui::EndDisabled();

        // Texte dessiné par-dessus la ligne (sans ajouter d'élément qui déplacerait le curseur)
        const ImVec2 haut  = ImGui::GetItemRectMin();
        const ImVec2 bas   = ImGui::GetItemRectMax();
        const float marge  = style.FramePadding.x;
        const float y1     = haut.y + style.FramePadding.y;
        const float y2     = y1 + ImGui::GetTextLineHeight();
        ImDrawList* dessin = ImGui::GetWindowDrawList();
        const ImU32 couleur_nom   = ImGui::GetColorU32(pret ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        const ImU32 couleur_serie = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const std::string etat    = usb::etat_court(a);
        const ImU32 couleur_etat  = ImGui::GetColorU32(pret ? COULEUR_OK : COULEUR_ALERTE);
        const float largeur_etat  = ImGui::CalcTextSize(etat.c_str()).x;
        dessin->PushClipRect(haut, ImVec2(bas.x - largeur_etat - marge * 2.0f, bas.y), true);
        dessin->AddText(ImVec2(haut.x + marge, y1), couleur_nom, a.nom().c_str());
        dessin->AddText(ImVec2(haut.x + marge, y2), couleur_serie, a.serie.c_str());
        dessin->PopClipRect();
        dessin->AddText(ImVec2(bas.x - marge - largeur_etat, haut.y + (hauteur_ligne - ImGui::GetTextLineHeight()) * 0.5f),
                        couleur_etat, etat.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(5);

    // --- Aide : erreur d'adb, conseils pour les appareils pas prêts, ou liste vide ---
    // Hauteur fixe : la fenêtre ne change pas de taille (et les boutons ne bougent pas sous la souris)
    // quand la liste se met à jour
    ImGui::BeginChild("##aide_usb", ImVec2(0, hauteur_aide));
    ImGui::PushTextWrapPos(0.0f);
    if (!liste.erreur.empty())
        ImGui::TextColored(COULEUR_ALERTE, "adb ne répond pas : %s", liste.erreur.c_str());
    std::vector<std::string> conseils;
    for (const AppareilUsb& a : liste.appareils)
    {
        const std::string c = usb::conseil(a);
        if (!c.empty() && std::find(conseils.begin(), conseils.end(), c) == conseils.end())
            conseils.push_back(c);
    }
    for (const std::string& c : conseils)
        ImGui::TextColored(COULEUR_ALERTE, "%s", c.c_str());
    if (liste_prete && liste.appareils.empty() && liste.erreur.empty())
        ImGui::TextDisabled("Vérifie que le câble transmet les données (certains ne font que charger) "
                            "et que le téléphone est déverrouillé.");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::TextDisabled("La liste se met à jour toute seule.");
    ImGui::Dummy(ImVec2(0, 4));

    // --- Boutons ---
    const float l = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f;
    if (petit_bouton("Annuler", l))
    {
        choisi.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(choisi.empty());
    if (petit_bouton("Se connecter", l))
        connecter = true;
    ImGui::EndDisabled();

    connecter = connecter && !choisi.empty();
    if (connecter)
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return connecter;
}

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

// Crée une texture OpenGL à partir de pixels RGBA (`pixels_par_ligne` : largeur réelle d'une ligne en mémoire)
Photo texture_rgba(const void* pixels, int l, int h, int pixels_par_ligne)
{
    Photo photo;
    photo.l = l;
    photo.h = h;
    glGenTextures(1, &photo.texture);
    glBindTexture(GL_TEXTURE_2D, photo.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pixels_par_ligne);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, l, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    return photo;
}

// Charge un BMP 32 bits en texture OpenGL (texture = 0 si échec)
Photo charger_photo(const std::string& chemin)
{
    SDL_Surface* brute = SDL_LoadBMP(chemin.c_str());
    if (!brute)
    {
        std::cerr << "Image introuvable : " << chemin << std::endl;
        return Photo();
    }
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(brute, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(brute);
    if (!rgba)
        return Photo();

    const Photo photo = texture_rgba(rgba->pixels, rgba->w, rgba->h, rgba->pitch / 4);
    SDL_FreeSurface(rgba);
    return photo;
}

// ================== Fenêtre « Remplacer » ==================

// Recherche de version et de logo lancée dans un thread : `infos` n'est lu qu'une fois `finie` à true
struct RechercheAlternative
{
    std::atomic<bool> finie{false};
    InfosAlternative infos;
};

// Logo carré : celui de l'appli s'il est chargé, sinon la première lettre du nom sur fond gris
void logo_alternative(const Photo& logo, const char* nom, float taille)
{
    if (logo.texture)
    {
        ImGui::Image((ImTextureID)(intptr_t)logo.texture, ImVec2(taille, taille));
        return;
    }
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(taille, taille));
    ImDrawList* dessin = ImGui::GetWindowDrawList();
    dessin->AddRectFilled(p, ImVec2(p.x + taille, p.y + taille),
                          ImGui::GetColorU32(ImGuiCol_FrameBg), taille * 0.22f);
    const char lettre[2] = {nom[0], 0};
    const float echelle = 2.0f;
    const ImVec2 t = ImGui::CalcTextSize(lettre);
    dessin->AddText(ImGui::GetFont(), ImGui::GetFontSize() * echelle,
                    ImVec2(p.x + (taille - t.x * echelle) * 0.5f, p.y + (taille - t.y * echelle) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), lettre);
}

int main()
{
    //Variable de base
    std::string IP_Cible{""}; //IP du telephone
    int Port{0};

    std::clog << "Démarrage du programme !" << std::endl;

    utiliser_adb_embarque();
    bool adb_ok = ADB_VERIF();// on verifie que ADB est installé

    // État de la connexion (modifié par un thread pour ne pas figer la fenêtre)
    enum Etat { DECONNECTE, CONNEXION_EN_COURS, CONNECTE };
    std::atomic<int> etat{DECONNECTE};
    std::mutex       verrou;        // protège les variables ci-dessous
    InfosAppareil    infos;         // résultat de detection_info
    std::string      message_erreur;
    std::string      message_action;  // résultat de la dernière action (désactiver, ...)
    std::string      cible_connectee; // "IP:Port" (sans fil) ou numéro de série (USB) de l'appareil connecté
    bool             connexion_usb = false; // l'appareil connecté l'est par câble (protégé par verrou)
    std::thread      travail;
    std::atomic<bool> action_en_cours{false};
    Action           a_confirmer;     // désinstallation en attente de confirmation
    ChoixRapport     choix_rapport;   // fenêtre « Générer un rapport »

    // Découverte automatique du port (thread pour ne pas figer la fenêtre)
    std::thread        thread_decouverte;
    std::atomic<bool>  decouverte_en_cours{false};
    bool               decouverte_finie = false; // résultat prêt à lire (protégé par verrou)
    ResultatDecouverte resultat_decouverte;      // protégé par verrou
    std::string        titre_erreur_decouverte, message_erreur_decouverte; // fenêtre d'erreur

    auto lancer_decouverte = [&](const std::string& ip)
    {
        if (decouverte_en_cours)
            return;
        if (thread_decouverte.joinable())
            thread_decouverte.join();
        decouverte_en_cours = true;
        thread_decouverte = std::thread([&, ip]()
        {
            ResultatDecouverte r = decouverte::trouver_port(ip);
            {
                std::lock_guard<std::mutex> garde(verrou);
                resultat_decouverte = std::move(r);
                decouverte_finie = true;
            }
            decouverte_en_cours = false;
        });
    };

    // Connexion par câble : la liste des téléphones est relue chaque seconde tant que la fenêtre
    // « Connexion USB » est ouverte (un téléphone branché ou autorisé apparaît tout seul)
    std::thread       thread_usb;
    std::atomic<bool> usb_actif{false};
    ListeUsb          liste_usb;              // protégé par verrou
    bool              liste_usb_prete = false; // première lecture faite (protégé par verrou)
    std::string       usb_choisi;             // numéro de série choisi dans la fenêtre

    auto demarrer_liste_usb = [&]()
    {
        usb_actif = false;
        if (thread_usb.joinable())
            thread_usb.join();
        {
            std::lock_guard<std::mutex> garde(verrou);
            liste_usb = ListeUsb();
            liste_usb_prete = false;
        }
        usb_choisi.clear();
        usb_actif = true;
        thread_usb = std::thread([&]()
        {
            while (usb_actif)
            {
                ListeUsb liste = usb::lister();
                if (!usb_actif)
                    break;
                {
                    std::lock_guard<std::mutex> garde(verrou);
                    liste_usb = std::move(liste);
                    liste_usb_prete = true;
                }
                for (int i = 0; i < 10 && usb_actif; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    };

    // Connexion à `cible` dans un thread : adb connect (sans fil seulement), puis analyse
    auto lancer_connexion = [&](const std::string& cible, bool cable)
    {
        if (travail.joinable())
            travail.join();
        {
            std::lock_guard<std::mutex> garde(verrou);
            message_erreur.clear();
            cible_connectee = cible;
            connexion_usb   = cable;
        }
        etat = CONNEXION_EN_COURS;
        travail = std::thread([&, cible, cable]()
        {
            std::string erreur;
            if (cable || connexion_adb(cible, erreur))
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
    };

    // Fenêtre « Remplacer »
    Action            a_remplacer;      // appli à remplacer (paquet + nom)
    std::map<std::string, std::shared_ptr<RechercheAlternative>> recherches; // par paquet d'alternative
    std::vector<std::thread> threads_recherche; // recherches de version et de logo (attendues à la fermeture)
    std::map<std::string, Photo> logos;        // logos déjà chargés, par paquet d'alternative
    std::thread       thread_telephone;        // commandes "am start" (ouvrir F-Droid, une page, une appli)
    std::atomic<bool> telephone_en_cours{false};
    std::string       message_remplacer;       // résultat de la dernière commande (protégé par verrou)
    std::thread       thread_suivi;            // surveille les installations pendant que la fenêtre est ouverte
    std::atomic<bool> suivi_actif{false};

    // Relit les paquets actifs toutes les 2 s : la fenêtre voit apparaître F-Droid ou l'alternative
    // dès que l'utilisateur les installe sur le téléphone
    auto demarrer_suivi = [&](const std::string& cible)
    {
        if (thread_suivi.joinable())
            thread_suivi.join();
        suivi_actif = true;
        thread_suivi = std::thread([&, cible]()
        {
            while (suivi_actif)
            {
                std::set<std::string> actifs = lister_paquets("adb -s " + cible + " shell pm list packages -e");
                if (!actifs.empty() && suivi_actif)
                {
                    std::lock_guard<std::mutex> garde(verrou);
                    infos.client_fdroid = client_fdroid(actifs);
                    infos.actifs        = std::move(actifs);
                }
                for (int i = 0; i < 20 && suivi_actif; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    };

    // Exécute une commande sur le téléphone sans figer la fenêtre ; `succes` s'affiche si elle réussit
    auto lancer_sur_telephone = [&](const std::string& cible, const std::string& commande_shell,
                                    const std::string& succes)
    {
        if (telephone_en_cours)
            return;
        if (thread_telephone.joinable())
            thread_telephone.join();
        telephone_en_cours = true;
        thread_telephone = std::thread([&, cible, commande_shell, succes]()
        {
            const std::string erreur = commande_telephone(cible, commande_shell);
            {
                std::lock_guard<std::mutex> garde(verrou);
                message_remplacer = erreur.empty() ? succes : "Échec : " + erreur;
            }
            telephone_en_cours = false;
        });
    };

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

    // Formats d'image des logos d'alternatives, chargés une fois ici (avant les threads qui les décodent).
    // BMP, GIF, ICO et SVG sont toujours disponibles ; un format absent est simplement ignoré.
    int formats_image = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP | IMG_INIT_TIF;
#if SDL_IMAGE_VERSION_ATLEAST(2, 6, 0)
    formats_image |= IMG_INIT_AVIF | IMG_INIT_JXL;
#endif
    if ((IMG_Init(formats_image) & (IMG_INIT_PNG | IMG_INIT_WEBP)) != (IMG_INIT_PNG | IMG_INIT_WEBP))
        std::cerr << "SDL_image : PNG ou WebP indisponible (" << IMG_GetError() << ")" << std::endl;

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

    // Police sobre (Noto Sans) : d'abord à côté de l'exécutable (AppImage), sinon celle du système,
    // sinon police par défaut d'ImGui
    auto charger_police = [&](const char* fichier) -> ImFont*
    {
        for (const std::string& dossier : std::vector<std::string>{dossier_executable(), std::string("/usr/share/fonts/truetype/noto/")})
        {
            const std::string chemin = dossier + fichier;
            if (!dossier.empty() && access(chemin.c_str(), R_OK) == 0)
                return io.Fonts->AddFontFromFileTTF(chemin.c_str(), 18.0f);
        }
        return nullptr;
    };
    if (!charger_police("NotoSans-Regular.ttf"))
        io.Fonts->AddFontDefault();
    // Police grasse pour les titres de section (nullptr si absente : police normale)
    ImFont* police_grasse = charger_police("NotoSans-Bold.ttf");

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
            bool          par_usb;
            {
                std::lock_guard<std::mutex> garde(verrou);
                vue        = infos;
                msg_action = message_action;
                cible      = cible_connectee;
                msg_maj    = message_maj;
                par_usb    = connexion_usb;
            }

            const float haut_entete = ImGui::GetCursorPosY();
            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextUnformatted((vue.fabricant + " " + vue.modele).c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextDisabled("Android %s  ·  %s", vue.version_android.c_str(), par_usb ? "USB" : cible.c_str());
            if (bouton_entete("Générer un rapport", haut_entete))
            {
                choix_rapport.message.clear();
                choix_rapport.reussi = false;
                ImGui::OpenPopup("Rapport");
            }
            fenetre_rapport(police_grasse, vue, choix_rapport);
            avertissement_incomplet(vue);

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
                suivi_actif = false;
                if (!par_usb) // en USB, il suffit de débrancher le câble
                    executer("adb disconnect " + cible);
                {
                    std::lock_guard<std::mutex> garde(verrou);
                    message_action.clear();
                }
                etat = DECONNECTE;
            }
            ImGui::EndDisabled();

            // ===== Fenêtre « Remplacer » : fiche de l'alternative, installation via F-Droid =====
            if (action.type == REMPLACER)
            {
                a_remplacer = action;
                action = Action();
                if (const Alternative* alt = fiche_alternative(alternative_de(a_remplacer.paquet)))
                {
                    // Version et logo (relancé si la recherche précédente a échoué)
                    std::shared_ptr<RechercheAlternative>& r = recherches[alt->paquet];
                    if (!r || (r->finie && (!r->infos.trouve || r->infos.icone.empty())))
                    {
                        r = std::make_shared<RechercheAlternative>();
                        threads_recherche.emplace_back([r, alt]()
                        {
                            r->infos = alternatives::chercher(*alt);
                            r->finie = true;
                        });
                    }
                    {
                        std::lock_guard<std::mutex> garde(verrou);
                        message_remplacer.clear();
                    }
                    demarrer_suivi(cible);
                    ImGui::OpenPopup("Remplacer");
                }
            }
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(480, 0));
            bool fenetre_remplacer = false;
            if (ImGui::BeginPopupModal("Remplacer", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
            {
                fenetre_remplacer = true;
                const Alternative* alt = fiche_alternative(alternative_de(a_remplacer.paquet));
                if (!alt) // ne devrait pas arriver : le bouton n'existe que pour les applis avec une fiche
                {
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                else
                {
                    // Recherche terminée : création du logo (une seule fois)
                    const std::shared_ptr<RechercheAlternative> recherche = recherches[alt->paquet];
                    const bool recherche_finie = recherche && recherche->finie;
                    if (recherche_finie && !recherche->infos.icone.empty() && !logos.count(alt->paquet))
                    {
                        const InfosAlternative& f = recherche->infos;
                        logos[alt->paquet] = texture_rgba(f.icone.data(), f.largeur, f.hauteur, f.largeur);
                    }

                    std::string message;
                    bool alternative_installee, fdroid_installe;
                    std::string client;
                    {
                        std::lock_guard<std::mutex> garde(verrou);
                        message               = message_remplacer;
                        alternative_installee = infos.actifs.count(alt->paquet) > 0;
                        client                = infos.client_fdroid;
                        fdroid_installe       = !client.empty();
                    }

                    // --- En-tête : logo, nom, version, description ---
                    const float taille_logo = 64.0f;
                    const auto logo = logos.find(alt->paquet);
                    logo_alternative(logo != logos.end() ? logo->second : Photo(), alt->nom, taille_logo);
                    ImGui::SameLine(0.0f, 16.0f);
                    ImGui::BeginGroup();
                    ImGui::SetWindowFontScale(1.35f);
                    ImGui::TextUnformatted(alt->nom);
                    ImGui::SetWindowFontScale(1.0f);
                    const char* source = alt->page_officielle ? "site officiel" : "F-Droid";
                    if (!recherche_finie)
                        ImGui::TextDisabled("Recherche de la dernière version...");
                    else if (recherche->infos.trouve)
                        ImGui::TextDisabled("Version %s · %s", recherche->infos.version.c_str(), source);
                    else
                        ImGui::TextDisabled("Version inconnue (pas de connexion ?) · %s", source);
                    ImGui::EndGroup();

                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextUnformatted(alt->description);
                    ImGui::TextDisabled("Pour remplacer %s", a_remplacer.nom.c_str());
                    ImGui::PopTextWrapPos();

                    ImGui::Dummy(ImVec2(0, 2));
                    ImGui::Separator();

                    // --- Étape 1 : installer l'alternative ---
                    if (police_grasse) ImGui::PushFont(police_grasse);
                    ImGui::Text("1. Installer %s", alt->nom);
                    if (police_grasse) ImGui::PopFont();

                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::BeginDisabled(telephone_en_cours);
                    const std::string paquet_alt = alt->paquet;
                    if (alternative_installee)
                    {
                        ImGui::TextColored(COULEUR_OK, "%s est installée sur le téléphone.", alt->nom);
                        if (ImGui::Button("Ouvrir sur le téléphone", ImVec2(-FLT_MIN, 0)))
                            lancer_sur_telephone(cible,
                                "monkey -p " + paquet_alt + " -c android.intent.category.LAUNCHER 1",
                                std::string(alt->nom) + " est ouverte sur le téléphone.");
                    }
                    else if (alt->page_officielle)
                    {
                        ImGui::TextDisabled("Cette appli n'est pas sur F-Droid : elle se télécharge "
                                            "depuis son site officiel, qui s'ouvre sur le téléphone.");
                        if (ImGui::Button("Ouvrir le site officiel", ImVec2(-FLT_MIN, 0)))
                            lancer_sur_telephone(cible,
                                "am start -a android.intent.action.VIEW -d '" + std::string(alt->page_officielle) + "'",
                                "Site ouvert sur le téléphone.");
                    }
                    else if (fdroid_installe && paquet_valide(client))
                    {
                        if (ImGui::Button("Ouvrir dans F-Droid", ImVec2(-FLT_MIN, 0)))
                            lancer_sur_telephone(cible,
                                "am start -a android.intent.action.VIEW -d 'market://details?id=" + paquet_alt +
                                "' -p " + client,
                                "Fiche ouverte dans F-Droid : appuie sur « Installer » sur le téléphone.");
                        ImGui::TextDisabled("F-Droid installe l'appli et la tient à jour.");
                    }
                    else
                    {
                        ImGui::TextColored(COULEUR_ALERTE, "F-Droid n'est pas installé.");
                        ImGui::TextDisabled("F-Droid est le magasin d'applis libres : il installe %s "
                                            "et la tient à jour.", alt->nom);
                        if (ImGui::Button("Installer F-Droid", ImVec2(-FLT_MIN, 0)))
                            lancer_sur_telephone(cible,
                                "am start -a android.intent.action.VIEW -d '" + std::string(URL_APK_FDROID) + "'",
                                "Téléchargement lancé : ouvre F-Droid.apk sur le téléphone et "
                                "autorise l'installation si Android le demande.");
                    }
                    ImGui::EndDisabled();

                    if (telephone_en_cours)
                        ImGui::TextDisabled("En cours...");
                    else if (!message.empty())
                        ImGui::TextDisabled("%s", message.c_str());
                    if (!alternative_installee)
                        ImGui::TextDisabled("Cette fenêtre se met à jour dès que l'installation est faite.");
                    ImGui::PopTextWrapPos();

                    ImGui::Dummy(ImVec2(0, 2));
                    ImGui::Separator();

                    // --- Étape 2 : retirer l'appli d'origine ---
                    if (police_grasse) ImGui::PushFont(police_grasse);
                    ImGui::Text("2. Retirer %s", a_remplacer.nom.c_str());
                    if (police_grasse) ImGui::PopFont();
                    if (!alternative_installee)
                    {
                        ImGui::PushTextWrapPos(0.0f);
                        ImGui::TextColored(COULEUR_ALERTE, "Installe d'abord %s pour ne pas rester sans appli.",
                                           alt->nom);
                        ImGui::PopTextWrapPos();
                    }

                    const float l = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f;
                    ImGui::BeginDisabled(occupe);
                    if (petit_bouton("Désactiver", l))
                    {
                        action = {DESACTIVER, a_remplacer.paquet, a_remplacer.nom};
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (petit_bouton("Désinstaller", l, true))
                    {
                        action = {DESINSTALLER, a_remplacer.paquet, a_remplacer.nom}; // confirmation ci-dessous
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndDisabled();

                    ImGui::Dummy(ImVec2(0, 2));
                    if (petit_bouton("Fermer", -FLT_MIN))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            }
            if (!fenetre_remplacer && suivi_actif)
                suivi_actif = false; // fenêtre fermée : plus besoin de surveiller les installations

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
            const float haut_entete = ImGui::GetCursorPosY();
            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextUnformatted("Connexion ADB");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("Connecte ton téléphone via le débogage sans fil.");
            ImGui::PopTextWrapPos();

            // Bouton « Connexion USB » à droite de l'en-tête : remplacé par « Générer un rapport »
            // une fois connecté (même emplacement)
            ImGui::BeginDisabled(!adb_ok || etat == CONNEXION_EN_COURS || decouverte_en_cours);
            if (bouton_entete("Connexion USB", haut_entete))
            {
                demarrer_liste_usb();
                ImGui::OpenPopup("Choix USB");
            }
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Connecte un téléphone branché par câble.");
            {
                ListeUsb liste;
                bool prete;
                {
                    std::lock_guard<std::mutex> garde(verrou);
                    liste = liste_usb;
                    prete = liste_usb_prete;
                }
                if (fenetre_usb(police_grasse, liste, prete, usb_choisi))
                {
                    std::clog << "[USB] Connexion à " << usb_choisi << std::endl;
                    lancer_connexion(usb_choisi, true);
                }
                if (usb_actif && !ImGui::IsPopupOpen("Choix USB"))
                    usb_actif = false; // fenêtre fermée : plus besoin de relire la liste
            }

            ImGui::Dummy(ImVec2(0, 4));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 4));

            // Statut adb : affiché seulement en cas de problème
            if (!adb_ok)
            {
                ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.45f, 1.0f), "adb introuvable");
                ImGui::TextDisabled("Installe-le : sudo apt install adb");
            }

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
            const char* texte_decouverte = "Découverte automatique";
            const float largeur_decouverte = ImGui::CalcTextSize(texte_decouverte).x + style.FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - largeur_decouverte - style.ItemSpacing.x);
            if (ImGui::InputInt("##port", &Port, 0, 0))
            {
                if (Port < 0)     Port = 0;
                if (Port > 65535) Port = 65535;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(IP_Cible.empty() || decouverte_en_cours || etat == CONNEXION_EN_COURS);
            if (petit_bouton(decouverte_en_cours ? "Recherche..." : texte_decouverte, largeur_decouverte))
            {
                if (decouverte::ipv4_valide(IP_Cible))
                    lancer_decouverte(IP_Cible);
                else
                {
                    titre_erreur_decouverte   = "Adresse IP invalide";
                    message_erreur_decouverte = "« " + IP_Cible + " » n'est pas une adresse IP valide. "
                        "Écris l'adresse affichée dans Débogage sans fil, sans le port (exemple : 192.168.1.42).";
                    ImGui::OpenPopup("Erreur découverte");
                }
            }
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Trouve le port du débogage sans fil à partir de l'adresse IP.");

            // Résultat de la découverte : port rempli, ou fenêtre d'erreur
            {
                std::lock_guard<std::mutex> garde(verrou);
                if (decouverte_finie)
                {
                    decouverte_finie = false;
                    if (resultat_decouverte.port > 0)
                        Port = resultat_decouverte.port;
                    else
                    {
                        titre_erreur_decouverte   = "Port introuvable";
                        message_erreur_decouverte = resultat_decouverte.erreur;
                        ImGui::OpenPopup("Erreur découverte");
                    }
                }
            }
            fenetre_erreur(police_grasse, "Erreur découverte", titre_erreur_decouverte, message_erreur_decouverte);

            ImGui::Dummy(ImVec2(0, 8));

            // Bouton de connexion : lance adb connect + detection_info dans un thread
            const bool en_connexion = (etat == CONNEXION_EN_COURS);
            ImGui::BeginDisabled(!adb_ok || IP_Cible.empty() || Port == 0 || en_connexion || decouverte_en_cours);
            if (ImGui::Button(en_connexion ? "Connexion..." : "Se connecter", ImVec2(-FLT_MIN, 0)))
            {
                if (!ip_valide(IP_Cible))
                {
                    std::lock_guard<std::mutex> garde(verrou);
                    message_erreur = "Adresse IP invalide.";
                }
                else
                    lancer_connexion(IP_Cible + ":" + std::to_string(Port), false);
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
    if (thread_decouverte.joinable())
        thread_decouverte.join();
    suivi_actif = false;
    if (thread_suivi.joinable())
        thread_suivi.join();
    usb_actif = false;
    if (thread_usb.joinable())
        thread_usb.join();
    if (thread_telephone.joinable())
        thread_telephone.join();
    for (std::thread& t : threads_recherche)
        if (t.joinable())
            t.join();
    for (Photo& photo : photos)
        if (photo.texture)
            glDeleteTextures(1, &photo.texture);
    for (auto& [paquet, logo] : logos)
        if (logo.texture)
            glDeleteTextures(1, &logo.texture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(fenetre);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
