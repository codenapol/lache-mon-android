// Lache mon Android ! - Rapports d'analyse (PDF ou HTML)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Un rapport est une suite de blocs (titres, paragraphes, puces...) indépendante du format.
// Il est ensuite écrit en PDF (généré ici, sans bibliothèque) ou en HTML (une seule page autonome).

#include <filesystem>
#include <string>
#include <vector>

enum class TypeBloc
{
    TITRE,        // titre du rapport
    SECTION,      // titre de section
    TEXTE,        // paragraphe
    PUCE,         // élément de liste
    DANGER,       // paragraphe en rouge (logiciel espion détecté)
    ALERTE,       // paragraphe en orange (réglage affaibli, analyse incomplète)
    OK,           // paragraphe en vert (rien de détecté)
    DISCRET,      // paragraphe en gris (précisions)
};

struct BlocRapport
{
    TypeBloc    type;
    std::string texte; // UTF-8
};

struct Rapport
{
    std::string titre;              // titre du document (métadonnées, onglet du navigateur)
    std::vector<BlocRapport> blocs;

    void ajouter(TypeBloc type, const std::string& texte) { blocs.push_back({type, texte}); }
};

enum class FormatRapport { PDF, HTML };

namespace rapport
{
    std::string en_html(const Rapport& r);
    std::string en_pdf(const Rapport& r);

    // Dossier Documents de l'utilisateur (XDG_DOCUMENTS_DIR, sinon ~/Documents)
    std::filesystem::path dossier_documents();

    // Écrit le rapport dans le dossier Documents sous le nom `nom_base` + extension, sans écraser
    // un fichier existant (un numéro est ajouté). Renvoie false et remplit `erreur` en cas d'échec.
    bool enregistrer(const Rapport& r, FormatRapport format, const std::string& nom_base,
                     std::filesystem::path& chemin, std::string& erreur);
}
