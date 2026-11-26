#!/bin/sh
# Lâche mon Android ! — compilation et assemblage de l'AppImage (exécuté DANS le conteneur)
# Copyright (C) 2026 codenapol — GPL-3.0-or-later
#
# Entrée standard : archive tar des fichiers suivis par git   /out : dossier de sortie
set -eu

TRAVAIL=/tmp/construction
rm -rf "$TRAVAIL"
mkdir -p "$TRAVAIL"
tar -C "$TRAVAIL" -xf -
cd "$TRAVAIL"

# --- Compilation (les warnings sont des erreurs, comme dans la CI) ---
make clean
make -j"$(nproc)" CXX="g++-10 -Werror -O2"

# Vérifie que l'OpenSSL 1.1 du système n'a pas été lié par erreur
if ldd lache_mon_android | grep -q libcrypto; then
    echo "ERREUR : libcrypto liée dynamiquement (OpenSSL statique attendu)" >&2
    exit 1
fi

# --- Arborescence AppDir ---
# Tout ce que le programme charge via SDL_GetBasePath() va à côté de l'exécutable
APPDIR="$TRAVAIL/AppDir"
BIN="$APPDIR/usr/bin"
DOC="$APPDIR/usr/share/doc/lache_mon_android"
mkdir -p "$BIN" "$DOC"

cp logo.bmp "$BIN/"
cp -r "photo de telephone" "$BIN/"
cp /usr/share/fonts/truetype/noto/NotoSans-Regular.ttf \
   /usr/share/fonts/truetype/noto/NotoSans-Bold.ttf "$BIN/"
cp /opt/adb/adb "$BIN/"

# Licences de tout ce qui est redistribué
cp LICENSE                                    "$DOC/LICENSE"
cp /opt/openssl/LICENSE.txt                   "$DOC/LICENSE.openssl"
cp /opt/adb/NOTICE.txt                        "$DOC/NOTICE.adb"
cp /usr/share/doc/fonts-noto-core/copyright   "$DOC/LICENSE.noto"
cp imgui/LICENSE.txt                          "$DOC/LICENSE.imgui"

# --- AppImage ---
# linuxdeploy copie SDL2, SDL2_image et leurs dépendances (libpng, libwebp…), mais jamais
# la glibc ni les pilotes OpenGL, qui doivent venir du système de l'utilisateur.
cd "$TRAVAIL"
export LINUXDEPLOY_OUTPUT_VERSION="${VERSION:-dev}"
export OUTPUT="lache_mon_android-${LINUXDEPLOY_OUTPUT_VERSION}-x86_64.AppImage"
cp logo.png lache_mon_android.png
# 1. Copie des bibliothèques dans l'AppDir
/opt/linuxdeploy/AppRun \
    --appdir "$APPDIR" \
    --executable lache_mon_android \
    --desktop-file packaging/appimage/lache_mon_android.desktop \
    --icon-file lache_mon_android.png

# 2. Hygiène : toutes les dates fixées à SOURCE_DATE_EPOCH (date du commit), pour ne révéler
#    ni l'heure réelle de construction ni le fuseau horaire. mksquashfs ne fait que plafonner
#    les dates à SOURCE_DATE_EPOCH : on les force donc nous-mêmes.
find "$APPDIR" -exec touch -h -d "@${SOURCE_DATE_EPOCH:-0}" {} +

# 3. Compression en AppImage, sans redéployer (les dates restent celles fixées)
/opt/linuxdeploy/usr/bin/linuxdeploy-plugin-appimage --appdir "$APPDIR"

cp "$OUTPUT" /out/
cd /out
sha256sum "$OUTPUT" > "$OUTPUT.sha256"
echo
echo "AppImage prête : $OUTPUT"
cat "$OUTPUT.sha256"
