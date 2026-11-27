// Lache mon Android ! - Rapports d'analyse (PDF ou HTML)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#include "rapport.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "outils.h" // lire_fichier, nettoyer

namespace fs = std::filesystem;

namespace
{
    // =====================================================================
    // HTML
    // =====================================================================

    std::string echapper_html(const std::string& s)
    {
        std::string r;
        for (char c : s)
            switch (c)
            {
                case '&': r += "&amp;";  break;
                case '<': r += "&lt;";   break;
                case '>': r += "&gt;";   break;
                case '"': r += "&quot;"; break;
                default:  r += c;
            }
        return r;
    }

    const char* STYLE_HTML = R"(
  body { font-family: "Noto Sans", "Helvetica Neue", Arial, sans-serif; color: #1c1c20; background: #ffffff;
         max-width: 760px; margin: 40px auto; padding: 0 20px; line-height: 1.5; font-size: 15px; }
  h1 { font-size: 26px; margin: 0 0 8px; }
  h2 { font-size: 18px; color: #3361bf; margin: 28px 0 8px; padding-bottom: 4px; border-bottom: 1px solid #d8dbe3; }
  p { margin: 0 0 8px; }
  ul { margin: 0 0 10px; padding-left: 22px; }
  li { margin: 0 0 4px; overflow-wrap: anywhere; }
  .danger { color: #b32626; font-weight: bold; }
  .alerte { color: #b8650a; }
  .ok { color: #25883f; }
  .discret { color: #6b6b73; font-size: 13px; }
  footer { margin-top: 36px; color: #6b6b73; font-size: 12px; }
  @media print { body { margin: 0 auto; } }
)";

    // =====================================================================
    // PDF : texte seul, polices standard Helvetica (aucune police embarquée)
    // =====================================================================

    // UTF-8 → WinAnsiEncoding (Windows-1252), le codage des polices standard du PDF.
    // Les caractères absents de ce codage deviennent '?'.
    std::string vers_winansi(const std::string& s)
    {
        std::string r;
        for (size_t i = 0; i < s.size();)
        {
            const unsigned char c = (unsigned char)s[i];
            unsigned int cp = c;
            size_t n = 1;
            if      (c >= 0xF0 && i + 3 < s.size()) { cp = ((c & 0x07) << 18) | ((s[i+1] & 0x3F) << 12) | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F); n = 4; }
            else if (c >= 0xE0 && i + 2 < s.size())     { cp = ((c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F); n = 3; }
            else if (c >= 0xC0 && i + 1 < s.size())     { cp = ((c & 0x1F) << 6) | (s[i+1] & 0x3F); n = 2; }
            i += n;

            if (cp < 0x80 || (cp >= 0xA0 && cp <= 0xFF)) { r += (char)cp; continue; }
            switch (cp)
            {
                case 0x20AC: r += '\x80'; break; // €
                case 0x2026: r += '\x85'; break; // …
                case 0x0152: r += '\x8C'; break; // Œ
                case 0x2018: r += '\x91'; break; // ‘
                case 0x2019: r += '\x92'; break; // ’
                case 0x201C: r += '\x93'; break; // “
                case 0x201D: r += '\x94'; break; // ”
                case 0x2022: r += '\x95'; break; // •
                case 0x2013: r += '\x96'; break; // –
                case 0x2014: r += '\x97'; break; // —
                case 0x0153: r += '\x9C'; break; // œ
                case 0x202F: r += '\xA0'; break; // espace fine insécable
                default:     r += '?';
            }
        }
        return r;
    }

    // Largeurs des caractères 32 à 126 (unités de 1/1000 du corps), métriques Adobe Helvetica
    const short LARGEURS_NORMAL[95] = {
        278,278,355,556,556,889,667,191,333,333,389,584,278,333,278,278,556,556,556,556,556,556,556,556,
        556,556,278,278,584,584,584,556,1015,667,667,722,722,667,611,778,722,278,500,667,556,833,722,778,
        667,778,722,667,611,722,667,944,667,667,611,278,278,278,469,556,333,556,556,500,556,556,278,556,
        556,222,222,500,222,833,556,556,556,556,333,500,278,556,500,722,500,500,500,334,260,334,584};
    const short LARGEURS_GRAS[95] = {
        278,333,474,556,556,889,722,238,333,333,389,584,278,333,278,278,556,556,556,556,556,556,556,556,
        556,556,333,333,584,584,584,611,975,722,722,722,722,667,611,778,722,278,556,722,611,833,722,778,
        667,778,722,667,611,722,667,944,667,667,611,333,278,333,584,556,333,556,611,556,611,556,333,611,
        611,278,278,556,278,889,611,611,611,611,389,556,333,611,556,778,556,556,500,389,280,389,584};

    // Largeur d'un caractère WinAnsi (lettres accentuées : largeur de la lettre de base)
    int largeur_caractere(unsigned char c, bool gras)
    {
        const short* t = gras ? LARGEURS_GRAS : LARGEURS_NORMAL;
        if (c >= 32 && c <= 126) return t[c - 32];
        if (c >= 0xC0 && c <= 0xC5) return t['A' - 32];
        if (c == 0xC7)              return t['C' - 32];
        if (c >= 0xC8 && c <= 0xCB) return t['E' - 32];
        if (c >= 0xCC && c <= 0xCF) return t['I' - 32];
        if (c >= 0xD2 && c <= 0xD6) return t['O' - 32];
        if (c >= 0xD9 && c <= 0xDC) return t['U' - 32];
        if (c >= 0xE0 && c <= 0xE5) return t['a' - 32];
        if (c == 0xE7)              return t['c' - 32];
        if (c >= 0xE8 && c <= 0xEB) return t['e' - 32];
        if (c >= 0xEC && c <= 0xEF) return t['i' - 32];
        if (c >= 0xF2 && c <= 0xF6) return t['o' - 32];
        if (c >= 0xF9 && c <= 0xFC) return t['u' - 32];
        switch (c)
        {
            case 0x95: return 350;              // •
            case 0x85: case 0x97: return 1000;  // … —
            case 0x8C: return 1000;             // Œ
            case 0x9C: return 944;              // œ
            case 0xA0: return 278;              // espace insécable
            case 0xAB: case 0xBB: return 556;   // « »
            case 0xB7: return 278;              // ·
            case 0xB0: return 400;              // °
            default:   return 556;
        }
    }

    double largeur_texte(const std::string& winansi, bool gras, double corps)
    {
        double l = 0;
        for (char c : winansi)
            l += largeur_caractere((unsigned char)c, gras);
        return l * corps / 1000.0;
    }

    // Découpe un texte (déjà en WinAnsi) en lignes d'au plus `largeur` points ;
    // un mot trop long (nom de paquet, chemin) est coupé où il le faut.
    std::vector<std::string> couper_lignes(const std::string& texte, bool gras, double corps, double largeur)
    {
        std::vector<std::string> lignes;
        std::string ligne;
        std::istringstream mots(texte);
        std::string mot;
        while (mots >> mot)
        {
            const std::string essai = ligne.empty() ? mot : ligne + " " + mot;
            if (largeur_texte(essai, gras, corps) <= largeur)
            {
                ligne = essai;
                continue;
            }
            if (!ligne.empty())
                lignes.push_back(ligne);
            ligne.clear();
            while (largeur_texte(mot, gras, corps) > largeur) // mot plus large que la ligne
            {
                size_t n = 1;
                while (n < mot.size() && largeur_texte(mot.substr(0, n + 1), gras, corps) <= largeur)
                    ++n;
                lignes.push_back(mot.substr(0, n));
                mot.erase(0, n);
            }
            ligne = mot;
        }
        if (!ligne.empty())
            lignes.push_back(ligne);
        return lignes;
    }

    std::string chaine_pdf(const std::string& winansi)
    {
        std::string r = "(";
        for (char c : winansi)
        {
            if (c == '(' || c == ')' || c == '\\')
                r += '\\';
            r += c;
        }
        return r + ")";
    }

    std::string nombre(double v)
    {
        char tampon[32];
        std::snprintf(tampon, sizeof(tampon), "%.2f", v);
        return tampon;
    }

    struct StyleBloc
    {
        bool   gras;
        double corps;          // taille de la police (points)
        double r, v, b;        // couleur
        double avant, apres;   // espaces verticaux
        double retrait;        // décalage à gauche
    };

    StyleBloc style_de(TypeBloc type)
    {
        switch (type)
        {
            case TypeBloc::TITRE:   return {true,  20.0, 0.11, 0.11, 0.13,  0, 10, 0};
            case TypeBloc::SECTION: return {true,  13.0, 0.20, 0.38, 0.75, 16,  6, 0};
            case TypeBloc::PUCE:    return {false, 10.5, 0.11, 0.11, 0.13,  0,  3, 14};
            case TypeBloc::DANGER:  return {true,  10.5, 0.70, 0.15, 0.15,  0,  5, 0};
            case TypeBloc::ALERTE:  return {false, 10.5, 0.72, 0.40, 0.04,  0,  5, 0};
            case TypeBloc::OK:      return {false, 10.5, 0.15, 0.53, 0.25,  0,  5, 0};
            case TypeBloc::DISCRET: return {false,  9.5, 0.42, 0.42, 0.45,  0,  5, 0};
            default:                return {false, 10.5, 0.11, 0.11, 0.13,  0,  5, 0};
        }
    }

    // Page A4 en points
    const double PAGE_L = 595.28, PAGE_H = 841.89;
    const double MARGE_G = 56, MARGE_D = 56, MARGE_HAUT = 60, MARGE_BAS = 64;
}

namespace rapport
{
    std::string en_html(const Rapport& r)
    {
        std::string h = "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"utf-8\">\n"
                        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>" +
                        echapper_html(r.titre) + "</title>\n<style>" + STYLE_HTML + "</style>\n</head>\n<body>\n";
        bool dans_liste = false;
        for (const BlocRapport& b : r.blocs)
        {
            if (b.type != TypeBloc::PUCE && dans_liste)
            {
                h += "</ul>\n";
                dans_liste = false;
            }
            const std::string t = echapper_html(b.texte);
            switch (b.type)
            {
                case TypeBloc::TITRE:   h += "<h1>" + t + "</h1>\n"; break;
                case TypeBloc::SECTION: h += "<h2>" + t + "</h2>\n"; break;
                case TypeBloc::PUCE:
                    if (!dans_liste) { h += "<ul>\n"; dans_liste = true; }
                    h += "<li>" + t + "</li>\n";
                    break;
                case TypeBloc::DANGER:  h += "<p class=\"danger\">" + t + "</p>\n"; break;
                case TypeBloc::ALERTE:  h += "<p class=\"alerte\">" + t + "</p>\n"; break;
                case TypeBloc::OK:      h += "<p class=\"ok\">" + t + "</p>\n"; break;
                case TypeBloc::DISCRET: h += "<p class=\"discret\">" + t + "</p>\n"; break;
                default:                h += "<p>" + t + "</p>\n";
            }
        }
        if (dans_liste)
            h += "</ul>\n";
        h += "<footer>Généré par Lâche mon Android !</footer>\n</body>\n</html>\n";
        return h;
    }

    std::string en_pdf(const Rapport& r)
    {
        // --- 1. Mise en page : un flux de dessin par page ---
        std::vector<std::string> pages(1);
        double y = PAGE_H - MARGE_HAUT;
        const double largeur_utile = PAGE_L - MARGE_G - MARGE_D;

        auto nouvelle_page = [&]()
        {
            pages.emplace_back();
            y = PAGE_H - MARGE_HAUT;
        };
        auto ecrire = [&](const std::string& winansi, bool gras, double corps, const StyleBloc& s, double x)
        {
            pages.back() += "BT /" + std::string(gras ? "F2 " : "F1 ") + nombre(corps) + " Tf " +
                            nombre(s.r) + " " + nombre(s.v) + " " + nombre(s.b) + " rg " +
                            nombre(x) + " " + nombre(y) + " Td " + chaine_pdf(winansi) + " Tj ET\n";
        };

        for (size_t k = 0; k < r.blocs.size(); ++k)
        {
            const BlocRapport& b = r.blocs[k];
            const StyleBloc s = style_de(b.type);
            const double interligne = s.corps * 1.35;
            const std::vector<std::string> lignes =
                couper_lignes(vers_winansi(b.texte), s.gras, s.corps, largeur_utile - s.retrait);

            // Un titre de section ne reste jamais seul en bas de page
            double besoin = s.avant + interligne * (double)std::max<size_t>(lignes.size(), 1);
            if (b.type == TypeBloc::SECTION)
                besoin += 4 + 10.5 * 1.35 * 2;
            if (y - besoin < MARGE_BAS && y < PAGE_H - MARGE_HAUT)
                nouvelle_page();
            else if (y < PAGE_H - MARGE_HAUT)
                y -= s.avant;

            for (size_t n = 0; n < lignes.size(); ++n)
            {
                if (y - interligne < MARGE_BAS)
                    nouvelle_page();
                y -= interligne;
                if (b.type == TypeBloc::PUCE && n == 0)
                    ecrire("\x95", false, s.corps, s, MARGE_G + 3);
                ecrire(lignes[n], s.gras, s.corps, s, MARGE_G + s.retrait);
            }
            if (b.type == TypeBloc::SECTION) // filet sous le titre de section
            {
                y -= 4;
                pages.back() += "0.85 0.86 0.89 RG 0.6 w " + nombre(MARGE_G) + " " + nombre(y) + " m " +
                                nombre(PAGE_L - MARGE_D) + " " + nombre(y) + " l S\n";
            }
            y -= s.apres;
        }

        // Pied de page : nom du programme et numéro de page
        const StyleBloc pied = style_de(TypeBloc::DISCRET);
        for (size_t p = 0; p < pages.size(); ++p)
        {
            const std::string gauche = vers_winansi("Lâche mon Android !");
            const std::string droite = "Page " + std::to_string(p + 1) + " / " + std::to_string(pages.size());
            const std::string couleur = nombre(pied.r) + " " + nombre(pied.v) + " " + nombre(pied.b) + " rg ";
            pages[p] += "BT /F1 8 Tf " + couleur + nombre(MARGE_G) + " 36 Td " + chaine_pdf(gauche) + " Tj ET\n";
            pages[p] += "BT /F1 8 Tf " + couleur + nombre(PAGE_L - MARGE_D - largeur_texte(droite, false, 8)) +
                        " 36 Td " + chaine_pdf(droite) + " Tj ET\n";
        }

        // --- 2. Objets du fichier PDF ---
        // 1 catalogue, 2 arbre des pages, 3 et 4 polices, 5 informations, puis page + contenu par page
        std::vector<std::string> objets;
        std::string kids;
        for (size_t p = 0; p < pages.size(); ++p)
            kids += std::to_string(6 + 2 * p) + " 0 R ";
        objets.push_back("<< /Type /Catalog /Pages 2 0 R >>");
        objets.push_back("<< /Type /Pages /Kids [ " + kids + "] /Count " + std::to_string(pages.size()) + " >>");
        objets.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
        objets.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>");
        objets.push_back("<< /Title " + chaine_pdf(vers_winansi(r.titre)) + " /Producer " +
                         chaine_pdf(vers_winansi("Lâche mon Android !")) + " >>");
        for (size_t p = 0; p < pages.size(); ++p)
        {
            objets.push_back("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + nombre(PAGE_L) + " " + nombre(PAGE_H) +
                             "] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents " +
                             std::to_string(7 + 2 * p) + " 0 R >>");
            objets.push_back("<< /Length " + std::to_string(pages[p].size()) + " >>\nstream\n" + pages[p] +
                             "endstream");
        }

        std::string pdf = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
        std::vector<size_t> positions;
        for (size_t i = 0; i < objets.size(); ++i)
        {
            positions.push_back(pdf.size());
            pdf += std::to_string(i + 1) + " 0 obj\n" + objets[i] + "\nendobj\n";
        }
        const size_t debut_xref = pdf.size();
        pdf += "xref\n0 " + std::to_string(objets.size() + 1) + "\n0000000000 65535 f \n";
        for (size_t pos : positions)
        {
            char ligne[24];
            std::snprintf(ligne, sizeof(ligne), "%010zu 00000 n \n", pos);
            pdf += ligne;
        }
        pdf += "trailer\n<< /Size " + std::to_string(objets.size() + 1) + " /Root 1 0 R /Info 5 0 R >>\n"
               "startxref\n" + std::to_string(debut_xref) + "\n%%EOF\n";
        return pdf;
    }

    fs::path dossier_documents()
    {
        const char* home = std::getenv("HOME");
        const std::string maison = home ? home : ".";

        // ~/.config/user-dirs.dirs : XDG_DOCUMENTS_DIR="$HOME/Documents" (nom traduit selon la langue)
        fs::path config = fs::path(maison) / ".config";
        if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
            config = xdg;
        std::istringstream lignes(lire_fichier(config / "user-dirs.dirs"));
        std::string ligne;
        while (std::getline(lignes, ligne))
        {
            ligne = nettoyer(ligne);
            const std::string cle = "XDG_DOCUMENTS_DIR=";
            if (ligne.rfind(cle, 0) != 0)
                continue;
            std::string valeur = ligne.substr(cle.size());
            if (valeur.size() >= 2 && valeur.front() == '"' && valeur.back() == '"')
                valeur = valeur.substr(1, valeur.size() - 2);
            if (valeur.rfind("$HOME", 0) == 0)
                valeur = maison + valeur.substr(5);
            // Un dossier Documents égal au dossier personnel signifie « désactivé »
            if (!valeur.empty() && valeur.front() == '/' && fs::path(valeur) != fs::path(maison))
                return valeur;
        }
        return fs::path(maison) / "Documents";
    }

    bool enregistrer(const Rapport& r, FormatRapport format, const std::string& nom_base,
                     fs::path& chemin, std::string& erreur)
    {
        const fs::path dossier = dossier_documents();
        std::error_code ec;
        fs::create_directories(dossier, ec);
        if (ec)
        {
            erreur = "Impossible de créer le dossier " + dossier.string() + " : " + ec.message();
            return false;
        }

        // Jamais d'écrasement : « nom.pdf », puis « nom_2.pdf », « nom_3.pdf »...
        const std::string extension = format == FormatRapport::PDF ? ".pdf" : ".html";
        chemin = dossier / (nom_base + extension);
        for (int n = 2; fs::exists(chemin, ec) && n < 1000; ++n)
            chemin = dossier / (nom_base + "_" + std::to_string(n) + extension);

        const std::string contenu = format == FormatRapport::PDF ? en_pdf(r) : en_html(r);
        {
            std::ofstream f(chemin, std::ios::binary | std::ios::trunc);
            f << contenu;
            if (!f.good())
            {
                erreur = "Impossible d'écrire " + chemin.string() + ".";
                fs::remove(chemin, ec);
                return false;
            }
        }
        // Le rapport décrit le téléphone : lisible par son seul propriétaire
        fs::permissions(chemin, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
        return true;
    }
}
