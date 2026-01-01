// Lache mon Android ! - Liste de debloat UAD-ng
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Liste de debloat de UAD-ng (Universal Android Debloater Next Generation).
// Cette liste est sous licence GPL-3.0 : elle est téléchargée depuis le dépôt du projet
// et n'est jamais incluse dans ce programme.
// https://github.com/Universal-Debloater-Alliance/universal-android-debloater-next-generation

#include <optional>
#include <string>
#include <vector>

// Niveau de risque de la suppression, tel que défini par UAD-ng
enum class NiveauUAD
{
    RECOMMANDE, // sans risque
    AVANCE,     // peut retirer une fonction utile
    EXPERT,     // peut casser une partie du système
    DANGEREUX,  // "Unsafe" : peut empêcher le téléphone de démarrer (jamais proposé)
    INCONNU
};

struct EntreeUAD
{
    std::string liste;                    // "Google", "Oem", "Aosp", "Carrier", "Misc"
    std::string description;              // en anglais, rédigée par les contributeurs UAD-ng
    NiveauUAD   niveau = NiveauUAD::INCONNU;
    std::vector<std::string> requis_par;  // paquets qui ont besoin de celui-ci
};

namespace uad
{
    // (Re)charge la liste depuis le disque (~/.local/share/lache_mon_android/uad_lists.json)
    void charger();

    // Télécharge la dernière liste (bloquant : à lancer dans un thread)
    bool mettre_a_jour(std::string& message);

    // Nombre de paquets connus (0 = liste absente)
    size_t nombre();

    // Informations sur un paquet, s'il est dans la liste
    std::optional<EntreeUAD> chercher(const std::string& paquet);
}
