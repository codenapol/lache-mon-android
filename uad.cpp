// Lache mon Android ! - Liste de debloat UAD-ng
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "uad.h"
#include "json.h"
#include "outils.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace fs = std::filesystem;

namespace
{

using Liste = std::unordered_map<std::string, EntreeUAD>;

std::mutex                   verrou_liste;
std::shared_ptr<const Liste> liste_actuelle = std::make_shared<Liste>();

std::shared_ptr<const Liste> liste()
{
    std::lock_guard<std::mutex> garde(verrou_liste);
    return liste_actuelle;
}

fs::path fichier_liste() { return dossier_programme() / "uad_lists.json"; }

const char* URL_LISTE = "https://raw.githubusercontent.com/Universal-Debloater-Alliance/"
                        "universal-android-debloater-next-generation/main/resources/assets/uad_lists.json";

NiveauUAD niveau_de(const std::string& texte)
{
    if (texte == "Recommended") return NiveauUAD::RECOMMANDE;
    if (texte == "Advanced")    return NiveauUAD::AVANCE;
    if (texte == "Expert")      return NiveauUAD::EXPERT;
    if (texte == "Unsafe")      return NiveauUAD::DANGEREUX;
    return NiveauUAD::INCONNU;
}

// Lit le JSON { "paquet": { "list", "description", "removal", "neededBy": [...] }, ... }
bool lire_liste(const std::string& contenu, Liste& sortie)
{
    Json racine;
    if (!LecteurJson(contenu).lire(racine) || racine.type != Json::OBJET)
        return false;

    for (const auto& [paquet, infos] : racine.objet)
    {
        if (infos.type != Json::OBJET)
            continue;
        EntreeUAD entree;
        entree.liste       = infos.texte_de("list");
        entree.description = nettoyer(infos.texte_de("description"));
        entree.niveau      = niveau_de(infos.texte_de("removal"));
        if (const Json* besoin = infos.champ("neededBy"); besoin && besoin->type == Json::TABLEAU)
            for (const Json& p : besoin->tableau)
                if (p.type == Json::TEXTE && !p.texte.empty())
                    entree.requis_par.push_back(p.texte);
        sortie.emplace(paquet, std::move(entree));
    }
    return true;
}

} // namespace

namespace uad
{

void charger()
{
    auto nouvelle = std::make_shared<Liste>();
    const std::string contenu = lire_fichier(fichier_liste());
    if (!contenu.empty() && !lire_liste(contenu, *nouvelle))
        std::cerr << "[UAD] Liste illisible : " << fichier_liste() << std::endl;

    std::clog << "[UAD] Liste chargée : " << nouvelle->size() << " paquets" << std::endl;
    std::lock_guard<std::mutex> garde(verrou_liste);
    liste_actuelle = nouvelle;
}

bool mettre_a_jour(std::string& message)
{
    std::error_code ec;
    fs::create_directories(dossier_programme(), ec);

    // Téléchargement à côté, puis vérification avant de remplacer l'ancienne liste
    const fs::path temporaire = fichier_liste().string() + ".nouveau";
    if (!telecharger(URL_LISTE, temporaire))
    {
        message = "Liste UAD-ng : téléchargement impossible.";
        return false;
    }
    Liste essai;
    if (!lire_liste(lire_fichier(temporaire), essai) || essai.empty())
    {
        fs::remove(temporaire, ec);
        message = "Liste UAD-ng : fichier reçu invalide.";
        return false;
    }
    fs::rename(temporaire, fichier_liste(), ec);
    message = "Liste UAD-ng : " + std::to_string(essai.size()) + " applis.";
    return !ec;
}

size_t nombre()
{
    return liste()->size();
}

std::optional<EntreeUAD> chercher(const std::string& paquet)
{
    const auto l = liste();
    if (auto it = l->find(paquet); it != l->end())
        return it->second;
    return std::nullopt;
}

} // namespace uad
