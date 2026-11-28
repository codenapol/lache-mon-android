// Lache mon Android ! - Tests de la lecture de « adb devices -l » (usb.cpp)
// Copyright (C) 2026 codenapol
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lancer : make test

#include "../usb.h"

#include <iostream>
#include <string>

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

    void tests_serie_valide()
    {
        verifier(usb::serie_valide("R58N123ABC"), "numéro Samsung");
        verifier(usb::serie_valide("emulator-5554"), "émulateur");
        verifier(usb::serie_valide("0123456789ABCDEF"), "numéro hexadécimal");
        verifier(usb::serie_valide("HT7A1.B_2-3"), "points, tirets et soulignés");
        verifier(!usb::serie_valide(""), "vide");
        verifier(!usb::serie_valide("-s"), "commence par un tiret (option d'adb)");
        verifier(!usb::serie_valide("abc;rm -rf ~"), "point-virgule et espace");
        verifier(!usb::serie_valide("abc$(reboot)"), "substitution de commande");
        verifier(!usb::serie_valide("abc`id`"), "accents graves");
        verifier(!usb::serie_valide("abc|id"), "tube");
        verifier(!usb::serie_valide("abc&id"), "esperluette");
        verifier(!usb::serie_valide("a'b"), "guillemet simple");
        verifier(!usb::serie_valide("a\"b"), "guillemet double");
        verifier(!usb::serie_valide("a b"), "espace");
        verifier(!usb::serie_valide("a\nb"), "retour à la ligne");
        verifier(!usb::serie_valide("????????????"), "points d'interrogation (vieux adb sans droits)");
        verifier(!usb::serie_valide("192.168.1.5:5555"), "deux-points");
        verifier(!usb::serie_valide("é123"), "caractère non ASCII");
        verifier(!usb::serie_valide(std::string(129, 'A')), "trop long");
        verifier(usb::serie_valide(std::string(128, 'A')), "longueur maximale");
    }

    void tests_sans_fil()
    {
        verifier(usb::est_sans_fil("192.168.1.42:5555"), "IP:port");
        verifier(usb::est_sans_fil("192.168.1.42:41235"), "IP:port aléatoire");
        verifier(usb::est_sans_fil("[fe80::1]:5555"), "IPv6");
        verifier(usb::est_sans_fil("adb-R58N123ABC-AbCdEf._adb-tls-connect._tcp"), "annonce mDNS");
        verifier(usb::est_sans_fil("adb-R58N123ABC-AbCdEf._adb._tcp"), "annonce mDNS ancienne");
        verifier(!usb::est_sans_fil("R58N123ABC"), "USB");
        verifier(!usb::est_sans_fil("emulator-5554"), "émulateur");
    }

    void tests_liste_vide()
    {
        ListeUsb l = usb::analyser("List of devices attached\n\n");
        egal(l.appareils.size(), 0, "liste vide : aucun appareil");
        verifier(l.erreur.empty(), "liste vide : pas d'erreur");

        l = usb::analyser("* daemon not running; starting now at tcp:5037\n"
                          "* daemon started successfully\n"
                          "List of devices attached\n\n");
        egal(l.appareils.size(), 0, "démarrage du serveur : aucun appareil");
        verifier(l.erreur.empty(), "démarrage du serveur : pas d'erreur");
    }

    void tests_erreurs_adb()
    {
        ListeUsb l = usb::analyser("");
        verifier(!l.erreur.empty(), "sortie vide = erreur");
        egal(l.appareils.size(), 0, "sortie vide : aucun appareil");

        l = usb::analyser("* daemon not running; starting now at tcp:5037\n"
                          "could not read ok from ADB Server\n"
                          "* failed to start daemon\n"
                          "error: cannot connect to daemon\n");
        verifier(!l.erreur.empty(), "serveur adb qui ne démarre pas = erreur");
        verifier(l.erreur.find("cannot connect to daemon") != std::string::npos, "le message d'adb est gardé");

        l = usb::analyser("sh: 1: adb: not found\n");
        verifier(!l.erreur.empty(), "adb absent = erreur");
        egal(l.appareils.size(), 0, "adb absent : aucun appareil");
    }

    void tests_telephone_pret()
    {
        ListeUsb l = usb::analyser(
            "List of devices attached\n"
            "R58N123ABC             device usb:1-1 product:beyond1lteeea model:SM_G973F device:beyond1 transport_id:1\n");
        egal(l.appareils.size(), 1, "un téléphone");
        const AppareilUsb& a = l.appareils[0];
        egal(a.serie, std::string("R58N123ABC"), "numéro de série");
        egal(a.etat, std::string("device"), "état");
        egal(a.modele, std::string("SM G973F"), "modèle sans soulignés");
        verifier(a.connectable(), "prêt");
        verifier(!a.emulateur, "pas un émulateur");
        egal(a.nom(), std::string("SM G973F"), "nom affiché = modèle");
        egal(usb::etat_court(a), std::string("Prêt"), "état court");
        verifier(usb::conseil(a).empty(), "aucun conseil pour un téléphone prêt");
    }

    void tests_format_court()
    {
        // « adb devices » sans -l : tabulation, pas de détails
        ListeUsb l = usb::analyser("List of devices attached\nR58N123ABC\tdevice\n\n");
        egal(l.appareils.size(), 1, "format court : un téléphone");
        egal(l.appareils[0].modele, std::string(""), "format court : pas de modèle");
        egal(l.appareils[0].nom(), std::string("Téléphone"), "format court : nom par défaut");
        verifier(l.appareils[0].connectable(), "format court : prêt");
    }

    void tests_fin_de_ligne_windows()
    {
        ListeUsb l = usb::analyser("List of devices attached\r\nR58N123ABC\tdevice\r\n\r\n");
        egal(l.appareils.size(), 1, "CRLF : un téléphone");
        egal(l.appareils[0].etat, std::string("device"), "CRLF : état sans \\r");
        verifier(l.appareils[0].connectable(), "CRLF : prêt");
    }

    void tests_etats()
    {
        ListeUsb l = usb::analyser(
            "List of devices attached\n"
            "AAA111                 unauthorized usb:1-2 transport_id:3\n"
            "BBB222                 offline usb:1-3 transport_id:4\n"
            "CCC333                 no permissions (missing udev rules? user is in the plugdev group); "
            "see [http://developer.android.com/tools/device.html] usb:3-1 transport_id:5\n"
            "DDD444                 recovery usb:1-4 product:x model:Pixel_7 device:panther transport_id:6\n"
            "EEE555                 sideload usb:1-5 transport_id:7\n"
            "FFF666                 authorizing usb:1-6 transport_id:8\n"
            "GGG777                 bootloader usb:1-7 transport_id:9\n");
        egal(l.appareils.size(), 7, "sept appareils, un par état");
        for (const AppareilUsb& a : l.appareils)
        {
            verifier(!a.connectable(), "pas prêt : " + a.serie);
            verifier(!usb::conseil(a).empty(), "conseil donné : " + a.etat);
            verifier(usb::etat_court(a) != "Prêt", "état court différent de Prêt : " + a.etat);
        }
        egal(l.appareils[0].etat, std::string("unauthorized"), "unauthorized");
        egal(l.appareils[1].etat, std::string("offline"), "offline");
        egal(l.appareils[2].etat, std::string("no permissions"), "no permissions (deux mots)");
        egal(l.appareils[2].modele, std::string(""), "no permissions : l'URL n'est pas prise pour un modèle");
        egal(l.appareils[3].modele, std::string("Pixel 7"), "modèle en recovery");
        egal(usb::etat_court(l.appareils[0]), std::string("Non autorisé"), "état court unauthorized");
        egal(usb::etat_court(l.appareils[2]), std::string("Accès refusé"), "état court no permissions");
        egal(usb::etat_court(l.appareils[1]), std::string("Hors ligne"), "état court offline");
        egal(usb::etat_court(l.appareils[3]), std::string("Mode spécial"), "état court recovery");
        egal(usb::etat_court(l.appareils[5]), std::string("Connexion..."), "état court authorizing");
        verifier(usb::conseil(l.appareils[2]).find("udev") != std::string::npos, "conseil udev");
        verifier(usb::conseil(l.appareils[0]).find("Autoriser") != std::string::npos, "conseil autorisation");
        verifier(usb::conseil(l.appareils[1]).find("rebranche") != std::string::npos, "conseil hors ligne");
    }

    void tests_vieux_adb_sans_droits()
    {
        // Les vieux adb affichent des « ? » à la place du numéro quand Linux bloque l'accès
        ListeUsb l = usb::analyser("List of devices attached\n????????????\tno permissions\n");
        egal(l.appareils.size(), 1, "vieux adb : appareil listé (pour afficher le conseil)");
        verifier(!l.appareils[0].connectable(), "vieux adb : pas connectable");
    }

    void tests_sans_fil_ignores()
    {
        ListeUsb l = usb::analyser(
            "List of devices attached\n"
            "192.168.1.42:41235     device product:panther model:Pixel_7 device:panther transport_id:1\n"
            "adb-28201FDH200ABC-xyz._adb-tls-connect._tcp device product:panther model:Pixel_7 transport_id:2\n"
            "R58N123ABC             device usb:1-1 product:beyond1lteeea model:SM_G973F transport_id:3\n");
        egal(l.appareils.size(), 1, "seul l'appareil USB reste");
        egal(l.appareils[0].serie, std::string("R58N123ABC"), "c'est bien l'appareil USB");
    }

    void tests_emulateur()
    {
        ListeUsb l = usb::analyser(
            "List of devices attached\n"
            "emulator-5554          device product:sdk_gphone64_x86_64 model:sdk_gphone64_x86_64 "
            "device:emu64xa transport_id:1\n"
            "emulator-5556          offline transport_id:2\n");
        egal(l.appareils.size(), 2, "deux émulateurs");
        verifier(l.appareils[0].emulateur, "emulator- = émulateur");
        verifier(l.appareils[0].connectable(), "émulateur prêt");
        egal(l.appareils[0].modele, std::string("sdk gphone64 x86 64"), "modèle de l'émulateur");
        egal(l.appareils[1].nom(), std::string("Émulateur"), "émulateur sans modèle");
    }

    void tests_plusieurs()
    {
        ListeUsb l = usb::analyser(
            "List of devices attached\n"
            "R58N123ABC             device usb:1-1 model:SM_G973F transport_id:1\n"
            "28201FDH200ABC         device usb:1-2 model:Pixel_7 transport_id:2\n"
            "AAA111                 unauthorized usb:1-3 transport_id:3\n");
        egal(l.appareils.size(), 3, "trois appareils");
        int prets = 0;
        for (const AppareilUsb& a : l.appareils)
            prets += a.connectable();
        egal(prets, 2, "deux prêts");
        egal(l.appareils[1].modele, std::string("Pixel 7"), "ordre gardé");
    }

    void tests_injections()
    {
        // adb ne devrait jamais afficher ça, mais la liste ne doit jamais proposer ces numéros
        ListeUsb l = usb::analyser(
            "List of devices attached\n"
            "abc;reboot             device usb:1-1 model:Piege transport_id:1\n"
            "$(reboot)              device usb:1-2 transport_id:2\n"
            "-s                     device usb:1-3 transport_id:3\n"
            "`id`                   device usb:1-4 transport_id:4\n");
        egal(l.appareils.size(), 4, "numéros piégés listés");
        for (const AppareilUsb& a : l.appareils)
        {
            verifier(!a.connectable(), "numéro piégé jamais connectable : " + a.serie);
            egal(usb::etat_court(a), std::string("Refusé"), "numéro piégé affiché « Refusé » : " + a.serie);
            verifier(usb::conseil(a).find("inhabituel") != std::string::npos, "numéro piégé expliqué : " + a.serie);
        }
    }

    void tests_lignes_parasites()
    {
        ListeUsb l = usb::analyser(
            "adb server version (39) doesn't match this client (41); killing...\n"
            "* daemon started successfully\n"
            "List of devices attached\n"
            "* something odd\n"
            "lonely\n"
            "XYZ999                 weirdstate usb:1-1\n"
            "R58N123ABC             device usb:1-1 transport_id:1\n"
            "   \n");
        egal(l.appareils.size(), 1, "lignes parasites ignorées");
        egal(l.appareils[0].serie, std::string("R58N123ABC"), "seul le vrai téléphone reste");
        verifier(l.erreur.empty(), "pas d'erreur malgré les lignes parasites");
    }

    void tests_modele_vide()
    {
        ListeUsb l = usb::analyser("List of devices attached\nR58N123ABC device model: transport_id:1\n");
        egal(l.appareils.size(), 1, "modèle vide : appareil gardé");
        egal(l.appareils[0].nom(), std::string("Téléphone"), "modèle vide : nom par défaut");
    }
}

int main()
{
    tests_serie_valide();
    tests_sans_fil();
    tests_liste_vide();
    tests_erreurs_adb();
    tests_telephone_pret();
    tests_format_court();
    tests_fin_de_ligne_windows();
    tests_etats();
    tests_vieux_adb_sans_droits();
    tests_sans_fil_ignores();
    tests_emulateur();
    tests_plusieurs();
    tests_injections();
    tests_lignes_parasites();
    tests_modele_vide();

    std::cout << (tests - echecs) << "/" << tests << " tests réussis" << std::endl;
    return echecs == 0 ? 0 : 1;
}
