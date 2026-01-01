// Lache mon Android ! - Mini lecteur JSON
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier
// selon les termes de la GNU General Public License version 3 ou ultérieure.
// Il est distribué SANS AUCUNE GARANTIE. Voir le fichier LICENSE.

#pragma once
// Mini lecteur JSON, sans dépendance (fichiers STIX2 des indicateurs, liste UAD-ng)

#include <cctype>
#include <string>
#include <utility>
#include <vector>

struct Json
{
    enum Type { NUL, BOOLEEN, NOMBRE, TEXTE, TABLEAU, OBJET } type = NUL;
    std::string texte;
    std::vector<Json> tableau;
    std::vector<std::pair<std::string, Json>> objet;

    const Json* champ(const std::string& cle) const
    {
        for (const auto& [k, v] : objet)
            if (k == cle)
                return &v;
        return nullptr;
    }

    std::string texte_de(const std::string& cle) const
    {
        const Json* v = champ(cle);
        return (v && v->type == TEXTE) ? v->texte : "";
    }
};

class LecteurJson
{
public:
    explicit LecteurJson(const std::string& s) : s_(s) {}

    bool lire(Json& sortie)
    {
        espaces();
        if (!valeur(sortie, 0))
            return false;
        espaces();
        return i_ == s_.size();
    }

private:
    const std::string& s_;
    size_t i_ = 0;

    bool fini() const { return i_ >= s_.size(); }
    void espaces() { while (!fini() && std::isspace((unsigned char)s_[i_])) ++i_; }

    bool mot(const char* m)
    {
        const size_t n = std::char_traits<char>::length(m);
        if (s_.compare(i_, n, m) != 0)
            return false;
        i_ += n;
        return true;
    }

    bool valeur(Json& v, int profondeur)
    {
        if (profondeur > 64 || fini())
            return false;
        const char c = s_[i_];
        if (c == '{') return objet(v, profondeur);
        if (c == '[') return tableau(v, profondeur);
        if (c == '"') { v.type = Json::TEXTE; return chaine(v.texte); }
        if (mot("true") || mot("false")) { v.type = Json::BOOLEEN; return true; }
        if (mot("null")) { v.type = Json::NUL; return true; }

        // Nombre : gardé sous forme de texte (inutile ici)
        const size_t debut = i_;
        while (!fini() && (std::isdigit((unsigned char)s_[i_]) ||
                           std::string("+-.eE").find(s_[i_]) != std::string::npos))
            ++i_;
        if (i_ == debut)
            return false;
        v.type = Json::NOMBRE;
        v.texte = s_.substr(debut, i_ - debut);
        return true;
    }

    bool objet(Json& v, int profondeur)
    {
        v.type = Json::OBJET;
        ++i_;
        espaces();
        if (!fini() && s_[i_] == '}') { ++i_; return true; }
        while (true)
        {
            espaces();
            std::string cle;
            if (fini() || s_[i_] != '"' || !chaine(cle))
                return false;
            espaces();
            if (fini() || s_[i_] != ':')
                return false;
            ++i_;
            espaces();
            v.objet.emplace_back(std::move(cle), Json());
            if (!valeur(v.objet.back().second, profondeur + 1))
                return false;
            espaces();
            if (!fini() && s_[i_] == ',') { ++i_; continue; }
            if (!fini() && s_[i_] == '}') { ++i_; return true; }
            return false;
        }
    }

    bool tableau(Json& v, int profondeur)
    {
        v.type = Json::TABLEAU;
        ++i_;
        espaces();
        if (!fini() && s_[i_] == ']') { ++i_; return true; }
        while (true)
        {
            espaces();
            v.tableau.emplace_back();
            if (!valeur(v.tableau.back(), profondeur + 1))
                return false;
            espaces();
            if (!fini() && s_[i_] == ',') { ++i_; continue; }
            if (!fini() && s_[i_] == ']') { ++i_; return true; }
            return false;
        }
    }

    static void ajouter_utf8(std::string& s, unsigned long cp)
    {
        if (cp < 0x80) s += (char)cp;
        else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
        else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F));
               s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
    }

    bool hexa4(unsigned long& cp)
    {
        if (i_ + 4 > s_.size())
            return false;
        cp = 0;
        for (int k = 0; k < 4; ++k)
        {
            const char h = s_[i_++];
            cp <<= 4;
            if (h >= '0' && h <= '9') cp |= h - '0';
            else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
            else return false;
        }
        return true;
    }

    bool chaine(std::string& sortie)
    {
        ++i_; // guillemet ouvrant
        while (!fini())
        {
            const char c = s_[i_++];
            if (c == '"')
                return true;
            if (c != '\\') { sortie += c; continue; }
            if (fini())
                return false;
            const char e = s_[i_++];
            switch (e)
            {
                case '"': sortie += '"'; break;
                case '\\': sortie += '\\'; break;
                case '/': sortie += '/'; break;
                case 'b': sortie += '\b'; break;
                case 'f': sortie += '\f'; break;
                case 'n': sortie += '\n'; break;
                case 'r': sortie += '\r'; break;
                case 't': sortie += '\t'; break;
                case 'u':
                {
                    unsigned long cp;
                    if (!hexa4(cp))
                        return false;
                    // Paire de substitution UTF-16
                    if (cp >= 0xD800 && cp <= 0xDBFF && s_.compare(i_, 2, "\\u") == 0)
                    {
                        i_ += 2;
                        unsigned long bas;
                        if (!hexa4(bas))
                            return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (bas - 0xDC00);
                    }
                    ajouter_utf8(sortie, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }
};
