<div align="center">

<img src="logo.png" alt="Logo de Lâche mon Android !" width="200">

# Lâche mon Android !

**Reprends la main sur ton téléphone Android depuis ton PC.**<br>
Debloat + détection de logiciels espions, sans rien installer sur le téléphone.

![Plateforme](https://img.shields.io/badge/plateforme-Linux-informational)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Interface](https://img.shields.io/badge/interface-Dear%20ImGui-8A2BE2)
![ADB](https://img.shields.io/badge/connexion-ADB%20Wi--Fi-3DDC84)
[![Licence : GPL-3.0](https://img.shields.io/badge/licence-GPL--3.0-blue)](LICENSE)

[Fonctionnalités](#-fonctionnalités) •
[Installation](#-installation) •
[Utilisation](#-utilisation) •
[Limites](#-limites-connues) •
[Feuille de route](#-feuille-de-route) •
[Crédits](#-crédits-et-licences)

</div>

---

**Lâche mon Android !** est un petit outil graphique qui pilote un téléphone Android via **ADB** (Android Debug Bridge), en Wi-Fi. Il permet :

- 🧹 de **retirer les applis inutiles** (bloatwares) et celles qui pistent l'utilisateur ;
- 🕵️ de **chercher des logiciels espions** connus (stalkerwares, Pegasus, Predator…) à partir des indicateurs du projet [MVT](https://github.com/mvt-project/mvt) d'Amnesty International.

Le programme ne fait que lancer des commandes `adb` : **aucune appli n'est installée sur le téléphone**. Il reste donc invisible pour un éventuel logiciel espion.

> [!WARNING]
> **Ce n'est ni un antivirus, ni un outil d'expertise.** Il détecte les logiciels espions *connus* et signale des accès suspects. Il ne protège pas en continu, ne détecte pas les malwares classiques (chevaux de Troie bancaires, adwares…) et ses résultats n'ont pas valeur de preuve.

## ✨ Fonctionnalités

### 🧹 Debloat

| Section | Contenu | Actions |
|---|---|---|
| **Bloatwares** | Liste intégrée + applis jugées sans risque par [UAD-ng](https://github.com/Universal-Debloater-Alliance/universal-android-debloater-next-generation) (« Recommended ») | Désactiver · Désinstaller · Remplacer¹ |
| **Suppression avancée** | UAD-ng « Advanced » : peut retirer une fonction utile | Désactiver · Désinstaller |
| **Suppression experte** | UAD-ng « Expert » : peut casser une partie du système | Désactiver · Désinstaller |
| **Applications problématiques** | Pistage ou publicité (Facebook, TikTok, Temu…) | Désactiver · Désinstaller · Remplacer¹ |
| **Applications désactivées** | Tout ce qui est désactivé sur le téléphone | Réactiver |
| **Applications désinstallées** | Applis système retirées pour l'utilisateur | Réinstaller |

<sub>¹ « Remplacer » ouvre une fiche de l'alternative libre (NewPipe, Organic Maps, Signal…) avec son logo et sa dernière version. Le programme n'installe rien lui-même : il ouvre la fiche de l'appli dans **F-Droid** sur le téléphone (ou propose d'installer F-Droid, ou ouvre le site officiel pour les applis absentes de F-Droid). F-Droid se charge ensuite de l'installation et des mises à jour. La fiche propose ensuite de désactiver ou désinstaller l'appli d'origine.</sub>

- 🔁 **Tout est réversible** : la désinstallation utilise `pm uninstall --user 0`. L'APK reste dans le système et l'appli peut être réinstallée depuis « Applications désinstallées ». Ses données, elles, sont perdues, et une confirmation est demandée.
- 🛑 Les applis du niveau UAD-ng **« Unsafe » ne sont jamais proposées** (risque de téléphone qui ne démarre plus).
- 📂 Les sections avancée et experte sont **repliées par défaut** et **masquées si elles sont vides**.
- 💬 Au survol : nom du paquet, **description UAD-ng** (en anglais) et applis qui en dépendent.

### 🕵️ Détection de logiciels espions

À chaque connexion, le téléphone est comparé aux **indicateurs de compromission** (IOC) utilisés par MVT : environ **4 800 indicateurs** issus d'Amnesty International, d'Echap ([stalkerware-indicators](https://github.com/AssoEchap/stalkerware-indicators)), de Citizen Lab, de Google, etc.

| Vérification | Méthode |
|---|---|
| Noms de paquets | `pm list packages` |
| Empreinte SHA-256 des APK (applis tierces) | `sha256sum` calculé sur le téléphone |
| Certificat de signature des APK (SHA-1 / SHA-256) | Lecture du bloc de signature v2/v3 (v1 en secours) : seulement quelques Kio transférés par appli |
| Processus en cours | `ps -A`. Les noms identiques à un programme système Android sont écartés, car beaucoup d'indicateurs viennent d'iOS |
| Propriétés système | `getprop` |
| Fichiers connus | `/sdcard` et `/data/local/tmp`, les seuls dossiers lisibles sans root |

En plus des indicateurs, le programme signale :

- les **applis tierces avec des accès sensibles** : service d'accessibilité, administrateur de l'appareil, lecture des notifications. La mention « hors magasin » s'ajoute si l'appli a été installée depuis un fichier APK ou par ADB ;
- les **réglages affaiblis** : Play Protect désactivé, vérification des installations ADB désactivée, root (su / Magisk), bootloader déverrouillé.

⏱️ Environ 6 s à la première analyse, 2 s ensuite (empreintes mises en cache).

🧾 **Analyse fiable ou signalée incomplète** : chaque commande envoyée au téléphone se termine par un marqueur de fin, et un `adb get-state` est lancé à la fin de l'analyse. Si le téléphone disparaît en cours de route ou si une commande n'est pas allée jusqu'au bout, tout le résultat est marqué **« Analyse incomplète »** : un « aucun logiciel espion connu détecté » ne s'affiche jamais à tort comme un résultat sûr.

> [!CAUTION]
> **Si un stalkerware est détecté, ne le supprime pas tout de suite.** Sa suppression prévient souvent la personne qui l'a installé. Contacte d'abord une association spécialisée, comme [Echap](https://echap.eu.org/) en France ou la [Coalition Against Stalkerware](https://stopstalkerware.org/fr/), pour préparer ta sécurité.
> En cas de soupçon de logiciel espion étatique : [Access Now Digital Security Helpline](https://www.accessnow.org/help/) ou Amnesty Tech.

### 📄 Rapport d'analyse

Le bouton **Générer un rapport**, à côté du nom du téléphone, propose deux types de rapport :

| Type | Contenu | Format |
|---|---|---|
| **Basique** (par défaut) | Appareil, déroulement de l'analyse (complète ou non), logiciels espions, accès sensibles, réglages de sécurité contrôlés un par un, nombre d'applis par catégorie, bases utilisées et **limites de l'analyse** | **PDF** (par défaut) ou **HTML** |
| **Complet** (à venir) | Pour une analyse par un expert ou pour servir de pièce à conviction en justice | |

- Le rapport est enregistré dans le dossier **Documents** (`~/Documents` ou `XDG_DOCUMENTS_DIR`), sous un nom comme `Rapport_Ulefone_Armor_22_2026-11-27_12h31.pdf`. Un rapport existant n'est **jamais écrasé** et le fichier n'est lisible que par son propriétaire.
- L'**adresse IP** du téléphone n'y figure pas : le rapport peut être transmis à une association ou à un proche.
- Le PDF est produit par le programme lui-même, **sans dépendance** supplémentaire.

## 📦 Installation

### AppImage (le plus simple)

Télécharge `lache_mon_android-…-x86_64.AppImage` depuis les [Releases](https://github.com/codenapol/lache-mon-android/releases), puis :

```bash
chmod +x lache_mon_android-*-x86_64.AppImage
./lache_mon_android-*-x86_64.AppImage
```

Rien à compiler ni à installer : `adb`, SDL2, OpenSSL et la police sont inclus. Seul `curl` doit être présent (pour télécharger les listes). Fonctionne sur toute distribution x86_64 avec glibc ≥ 2.31 (Ubuntu 20.04, Debian 11, Fedora, Arch… et plus récentes).

> [!IMPORTANT]
> **Seule l'AppImage publiée dans les Releases de ce dépôt est officielle.** Aucun APK n'existe : le programme tourne sur le PC, jamais sur le téléphone. Vérifie l'empreinte avec `sha256sum -c lache_mon_android-*.AppImage.sha256`.

<details>
<summary>Construire l'AppImage soi-même</summary>

Nécessite Docker. La compilation se fait dans un conteneur Ubuntu 20.04 (glibc 2.31), pour que le binaire fonctionne aussi sur les distributions plus anciennes :

```bash
VERSION=1.0.0 packaging/appimage/construire.sh   # résultat dans dist/
```

</details>

### Compilation depuis les sources

#### Prérequis

Testé sous Kali / Debian.

```bash
sudo apt install build-essential libsdl2-dev libgl-dev libssl-dev libsdl2-image-dev adb curl fonts-noto-core
```

<details>
<summary>À quoi sert chaque dépendance ?</summary>

| Dépendance | Rôle |
|---|---|
| `g++` (C++17) | Compilation |
| SDL2 + OpenGL | Fenêtre et rendu |
| OpenSSL (`libcrypto`) | Lecture des certificats de signature des APK |
| SDL2_image | Logos des applis alternatives, quel que soit leur format (PNG, WebP, JPEG, SVG…) |
| `adb` | Communication avec le téléphone |
| `curl` | Téléchargement des listes |
| Noto Sans *(facultatif)* | Police de l'interface (sinon police par défaut) |

[Dear ImGui](https://github.com/ocornut/imgui) (v1.91.5) est inclus comme sous-module git dans `imgui/`. Si tu as cloné sans `--recursive` : `git submodule update --init`.

</details>

#### Compilation

```bash
git clone --recursive https://github.com/codenapol/lache-mon-android.git
cd lache-mon-android
make
./lache_mon_android
```

> [!NOTE]
> Garde `logo.bmp` et le dossier `photo de telephone/` à côté de l'exécutable : ils sont chargés au démarrage.

## 🚀 Utilisation

### 1. Activer le débogage sans fil sur le téléphone

1. **Paramètres → À propos du téléphone** → appuie 7 fois sur **Numéro de build** pour activer les options pour les développeurs.
2. **Paramètres → Système → Options pour les développeurs → Débogage sans fil** → active-le. Le PC et le téléphone doivent être sur le même Wi-Fi.
3. **Première fois seulement** : associe le PC au téléphone. Dans le programme, saisis l'adresse IP puis clique sur **Associer ce PC** (sous « Se connecter » ; le bouton est aussi proposé quand une connexion échoue faute d'association). Sur le téléphone, dans « Débogage sans fil », touche **Associer l'appareil avec un code** et garde cette fenêtre ouverte :
   - le **port d'association** se remplit tout seul quand le téléphone l'annonce sur le réseau (mDNS) ; sinon, recopie-le ;
   - saisis le **code à 6 chiffres**, puis **Associer** ;
   - une fois associé, le programme retrouve le port de connexion et se connecte tout seul.

   <details>
   <summary>En secours : association depuis le terminal</summary>

   ```bash
   adb pair IP:PORT_D_ASSOCIATION   # puis saisis le code à 6 chiffres affiché sur le téléphone
   ```
   </details>

### 2. Se connecter

Dans le programme, saisis l'**adresse IP** affichée sur l'écran « Débogage sans fil », puis :

- clique sur **Découverte automatique** à côté du champ Port : le programme retrouve le port tout seul ;
- ou recopie le **port** affiché sur le téléphone.

Clique ensuite sur **Se connecter**.

> [!TIP]
> Le port de connexion n'est **pas** le même que celui de l'association, et il change à chaque activation du débogage sans fil : c'est là que la découverte automatique sert.

<details>
<summary>Comment fonctionne la découverte automatique ?</summary>

1. Le programme vérifie que l'adresse saisie est une adresse IPv4 valide.
2. Il cherche le port annoncé par le téléphone sur le réseau local (**mDNS**, `adb mdns services`).
3. Sinon (PC et téléphone sur des sous-réseaux différents, mDNS bloqué…), il **scanne les ports 30000 à 60999**, où Android place le débogage sans fil, ainsi que 5555. Comptez 1 à 3 secondes.
4. Chaque port ouvert est **vérifié avec le protocole adb** : un autre service ouvert sur le téléphone n'est jamais pris pour le bon port. Rien n'est demandé ni modifié sur le téléphone.

Une fenêtre d'erreur explique le problème si le téléphone ne répond pas, si aucun port de débogage n'est trouvé, ou si plusieurs ports possibles empêchent de choisir.

</details>

### 3. Télécharger les listes

Au premier lancement, clique sur **Télécharger** (puis plus tard sur **Mettre à jour**). Ce lien récupère à la fois la base anti-espions et la liste de debloat UAD-ng.

<details>
<summary>Où sont enregistrées les données ?</summary>

Dans `~/.local/share/lache_mon_android/` (ou `$XDG_DATA_HOME/lache_mon_android/`) :

```
lache_mon_android/
├── indicateurs/      # fichiers STIX2 des indicateurs MVT + sources.tsv + derniere_maj
├── alternatives/     # version et logo des alternatives (cache, pour le mode hors ligne)
└── uad_lists.json    # liste de debloat UAD-ng
```

Si [MVT](https://github.com/mvt-project/mvt) est installé, ses propres indicateurs (`~/.local/share/mvt/indicators/`) sont aussi lus.

</details>

## 🗂️ Structure du code

```
.
├── main.cpp              # Interface (ImGui), connexion ADB, détection des applis, actions
├── securite.cpp / .h     # Logiciels espions : indicateurs STIX2, analyse, certificats d'APK
├── uad.cpp / .h          # Téléchargement et lecture de la liste UAD-ng
├── alternatives.cpp / .h # Alternatives libres : catalogue, version et logo (F-Droid ou éditeur)
├── decouverte.cpp / .h   # Découverte automatique du port du débogage sans fil (mDNS, scan, sonde adb)
├── association.cpp / .h  # Association du PC au téléphone (adb pair, code à 6 chiffres)
├── rapport.cpp / .h      # Rapports d'analyse : écriture en PDF (sans bibliothèque) ou en HTML
├── json.h                # Mini lecteur JSON sans dépendance
├── outils.cpp / .h       # Commandes shell (avec marqueur de fin), téléchargement, chemins de données
├── imgui/                # Dear ImGui (sous-module git)
├── photo de telephone/   # Photos des modèles reconnus
├── logo.bmp              # Icône de la fenêtre
└── Makefile
```

- Les commandes ADB tournent dans un **thread séparé** pour ne pas figer la fenêtre.
- Les noms de paquets, adresses et chemins insérés dans les commandes shell sont **validés** (caractères autorisés uniquement).
- Modèles reconnus pour la photo : Ulefone Armor 22, Google Pixel 6, Google Pixel 11, Samsung Galaxy S26. Les autres modèles s'affichent avec la photo par défaut, et un menu déroulant permet d'en choisir une autre.

## ⚠️ Limites connues

- **Linux uniquement** pour l'instant.
- **Testé sur un seul téléphone** (Ulefone Armor 22). Les sorties de `dumpsys` varient selon les constructeurs : d'autres modèles peuvent donner des résultats incomplets.
- **Sans root, une partie des indicateurs est inexploitable** : les quelque 6 800 indicateurs de domaines, d'URL et d'adresses IP concernent surtout les SMS et l'historique de navigation, illisibles via ADB sur un Android récent.
- La section « Accès sensibles » liste aussi des **applis légitimes** (gestionnaires de mots de passe, blocage d'appels…). Vérifie avant de supprimer.
- Les descriptions UAD-ng sont **en anglais**.

## 🛣️ Feuille de route

- [x] Bouton « Remplacer » (fiche de l'alternative libre, installation via F-Droid)
- [ ] Tutoriel intégré (lien « Lancer le tuto »)
- [x] Association ADB (`adb pair`) depuis l'interface
- [ ] Avertissement de sécurité avant la suppression d'un stalkerware détecté
- [x] Découverte automatique du port du débogage sans fil
- [x] Rapport d'analyse basique (PDF ou HTML)
- [ ] Rapport d'analyse complet (expertise, pièce à conviction)
- [ ] Historique des installations et désinstallations (traces d'un espion déjà supprimé)
- [ ] Version Windows
- [ ] Traduction française des descriptions UAD-ng

## 📜 Crédits et licences

| Composant | Utilisation | Licence |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | Interface, sous-module `imgui/` (v1.91.5) | MIT |
| [UAD-ng](https://github.com/Universal-Debloater-Alliance/universal-android-debloater-next-generation) | Liste de debloat, **téléchargée à l'exécution** | GPL-3.0 |
| [mvt-indicators](https://github.com/mvt-project/mvt-indicators), [Amnesty Tech](https://github.com/AmnestyTech/investigations), [Echap](https://github.com/AssoEchap/stalkerware-indicators) | Indicateurs de compromission, **téléchargés à l'exécution** | Voir chaque dépôt |
| SDL2, SDL2_image, OpenSSL | Bibliothèques système | zlib, zlib, Apache-2.0 |
| [F-Droid](https://f-droid.org), sites des éditeurs | Version et logo des alternatives, **téléchargés à l'exécution** | Voir chaque appli |

> [!IMPORTANT]
> Les listes (UAD-ng, indicateurs) sont **téléchargées** à l'exécution plutôt qu'embarquées : elles restent ainsi toujours à jour, et chaque source garde sa propre licence.


**Licence de ce projet :** [GNU GPL v3.0 ou ultérieure](LICENSE). Copyright © 2026 codenapol.
