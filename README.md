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

<sub>¹ Le bouton « Remplacer » (proposition d'une alternative libre : NewPipe, Organic Maps, Signal…) est affiché mais **pas encore fonctionnel**.</sub>

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

> [!CAUTION]
> **Si un stalkerware est détecté, ne le supprime pas tout de suite.** Sa suppression prévient souvent la personne qui l'a installé. Contacte d'abord une association spécialisée, comme [Echap](https://echap.eu.org/) en France ou la [Coalition Against Stalkerware](https://stopstalkerware.org/fr/), pour préparer ta sécurité.
> En cas de soupçon de logiciel espion étatique : [Access Now Digital Security Helpline](https://www.accessnow.org/help/) ou Amnesty Tech.

## 📦 Installation

### Prérequis

Testé sous Kali / Debian.

```bash
sudo apt install build-essential libsdl2-dev libgl-dev libssl-dev adb curl fonts-noto-core
```

<details>
<summary>À quoi sert chaque dépendance ?</summary>

| Dépendance | Rôle |
|---|---|
| `g++` (C++17) | Compilation |
| SDL2 + OpenGL | Fenêtre et rendu |
| OpenSSL (`libcrypto`) | Lecture des certificats de signature des APK |
| `adb` | Communication avec le téléphone |
| `curl` | Téléchargement des listes |
| Noto Sans *(facultatif)* | Police de l'interface (sinon police par défaut) |

[Dear ImGui](https://github.com/ocornut/imgui) (v1.91.5) est inclus comme sous-module git dans `imgui/`. Si tu as cloné sans `--recursive` : `git submodule update --init`.

</details>

### Compilation

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
3. **Première fois seulement** : apparie le PC. Dans « Débogage sans fil », touche **Associer l'appareil avec un code**, puis lance sur le PC :
   ```bash
   adb pair IP:PORT_D_ASSOCIATION   # puis saisis le code à 6 chiffres affiché sur le téléphone
   ```
   Le programme ne gère pas encore cette étape.

### 2. Se connecter

Dans le programme, saisis l'**adresse IP** et le **port** affichés sur l'écran « Débogage sans fil », puis clique sur **Se connecter**.

> [!TIP]
> Le port de connexion n'est **pas** le même que celui de l'association, et il change à chaque activation du débogage sans fil.

### 3. Télécharger les listes

Au premier lancement, clique sur **Télécharger** (puis plus tard sur **Mettre à jour**). Ce lien récupère à la fois la base anti-espions et la liste de debloat UAD-ng.

<details>
<summary>Où sont enregistrées les données ?</summary>

Dans `~/.local/share/lache_mon_android/` (ou `$XDG_DATA_HOME/lache_mon_android/`) :

```
lache_mon_android/
├── indicateurs/      # fichiers STIX2 des indicateurs MVT + sources.tsv + derniere_maj
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
├── json.h                # Mini lecteur JSON sans dépendance
├── outils.cpp / .h       # Commandes shell, téléchargement, chemins de données
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

- [ ] Bouton « Remplacer » (installation de l'alternative libre)
- [ ] Tutoriel intégré (lien « Lancer le tuto »)
- [ ] Appariement ADB (`adb pair`) depuis l'interface
- [ ] Avertissement de sécurité avant la suppression d'un stalkerware détecté
- [ ] Export d'un rapport d'analyse
- [ ] Historique des installations et désinstallations (traces d'un espion déjà supprimé)
- [ ] Version Windows
- [ ] Traduction française des descriptions UAD-ng

## 📜 Crédits et licences

| Composant | Utilisation | Licence |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | Interface, sous-module `imgui/` (v1.91.5) | MIT |
| [UAD-ng](https://github.com/Universal-Debloater-Alliance/universal-android-debloater-next-generation) | Liste de debloat, **téléchargée à l'exécution** | GPL-3.0 |
| [mvt-indicators](https://github.com/mvt-project/mvt-indicators), [Amnesty Tech](https://github.com/AmnestyTech/investigations), [Echap](https://github.com/AssoEchap/stalkerware-indicators) | Indicateurs de compromission, **téléchargés à l'exécution** | Voir chaque dépôt |
| SDL2, OpenSSL | Bibliothèques système | zlib, Apache-2.0 |

> [!IMPORTANT]
> Les listes (UAD-ng, indicateurs) sont **téléchargées** à l'exécution plutôt qu'embarquées : elles restent ainsi toujours à jour, et chaque source garde sa propre licence.


**Licence de ce projet :** [GNU GPL v3.0 ou ultérieure](LICENSE). Copyright © 2026 codenapol.
