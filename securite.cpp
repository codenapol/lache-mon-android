// Lache mon Android ! - Détection de logiciels espions
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "securite.h"
#include "json.h"
#include "outils.h"

#include <openssl/evp.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace fs = std::filesystem;

namespace
{

// ================== Base d'indicateurs ==================

struct Origine
{
    std::string menace; // nom du logiciel espion
    std::string source; // collection d'indicateurs
};

struct Base
{
    std::unordered_map<std::string, Origine> paquets;      // "app:id"
    std::unordered_map<std::string, Origine> empreintes;   // "file:hashes.sha256" (minuscules)
    std::unordered_map<std::string, Origine> certificats;  // "app:cert.sha1" / "app:cert.sha256" (minuscules)
    std::unordered_map<std::string, Origine> processus;    // "process:name"
    std::unordered_map<std::string, Origine> proprietes;   // "android-property:name"
    std::unordered_map<std::string, Origine> chemins;      // "file:path"
    std::unordered_map<std::string, Origine> noms_fichiers;// "file:name"
    std::string date;                                      // dernière mise à jour

    size_t taille() const
    {
        return paquets.size() + empreintes.size() + certificats.size() + processus.size() +
               proprietes.size() + chemins.size() + noms_fichiers.size();
    }
};

std::mutex                  verrou_base;
std::shared_ptr<const Base> base_actuelle = std::make_shared<Base>();

std::shared_ptr<const Base> base()
{
    std::lock_guard<std::mutex> garde(verrou_base);
    return base_actuelle;
}

fs::path dossier_indicateurs() { return dossier_programme() / "indicateurs"; }
fs::path dossier_mvt()         { return dossier_donnees() / "mvt" / "indicators"; } // mvt-android download-iocs

// "[app:id='com.x']" ou "[file:hashes.'SHA-256' = 'abc']" → cle normalisée + valeur
bool decouper_motif(const std::string& motif, std::string& cle, std::string& valeur)
{
    std::string m = nettoyer(motif);
    if (m.size() < 2 || m.front() != '[' || m.back() != ']')
        return false;
    m = m.substr(1, m.size() - 2);

    // La valeur est entre les deux derniers guillemets simples
    const size_t fin = m.rfind('\'');
    if (fin == std::string::npos || fin == 0)
        return false;
    const size_t debut = m.rfind('\'', fin - 1);
    if (debut == std::string::npos)
        return false;
    valeur = m.substr(debut + 1, fin - debut - 1);

    std::string gauche = nettoyer(m.substr(0, debut));
    if (gauche.empty() || gauche.back() != '=')
        return false; // motif composé (AND / OR...) : ignoré
    gauche.pop_back();

    cle.clear();
    for (char ch : minuscules(nettoyer(gauche)))
        if (ch != '\'')
            cle += ch;
    if (cle.rfind("file:hashes.", 0) == 0) // "file:hashes.'SHA-256'" → "file:hashes.sha256"
        cle.erase(std::remove(cle.begin(), cle.end(), '-'), cle.end());
    if (cle.find(' ') != std::string::npos)
        return false;
    return !valeur.empty();
}

// Lit un fichier STIX2 et ajoute ses indicateurs à `base`
void charger_fichier(const fs::path& chemin, const std::string& source, Base& base)
{
    const std::string contenu = lire_fichier(chemin);
    Json racine;
    if (!LecteurJson(contenu).lire(racine))
    {
        std::cerr << "[SECURITE] Fichier illisible : " << chemin << std::endl;
        return;
    }
    const Json* objets = racine.champ("objects");
    if (!objets || objets->type != Json::TABLEAU)
        return;

    std::unordered_map<std::string, std::string> malwares; // id → nom
    std::unordered_map<std::string, std::string> liens;    // indicateur → malware
    std::vector<std::pair<std::string, std::string>> indicateurs; // (id, motif)

    for (const Json& o : objets->tableau)
    {
        const std::string type = o.texte_de("type");
        if (type == "malware")
            malwares[o.texte_de("id")] = o.texte_de("name");
        else if (type == "indicator")
            indicateurs.emplace_back(o.texte_de("id"), o.texte_de("pattern"));
        else if (type == "relationship" && o.texte_de("relationship_type") == "indicates")
            liens[o.texte_de("source_ref")] = o.texte_de("target_ref");
    }

    for (const auto& [id, motif] : indicateurs)
    {
        std::string cle, valeur;
        if (!decouper_motif(motif, cle, valeur))
            continue;

        Origine origine{"", source};
        if (auto l = liens.find(id); l != liens.end())
            if (auto m = malwares.find(l->second); m != malwares.end())
                origine.menace = m->second;
        if (origine.menace.empty() && malwares.size() == 1)
            origine.menace = malwares.begin()->second;
        if (origine.menace.empty())
            origine.menace = source;

        if (cle == "app:id")
            base.paquets.emplace(valeur, origine);
        else if (cle == "file:hashes.sha256")
            base.empreintes.emplace(minuscules(valeur), origine);
        else if (cle == "app:cert.sha1" || cle == "app:cert.sha256")
        {
            std::string empreinte = minuscules(valeur);
            empreinte.erase(std::remove(empreinte.begin(), empreinte.end(), ':'), empreinte.end());
            base.certificats.emplace(empreinte, origine);
        }
        else if (cle == "process:name")
            base.processus.emplace(valeur, origine);
        else if (cle == "android-property:name")
            base.proprietes.emplace(valeur, origine);
        else if (cle == "file:path")
            base.chemins.emplace(valeur, origine);
        else if (cle == "file:name")
            base.noms_fichiers.emplace(valeur, origine);
    }
}

// Collection listée dans indicators.yaml de MVT
struct Collection
{
    std::string nom, owner, repo, branch, path;
};

// Lecture ligne à ligne de indicators.yaml (format simple et stable)
std::vector<Collection> lire_liste_mvt(const std::string& yaml)
{
    std::vector<Collection> liste;
    Collection c;
    auto terminer = [&]()
    {
        if (!c.path.empty())
            liste.push_back(c);
        c = Collection();
    };

    std::istringstream flux(yaml);
    std::string ligne;
    while (std::getline(flux, ligne))
    {
        std::string l = nettoyer(ligne);
        if (l == "-") { terminer(); continue; }
        if (l.rfind("- ", 0) == 0)
            l = nettoyer(l.substr(2));
        const size_t deux_points = l.find(':');
        if (deux_points == std::string::npos)
            continue;
        const std::string cle = nettoyer(l.substr(0, deux_points));
        const std::string val = nettoyer(l.substr(deux_points + 1));
        if (cle == "type")        { terminer(); }
        else if (cle == "name")   c.nom = val;
        else if (cle == "owner")  c.owner = val;
        else if (cle == "repo")   c.repo = val;
        else if (cle == "branch") c.branch = val;
        else if (cle == "path")   c.path = val;
    }
    terminer();
    return liste;
}

// Les morceaux d'URL vont dans une commande shell : caractères sûrs uniquement
bool morceau_sur(const std::string& s)
{
    if (s.empty() || s.find("..") != std::string::npos)
        return false;
    for (char ch : s)
        if (!(std::isalnum((unsigned char)ch) || ch == '.' || ch == '_' || ch == '-' || ch == '/'))
            return false;
    return true;
}

// Paquets d'une liste "paquet/classe:paquet/classe" (réglages accessibilité, notifications)
std::vector<std::string> paquets_de_composants(const std::string& liste)
{
    std::vector<std::string> paquets;
    std::istringstream flux(liste);
    std::string composant;
    while (std::getline(flux, composant, ':'))
    {
        composant = nettoyer(composant);
        const std::string paquet = composant.substr(0, composant.find('/'));
        if (paquet_valide(paquet) && paquet != "null")
            paquets.push_back(paquet);
    }
    return paquets;
}

// Chemin d'APK inséré dans une commande shell : caractères sûrs uniquement
bool chemin_sur(const std::string& chemin)
{
    if (chemin.empty() || chemin[0] != '/')
        return false;
    for (char ch : chemin)
        if (!(std::isalnum((unsigned char)ch) || std::string("/._-~=+").find(ch) != std::string::npos))
            return false;
    return true;
}

// Nom de fichier (indicateur "file:name") inséré dans une commande find : caractères sûrs uniquement
bool nom_fichier_sur(const std::string& nom)
{
    if (nom.empty() || nom == "." || nom == "..")
        return false;
    for (char ch : nom)
        if (!(std::isalnum((unsigned char)ch) || ch == '.' || ch == '_' || ch == '-'))
            return false;
    return true;
}

// ================== Certificats de signature des APK ==================

uint32_t u32(const char* p)
{
    const auto* o = reinterpret_cast<const unsigned char*>(p);
    return o[0] | (o[1] << 8) | (o[2] << 16) | ((uint32_t)o[3] << 24);
}

uint64_t u64(const char* p)
{
    return u32(p) | ((uint64_t)u32(p + 4) << 32);
}

// Empreinte hexadécimale (minuscules) avec l'algorithme demandé (EVP_sha1, EVP_sha256)
std::string empreinte_hex(const std::string& donnees, const EVP_MD* algo)
{
    unsigned char resultat[EVP_MAX_MD_SIZE];
    unsigned int taille = 0;
    if (!EVP_Digest(donnees.data(), donnees.size(), resultat, &taille, algo, nullptr))
        return "";
    static const char* hexa = "0123456789abcdef";
    std::string texte;
    for (unsigned int k = 0; k < taille; ++k)
    {
        texte += hexa[resultat[k] >> 4];
        texte += hexa[resultat[k] & 0xF];
    }
    return texte;
}

// Lit `taille` octets à partir de `debut` dans un fichier du téléphone (dd par blocs de 4 Kio,
// sans transférer le reste du fichier)
std::string lire_morceau(const std::string& adb, const std::string& chemin, uint64_t debut, uint64_t taille)
{
    const uint64_t bloc = 4096;
    const uint64_t premier = debut / bloc;
    const uint64_t dernier = (debut + taille + bloc - 1) / bloc;
    const std::string brut = executer_binaire(adb + "exec-out \"dd if=" + chemin + " bs=4096 skip=" +
                                              std::to_string(premier) + " count=" +
                                              std::to_string(dernier - premier) + " 2>/dev/null\"");
    const uint64_t decalage = debut - premier * bloc;
    if (brut.size() < decalage + taille)
        return "";
    return brut.substr(decalage, taille);
}

// Bloc préfixé par sa longueur (uint32 little-endian), format du bloc de signature APK v2/v3
bool lire_prefixe(std::string_view& source, std::string_view& sortie)
{
    if (source.size() < 4)
        return false;
    const uint32_t n = u32(source.data());
    if (source.size() - 4 < n)
        return false;
    sortie = source.substr(4, n);
    source.remove_prefix(4 + n);
    return true;
}

// Certificats (DER) d'une signature v2 / v3 :
// signataires[ signataire[ données signées[ condensés, certificats[ cert, ... ], ... ], ... ] ]
void certificats_signature_v2(std::string_view valeur, std::vector<std::string>& certificats)
{
    std::string_view signataires;
    if (!lire_prefixe(valeur, signataires))
        return;
    while (!signataires.empty())
    {
        std::string_view signataire, donnees, condenses, liste, cert;
        if (!lire_prefixe(signataires, signataire) || !lire_prefixe(signataire, donnees) ||
            !lire_prefixe(donnees, condenses) || !lire_prefixe(donnees, liste))
            return;
        while (lire_prefixe(liste, cert))
            certificats.emplace_back(cert);
    }
}

// Certificats d'une signature v1 (fichier PKCS#7 META-INF/*.RSA, *.DSA ou *.EC)
void certificats_pkcs7(const std::string& der, std::vector<std::string>& certificats)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(der.data());
    PKCS7* p7 = d2i_PKCS7(nullptr, &p, (long)der.size());
    if (!p7)
        return;
    if (PKCS7_type_is_signed(p7) && p7->d.sign && p7->d.sign->cert)
        for (int k = 0; k < sk_X509_num(p7->d.sign->cert); ++k)
        {
            X509* x = sk_X509_value(p7->d.sign->cert, k);
            const int n = i2d_X509(x, nullptr);
            if (n <= 0)
                continue;
            std::string cert(n, '\0');
            unsigned char* q = reinterpret_cast<unsigned char*>(cert.data());
            i2d_X509(x, &q);
            certificats.push_back(std::move(cert));
        }
    PKCS7_free(p7);
}

// Empreintes SHA-1 et SHA-256 des certificats de signature d'un APK du téléphone.
// Ne transfère que la fin du fichier et le bloc de signature (quelques dizaines de Kio).
std::vector<std::string> empreintes_certificats(const std::string& adb, const std::string& chemin)
{
    std::vector<std::string> certificats;

    // 1. Fin du fichier ZIP : enregistrement EOCD → position du répertoire central
    const std::string fin = executer_binaire(adb + "exec-out \"tail -c 65557 " + chemin + "\"");
    size_t eocd = std::string::npos;
    for (size_t k = fin.size() >= 22 ? fin.size() - 22 : std::string::npos; k != std::string::npos; --k)
    {
        if (fin.compare(k, 4, "PK\x05\x06") == 0)
        {
            const uint16_t commentaire = (unsigned char)fin[k + 20] | ((unsigned char)fin[k + 21] << 8);
            if (k + 22 + commentaire == fin.size()) { eocd = k; break; }
        }
        if (k == 0) break;
    }

    if (eocd != std::string::npos)
    {
        const uint64_t repertoire = u32(fin.data() + eocd + 16);

        // 2. Bloc de signature APK (v2 / v3), juste avant le répertoire central
        uint64_t fenetre = std::min<uint64_t>(repertoire, 65536);
        std::string bloc = lire_morceau(adb, chemin, repertoire - fenetre, fenetre);
        if (bloc.size() >= 32 && bloc.compare(bloc.size() - 16, 16, "APK Sig Block 42") == 0)
        {
            const uint64_t taille = u64(bloc.data() + bloc.size() - 24); // sans le 1er champ taille
            const bool taille_ok = taille >= 24 && taille < (16u << 20) && taille + 8 <= repertoire;
            if (taille_ok && taille + 8 > bloc.size())
                bloc = lire_morceau(adb, chemin, repertoire - taille - 8, taille + 8);
            if (taille_ok && taille + 8 <= bloc.size())
            {
                std::string_view paires(bloc.data() + bloc.size() - taille - 8 + 8, taille - 24);
                while (paires.size() >= 12)
                {
                    const uint64_t longueur = u64(paires.data());
                    if (longueur < 4 || longueur > paires.size() - 8)
                        break;
                    const uint32_t id = u32(paires.data() + 8);
                    if (id == 0x7109871a || id == 0xf05368c0 || id == 0x1b93ad61) // v2, v3, v3.1
                        certificats_signature_v2(paires.substr(12, longueur - 4), certificats);
                    paires.remove_prefix(8 + longueur);
                }
            }
        }
    }

    // 3. Pas de signature v2/v3 : ancienne signature v1 (PKCS#7 dans META-INF)
    if (certificats.empty())
    {
        std::istringstream liste(executer(adb + "shell unzip -l " + chemin));
        std::string ligne;
        while (std::getline(liste, ligne))
        {
            std::istringstream mots(ligne);
            std::string mot, nom;
            while (mots >> mot)
                nom = mot; // dernier mot = nom du fichier
            const std::string ext = nom.size() > 4 ? nom.substr(nom.rfind('.') + 1) : "";
            if (nom.rfind("META-INF/", 0) == 0 && nom.find('/', 9) == std::string::npos &&
                (ext == "RSA" || ext == "DSA" || ext == "EC") && chemin_sur("/" + nom))
            {
                certificats_pkcs7(executer_binaire(adb + "exec-out \"unzip -p " + chemin + " " + nom + "\""),
                                  certificats);
                break;
            }
        }
    }

    std::vector<std::string> empreintes;
    for (const std::string& cert : certificats)
    {
        empreintes.push_back(empreinte_hex(cert, EVP_sha1()));
        empreintes.push_back(empreinte_hex(cert, EVP_sha256()));
    }
    std::sort(empreintes.begin(), empreintes.end());
    empreintes.erase(std::unique(empreintes.begin(), empreintes.end()), empreintes.end());
    return empreintes;
}

// Résultats déjà calculés, par chemin d'APK (le chemin change à chaque mise à jour de l'appli)
std::mutex verrou_cache;
std::map<std::string, std::string>              cache_empreintes;  // chemin → SHA-256 de l'APK
std::map<std::string, std::vector<std::string>> cache_certificats; // chemin → empreintes des certificats

} // namespace

namespace securite
{

void charger_indicateurs()
{
    auto nouvelle = std::make_shared<Base>();

    // Noms lisibles des collections (écrits lors de la mise à jour)
    std::map<std::string, std::string> noms;
    {
        std::istringstream flux(lire_fichier(dossier_indicateurs() / "sources.tsv"));
        std::string ligne;
        while (std::getline(flux, ligne))
            if (size_t tab = ligne.find('\t'); tab != std::string::npos)
                noms[ligne.substr(0, tab)] = ligne.substr(tab + 1);
    }
    nouvelle->date = nettoyer(lire_fichier(dossier_indicateurs() / "derniere_maj"));

    for (const fs::path& dossier : {dossier_indicateurs(), dossier_mvt()})
    {
        std::error_code ec;
        if (!fs::is_directory(dossier, ec))
            continue;
        std::vector<fs::path> fichiers;
        for (const auto& entree : fs::directory_iterator(dossier, ec))
            if (entree.path().extension() == ".stix2")
                fichiers.push_back(entree.path());
        std::sort(fichiers.begin(), fichiers.end());

        for (const fs::path& f : fichiers)
        {
            const auto nom = noms.find(f.filename().string());
            const std::string source = nom != noms.end() ? nom->second : f.stem().string();
            charger_fichier(f, source, *nouvelle);
        }
    }

    std::clog << "[SECURITE] Base chargée : " << nouvelle->paquets.size() << " paquets, "
              << nouvelle->empreintes.size() << " empreintes d'APK, "
              << nouvelle->certificats.size() << " certificats, "
              << nouvelle->processus.size() << " processus, "
              << nouvelle->proprietes.size() << " propriétés, "
              << nouvelle->chemins.size() + nouvelle->noms_fichiers.size() << " fichiers" << std::endl;

    std::lock_guard<std::mutex> garde(verrou_base);
    base_actuelle = nouvelle;
}

bool mettre_a_jour(std::string& message)
{
    const fs::path dossier = dossier_indicateurs();
    std::error_code ec;
    fs::create_directories(dossier, ec);
    if (ec)
    {
        message = "Impossible de créer " + dossier.string();
        return false;
    }

    const fs::path liste = dossier / "indicators.yaml";
    if (!telecharger("https://raw.githubusercontent.com/mvt-project/mvt-indicators/main/indicators.yaml", liste))
    {
        message = "Téléchargement impossible (connexion internet ? curl installé ?)";
        return false;
    }

    const std::vector<Collection> collections = lire_liste_mvt(lire_fichier(liste));
    int reussies = 0, total = 0;
    std::ofstream sources(dossier / "sources.tsv", std::ios::trunc);

    for (const Collection& c : collections)
    {
        if (!morceau_sur(c.owner) || !morceau_sur(c.repo) || !morceau_sur(c.branch) ||
            !morceau_sur(c.path) || fs::path(c.path).extension() != ".stix2")
            continue;
        ++total;

        std::string fichier = c.owner + "_" + c.repo + "_" + c.path;
        std::replace(fichier.begin(), fichier.end(), '/', '_');
        sources << fichier << '\t' << (c.nom.empty() ? fichier : c.nom) << '\n';

        const std::string url = "https://raw.githubusercontent.com/" + c.owner + "/" + c.repo + "/" +
                                c.branch + "/" + c.path;
        if (telecharger(url, dossier / fichier))
            ++reussies;
        else
            std::cerr << "[SECURITE] Échec : " << url << std::endl;
    }
    sources.close();

    if (reussies == 0)
    {
        message = "Aucune collection d'indicateurs téléchargée.";
        return false;
    }

    // Date du jour (JJ/MM/AAAA)
    char date[16];
    const std::time_t maintenant = std::time(nullptr);
    std::strftime(date, sizeof(date), "%d/%m/%Y", std::localtime(&maintenant));
    std::ofstream(dossier / "derniere_maj", std::ios::trunc) << date << '\n';

    message = std::to_string(reussies) + "/" + std::to_string(total) + " collections téléchargées.";
    return true;
}

size_t nombre_indicateurs()
{
    return base()->taille();
}

std::string date_mise_a_jour()
{
    return base()->date;
}

RapportSecurite analyser(const std::string& adb, const std::set<std::string>& installes)
{
    RapportSecurite rapport;
    const auto b = base();
    rapport.indicateurs_verifies = b->taille();
    std::set<std::string> deja_detectes;

    // --- 1. Noms de paquets connus comme logiciels espions ---
    for (const std::string& paquet : installes)
        if (auto it = b->paquets.find(paquet); it != b->paquets.end())
        {
            rapport.detections.push_back({paquet, it->second.menace, it->second.source, "nom du paquet"});
            deja_detectes.insert(paquet);
        }

    // --- 2. Empreintes SHA-256 des APK des applis tierces ---
    // "package:/data/app/~~xx==/com.x-yy==/base.apk=com.x"
    std::map<std::string, std::string> apk_vers_paquet;
    {
        std::istringstream flux(executer(adb + "shell pm list packages -3 -f"));
        std::string ligne;
        while (std::getline(flux, ligne))
        {
            ligne = nettoyer(ligne);
            if (ligne.rfind("package:", 0) != 0)
                continue;
            const size_t egal = ligne.rfind('=');
            if (egal == std::string::npos)
                continue;
            const std::string chemin = ligne.substr(8, egal - 8);
            const std::string paquet = ligne.substr(egal + 1);
            if (chemin_sur(chemin) && paquet_valide(paquet))
                apk_vers_paquet[chemin] = paquet;
        }
    }

    if (!b->empreintes.empty() && !apk_vers_paquet.empty())
    {
        // Empreintes pas encore calculées (calcul sur le téléphone, par lots)
        std::vector<std::string> a_calculer;
        {
            std::lock_guard<std::mutex> garde(verrou_cache);
            for (const auto& [chemin, paquet] : apk_vers_paquet)
                if (!cache_empreintes.count(chemin))
                    a_calculer.push_back(chemin);
        }
        for (size_t debut = 0; debut < a_calculer.size(); debut += 30) // longueur de commande limitée
        {
            std::string commande = adb + "shell sha256sum";
            for (size_t k = debut; k < std::min(debut + 30, a_calculer.size()); ++k)
                commande += " " + a_calculer[k];

            std::istringstream flux(executer(commande));
            std::string ligne;
            while (std::getline(flux, ligne))
            {
                std::istringstream mots(ligne);
                std::string empreinte, chemin;
                if ((mots >> empreinte >> chemin) && apk_vers_paquet.count(chemin))
                {
                    std::lock_guard<std::mutex> garde(verrou_cache);
                    cache_empreintes[chemin] = minuscules(empreinte);
                }
            }
        }

        for (const auto& [chemin, paquet] : apk_vers_paquet)
        {
            std::string empreinte;
            {
                std::lock_guard<std::mutex> garde(verrou_cache);
                auto it = cache_empreintes.find(chemin);
                if (it == cache_empreintes.end())
                    continue;
                empreinte = it->second;
            }
            const auto it = b->empreintes.find(empreinte);
            if (it != b->empreintes.end() && deja_detectes.insert(paquet).second)
                rapport.detections.push_back({paquet, it->second.menace, it->second.source,
                                              "empreinte SHA-256 de l'APK"});
        }
    }

    // --- 2 bis. Certificats de signature des APK des applis tierces ---
    // (un espion re-compilé change d'empreinte, mais garde souvent le même certificat)
    if (!b->certificats.empty())
        for (const auto& [chemin, paquet] : apk_vers_paquet)
        {
            std::vector<std::string> empreintes;
            {
                std::lock_guard<std::mutex> garde(verrou_cache);
                auto it = cache_certificats.find(chemin);
                if (it != cache_certificats.end())
                    empreintes = it->second;
            }
            if (empreintes.empty())
            {
                empreintes = empreintes_certificats(adb, chemin);
                std::lock_guard<std::mutex> garde(verrou_cache);
                if (!empreintes.empty())
                    cache_certificats[chemin] = empreintes;
            }
            for (const std::string& e : empreintes)
                if (auto it = b->certificats.find(e); it != b->certificats.end())
                {
                    if (deja_detectes.insert(paquet).second)
                        rapport.detections.push_back({paquet, it->second.menace, it->second.source,
                                                      "certificat de signature de l'APK"});
                    break;
                }
        }

    // --- 2 ter. Processus en cours ---
    if (!b->processus.empty())
    {
        std::istringstream flux(executer(adb + "shell ps -A -o NAME"));
        std::string ligne;
        std::set<std::string> deja;
        const size_t debut_traces = rapport.traces.size();
        while (std::getline(flux, ligne))
        {
            const std::string nom = nettoyer(ligne);
            if (nom.empty() || nom == "NAME")
                continue;
            // Nom complet, nom sans le chemin, nom sans ":service" (processus secondaires d'applis)
            const std::string base_nom = nom.substr(nom.rfind('/') == std::string::npos ? 0 : nom.rfind('/') + 1);
            const std::string sans_service = base_nom.substr(0, base_nom.find(':'));
            for (const std::string& candidat : {nom, base_nom, sans_service})
                if (auto it = b->processus.find(candidat); it != b->processus.end())
                {
                    if (deja.insert(candidat).second)
                        rapport.traces.push_back({candidat, it->second.menace, it->second.source, "processus en cours"});
                    break;
                }
            // Le noyau tronque les noms à 15 caractères : on compare alors le début
            if (base_nom.size() == 15)
                for (const auto& [ioc, origine] : b->processus)
                    if (ioc.size() > 15 && ioc.compare(0, 15, base_nom) == 0 && deja.insert(ioc).second)
                        rapport.traces.push_back({base_nom, origine.menace, origine.source,
                                                  "processus en cours (nom tronqué)"});
        }

        // Beaucoup de ces indicateurs viennent d'iOS : un nom identique à un programme du
        // système Android (ex. "gatekeeperd") n'est pas suspect, on l'écarte.
        // (le compte "shell" ne peut pas lire ces binaires, mais il peut lister leurs dossiers)
        if (rapport.traces.size() > debut_traces)
        {
            std::set<std::string> programmes_systeme;
            std::istringstream sortie(executer(adb + "shell \"ls /system/bin /system/xbin /system_ext/bin "
                                               "/product/bin /vendor/bin /vendor/bin/hw /odm/bin /apex/*/bin 2>/dev/null\""));
            while (std::getline(sortie, ligne))
                programmes_systeme.insert(nettoyer(ligne));
            rapport.traces.erase(std::remove_if(rapport.traces.begin() + debut_traces, rapport.traces.end(),
                [&](const Trace& t)
                {
                    if (!programmes_systeme.count(t.element))
                        return false;
                    std::clog << "[SECURITE] Ignoré (programme système Android) : " << t.element << std::endl;
                    return true;
                }), rapport.traces.end());
        }
    }

    // --- 2 quater. Propriétés système ---
    if (!b->proprietes.empty())
    {
        // "[nom]: [valeur]"
        std::istringstream flux(executer(adb + "shell getprop"));
        std::string ligne;
        while (std::getline(flux, ligne))
        {
            const size_t debut = ligne.find('['), fin = ligne.find(']');
            if (debut == std::string::npos || fin == std::string::npos || fin <= debut)
                continue;
            const std::string nom = ligne.substr(debut + 1, fin - debut - 1);
            if (auto it = b->proprietes.find(nom); it != b->proprietes.end())
                rapport.traces.push_back({nom, it->second.menace, it->second.source, "propriété système"});
        }
    }

    // --- 2 quinquies. Fichiers (seuls les dossiers lisibles sans root sont accessibles) ---
    if (!b->chemins.empty())
    {
        std::string commande = adb + "shell \"ls -d";
        size_t nombre = 0;
        for (const auto& [chemin, origine] : b->chemins)
            if (chemin_sur(chemin))
            {
                commande += " '" + chemin + "'";
                ++nombre;
            }
        commande += " 2>/dev/null\"";

        if (nombre > 0)
        {
            std::istringstream flux(executer(commande));
            std::string ligne;
            while (std::getline(flux, ligne))
            {
                const std::string chemin = nettoyer(ligne);
                if (auto it = b->chemins.find(chemin); it != b->chemins.end())
                    rapport.traces.push_back({chemin, it->second.menace, it->second.source, "fichier présent"});
            }
        }
    }
    if (!b->noms_fichiers.empty())
    {
        std::string commande = adb + "shell \"find /sdcard/ /data/local/tmp/ '('";
        bool premier = true;
        for (const auto& [nom, origine] : b->noms_fichiers)
            if (nom_fichier_sur(nom))
            {
                commande += std::string(premier ? "" : " -o") + " -name '" + nom + "'";
                premier = false;
            }
        commande += " ')' 2>/dev/null\"";

        if (!premier)
        {
            std::istringstream flux(executer(commande));
            std::string ligne;
            while (std::getline(flux, ligne))
            {
                const std::string chemin = nettoyer(ligne);
                const std::string nom = chemin.substr(chemin.rfind('/') + 1);
                if (auto it = b->noms_fichiers.find(nom); it != b->noms_fichiers.end() && chemin.find('/') == 0)
                    rapport.traces.push_back({chemin, it->second.menace, it->second.source, "fichier présent"});
            }
        }
    }

    // --- 3. Applis tierces avec des accès qui permettent d'espionner ---
    std::map<std::string, std::string> installeurs; // paquet tiers → installeur
    {
        // "package:com.x  installer=com.android.vending"
        std::istringstream flux(executer(adb + "shell pm list packages -3 -i"));
        std::string ligne;
        while (std::getline(flux, ligne))
        {
            std::istringstream mots(nettoyer(ligne));
            std::string paquet, installeur;
            mots >> paquet >> installeur;
            if (paquet.rfind("package:", 0) != 0)
                continue;
            paquet = paquet.substr(8);
            if (installeur.rfind("installer=", 0) == 0)
                installeur = installeur.substr(10);
            installeurs[paquet] = installeur;
        }
    }
    for (const auto& [chemin, paquet] : apk_vers_paquet)
        installeurs.emplace(paquet, "");

    std::map<std::string, AppSensible> sensibles;
    auto marquer = [&](const std::string& paquet, bool AppSensible::* acces)
    {
        if (!installeurs.count(paquet) || deja_detectes.count(paquet))
            return; // applis système exclues ; les applis déjà détectées sont listées plus haut
        AppSensible& app = sensibles[paquet];
        app.paquet = paquet;
        app.*acces = true;
    };

    for (const std::string& p : paquets_de_composants(executer(adb + "shell settings get secure enabled_accessibility_services")))
        marquer(p, &AppSensible::accessibilite);
    for (const std::string& p : paquets_de_composants(executer(adb + "shell settings get secure enabled_notification_listeners")))
        marquer(p, &AppSensible::notifications);

    // Administrateurs : lignes "com.x/.Recepteur:" sous "Enabled Device Admins"
    {
        std::istringstream flux(executer(adb + "shell dumpsys device_policy"));
        std::string ligne;
        bool dans_bloc = false;
        while (std::getline(flux, ligne))
        {
            const std::string l = nettoyer(ligne);
            if (l.rfind("Enabled Device Admins", 0) == 0) { dans_bloc = true; continue; }
            if (!dans_bloc)
                continue;
            if (l.empty()) { dans_bloc = false; continue; }
            const size_t barre = l.find('/');
            if (barre != std::string::npos && l.back() == ':')
                if (const std::string paquet = l.substr(0, barre); paquet_valide(paquet))
                    marquer(paquet, &AppSensible::administrateur);
        }
    }

    // Installée depuis un fichier APK ou par ADB (méthode habituelle des stalkerwares)
    const std::set<std::string> hors_magasin = {"", "null", "com.android.packageinstaller",
                                                "com.google.android.packageinstaller", "com.android.shell"};
    for (auto& [paquet, app] : sensibles)
    {
        app.hors_magasin = hors_magasin.count(installeurs[paquet]) > 0;
        rapport.a_surveiller.push_back(app);
    }
    // Les plus suspectes d'abord : hors magasin, puis nombre d'accès
    auto score = [](const AppSensible& a)
    { return a.hors_magasin * 4 + a.accessibilite + a.administrateur + a.notifications; };
    std::stable_sort(rapport.a_surveiller.begin(), rapport.a_surveiller.end(),
                     [&](const AppSensible& x, const AppSensible& y) { return score(x) > score(y); });

    // --- 4. Réglages de sécurité affaiblis ---
    auto reglage = [&](const char* espace, const char* cle)
    { return nettoyer(executer(adb + "shell settings get " + espace + " " + cle)); };

    if (reglage("global", "package_verifier_enable") == "0")
        rapport.alertes.push_back("Google Play Protect est désactivé.");
    if (reglage("global", "upload_apk_enable") == "0")
        rapport.alertes.push_back("Play Protect n'analyse plus les applis inconnues.");
    if (reglage("global", "verifier_verify_adb_installs") == "0")
        rapport.alertes.push_back("Les applis installées par ADB ne sont plus vérifiées.");
    if (executer(adb + "shell which su magisk").find('/') != std::string::npos)
        rapport.alertes.push_back("Accès root détecté (su ou Magisk présent).");
    if (nettoyer(executer(adb + "shell getprop ro.boot.verifiedbootstate")) == "orange")
        rapport.alertes.push_back("Bootloader déverrouillé : le système a pu être modifié.");

    std::clog << "[SECURITE] " << rapport.detections.size() << " détection(s), "
              << rapport.traces.size() << " trace(s), "
              << rapport.a_surveiller.size() << " appli(s) à surveiller, "
              << rapport.alertes.size() << " alerte(s)" << std::endl;
    return rapport;
}

} // namespace securite
