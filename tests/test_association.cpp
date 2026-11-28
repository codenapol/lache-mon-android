// Lache mon Android ! - Tests de l'association (association.cpp) et de la lecture mDNS (decouverte.cpp)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lancer : make test

#include "../association.h"
#include "../decouverte.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
    int tests = 0, echecs = 0;

    void verifier(bool condition, const std::string& description)
    {
        ++tests;
        if (!condition)
        {
            ++echecs;
            std::cerr << "ÉCHEC : " << description << std::endl;
        }
    }

    template <typename T>
    void egal(const T& obtenu, const T& attendu, const std::string& description)
    {
        ++tests;
        if (!(obtenu == attendu))
        {
            ++echecs;
            std::cerr << "ÉCHEC : " << description << "\n  obtenu  : " << obtenu
                      << "\n  attendu : " << attendu << std::endl;
        }
    }

    void egal(size_t obtenu, size_t attendu, const std::string& description)
    {
        egal<size_t>(obtenu, attendu, description);
    }

    bool contient(const std::string& texte, const std::string& motif)
    {
        return texte.find(motif) != std::string::npos;
    }

    void tests_code_valide()
    {
        verifier(association::code_valide("123456"), "6 chiffres");
        verifier(association::code_valide("000000"), "que des zéros");
        verifier(!association::code_valide(""), "vide");
        verifier(!association::code_valide("12345"), "5 chiffres");
        verifier(!association::code_valide("1234567"), "7 chiffres");
        verifier(!association::code_valide("12345a"), "lettre");
        verifier(!association::code_valide("123 456"), "espace");
        verifier(!association::code_valide(" 12345"), "espace au début");
        verifier(!association::code_valide("-12345"), "signe moins");
        verifier(!association::code_valide("12.456"), "point (accepté par CharsDecimal d'ImGui)");
        verifier(!association::code_valide("123456;reboot"), "point-virgule");
        verifier(!association::code_valide(";reboot"), "injection courte");
        verifier(!association::code_valide("$(id)1"), "substitution de commande");
        verifier(!association::code_valide("\xd9\xa1\xd9\xa2\xd9\xa3"), "chiffres arabes-indiens (non ASCII)");
        verifier(!association::code_valide(std::string("123\0456", 7)), "caractère nul");
    }

    void tests_succes()
    {
        ResultatAssociation r = association::analyser(
            "Successfully paired to 192.168.1.42:37099 [guid=adb-R58N123ABC-AbCdEf]\n", 0);
        verifier(r.reussi, "succès");
        verifier(r.erreur.empty(), "succès : pas d'erreur");

        r = association::analyser("* daemon not running; starting now at tcp:5037\n"
                                  "* daemon started successfully\n"
                                  "Successfully paired to 192.168.1.42:37099 [guid=adb-X]\n", 0);
        verifier(r.reussi, "succès après le démarrage du serveur adb");

        r = association::analyser("Successful\n", 1);
        verifier(!r.reussi, "« Successful » seul n'est pas un succès");
    }

    void tests_echecs()
    {
        // adb 31 : échec sur stdout avec le code 0 ; adb 34 et plus : stderr « error: », code 1
        ResultatAssociation v31 = association::analyser("Failed: Wrong password or connection was dropped.\n", 0);
        ResultatAssociation v34 = association::analyser("error: Failed: Wrong password or connection was dropped.\n", 1);
        verifier(!v31.reussi && !v34.reussi, "mauvais code : échec (31 et 34)");
        egal(v31.erreur, v34.erreur, "mauvais code : même message quelle que soit la version");
        verifier(contient(v34.erreur, "Code incorrect"), "mauvais code : message");

        ResultatAssociation r = association::analyser("error: Failed: Unable to start pairing client.\n", 1);
        verifier(!r.reussi && contient(r.erreur, "ne répond pas"), "port injoignable");

        r = association::analyser("", 124);
        verifier(!r.reussi && contient(r.erreur, "ne répond pas"), "délai dépassé (timeout)");

        // Sortie réelle d'adb 34.0.5 (Debian) et 37.0.3 (AppImage) sur un port fermé : le message
        // du code source n'arrive pas jusqu'au client
        r = association::analyser("* daemon not running; starting now at tcp:5039\n"
                                  "* daemon started successfully\n"
                                  "error: protocol fault (couldn't read status message): Success\n", 1);
        verifier(!r.reussi, "protocol fault : échec");
        verifier(contient(r.erreur, "Association refusée"), "protocol fault : message général");
        verifier(contient(r.detail, "protocol fault"), "protocol fault : réponse brute gardée");

        r = association::analyser("", 1);
        verifier(!r.reussi && !r.erreur.empty(), "sortie vide : échec avec message");

        r = association::analyser("", -1);
        verifier(!r.reussi && !r.erreur.empty(), "commande interrompue : échec avec message");

        r = association::analyser("adb: unknown command pair\n", 1);
        verifier(!r.reussi && contient(r.detail, "unknown command"), "vieil adb sans « pair »");
    }

    void tests_associer_refuse_entrees()
    {
        // Ces appels ne lancent aucune commande : les entrées sont refusées avant
        ResultatAssociation r = association::associer("192.168.1.42;reboot", 37099, "123456");
        verifier(!r.reussi && contient(r.erreur, "adresse IP"), "IP avec injection");
        r = association::associer("", 37099, "123456");
        verifier(!r.reussi && contient(r.erreur, "adresse IP"), "IP vide");
        r = association::associer("192.168.1.42", 0, "123456");
        verifier(!r.reussi && contient(r.erreur, "port"), "port 0");
        r = association::associer("192.168.1.42", 70000, "123456");
        verifier(!r.reussi && contient(r.erreur, "port"), "port trop grand");
        r = association::associer("192.168.1.42", 37099, "12345$(id)");
        verifier(!r.reussi && contient(r.erreur, "6 chiffres"), "code avec injection");
    }

    void tests_cause_possible()
    {
        verifier(association::cause_possible("failed to authenticate to 192.168.1.42:37123"),
                 "appareil non autorisé");
        verifier(association::cause_possible("failed to connect to 192.168.1.42:37123"),
                 "le port répond mais refuse");
        // Sortie réelle d'adb 34.0.5 et 37.0.3
        verifier(!association::cause_possible("failed to connect to '127.0.0.1:1': Connection refused"),
                 "port fermé : erreur réseau");
        verifier(!association::cause_possible("failed to connect to '192.168.1.42:37123': No route to host"),
                 "hôte injoignable");
        verifier(!association::cause_possible("failed to connect to '192.168.1.42:37123': Connection timed out"),
                 "délai réseau");
        verifier(!association::cause_possible("failed to resolve host: 'x': Name or service not known"),
                 "nom inconnu");
        verifier(!association::cause_possible("connected to 192.168.1.42:37123"), "succès");
        verifier(!association::cause_possible(""), "vide");
        verifier(!association::cause_possible("Connexion impossible."), "notre message par défaut");
    }

    void tests_mdns()
    {
        // Sortie sans téléphone (relevée avec adb 34.0.5 et 37.0.3)
        egal(decouverte::analyser_mdns("List of discovered mdns services\n\n").size(), size_t(0),
             "aucun service");
        egal(decouverte::analyser_mdns("").size(), size_t(0), "sortie vide");

        const std::vector<ServiceMdns> s = decouverte::analyser_mdns(
            "List of discovered mdns services\n"
            "adb-R58N123ABC-AbCdEf\t_adb-tls-connect._tcp\t192.168.1.42:37123\n"
            "adb-R58N123ABC-AbCdEf\t_adb-tls-pairing._tcp\t192.168.1.42:37099\n"
            "adb-OTHER\t_adb._tcp\t192.168.1.50:5555\n"
            "\n");
        egal(s.size(), size_t(3), "trois services");
        if (s.size() == 3)
        {
            egal(s[0].instance, std::string("adb-R58N123ABC-AbCdEf"), "instance");
            egal(s[0].type, std::string("_adb-tls-connect._tcp"), "type connexion");
            egal(s[0].ip, std::string("192.168.1.42"), "IP");
            egal(s[0].port, 37123, "port connexion");
            egal(s[1].type, std::string("_adb-tls-pairing._tcp"), "type association");
            egal(s[1].port, 37099, "port association");
            egal(s[2].type, std::string("_adb._tcp"), "type adb tcpip");
        }

        std::vector<ServiceMdns> t = decouverte::analyser_mdns(
            "adb-X\t_adb-tls-pairing._tcp.\t192.168.1.42:37099\r\n");
        verifier(t.size() == 1 && t[0].type == "_adb-tls-pairing._tcp", "point final et \\r retirés");

        t = decouverte::analyser_mdns("adb-X   _adb-tls-connect._tcp   192.168.1.42:37123\n");
        verifier(t.size() == 1 && t[0].port == 37123, "espaces au lieu de tabulations");

        for (const char* ligne : {
                 "adb-X\t_adb-tls-connect._tcp\t192.168.1.42:37123abc", // ancien atoi : 37123
                 "adb-X\t_adb-tls-connect._tcp\t192.168.1.42:0",
                 "adb-X\t_adb-tls-connect._tcp\t192.168.1.42:70000",
                 "adb-X\t_adb-tls-connect._tcp\t192.168.1.42:999999999999",
                 "adb-X\t_adb-tls-connect._tcp\t192.168.1.42:",
                 "adb-X\t_adb-tls-connect._tcp\t192.168.1.42",
                 "adb-X\t_adb-tls-connect._tcp\t192.168.1.420:37123",
                 "adb-X\t_adb-tls-connect._tcp\t[fe80::1]:37123",
                 "adb-X\t_adb-tls-connect._tcp\t192.168.1.42:-1",
                 "_adb-tls-connect._tcp\t192.168.1.42:37123", // 2 mots seulement
             })
            egal(decouverte::analyser_mdns(ligne).size(), size_t(0), std::string("ligne rejetée : ") + ligne);
    }
}

int main()
{
    tests_code_valide();
    tests_succes();
    tests_echecs();
    tests_associer_refuse_entrees();
    tests_cause_possible();
    tests_mdns();

    std::cout << (tests - echecs) << "/" << tests << " tests réussis" << std::endl;
    return echecs == 0 ? 0 : 1;
}
