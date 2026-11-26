#!/bin/sh
# Lâche mon Android ! — construit l'AppImage dans un conteneur Ubuntu 20.04 (glibc 2.31)
# Copyright (C) 2026 codenapol — GPL-3.0-or-later
#
# Utilisation (depuis la racine du projet) :
#   packaging/appimage/construire.sh            → version « dev »
#   VERSION=1.0.0 packaging/appimage/construire.sh
#
# Nécessite Docker. Le résultat est déposé dans dist/.
set -eu

RACINE="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE=lache-mon-android-appimage

if [ ! -f "$RACINE/imgui/imgui.cpp" ]; then
    echo "Sous-module imgui absent : lance « git submodule update --init »" >&2
    exit 1
fi

# Hygiène : toutes les dates de l'AppImage sont fixées à celle du dernier commit, pour ne
# révéler ni l'heure réelle de construction ni le fuseau horaire (build reproductible).
if [ -z "${SOURCE_DATE_EPOCH:-}" ]; then
    SOURCE_DATE_EPOCH="$(git -C "$RACINE" log -1 --format=%ct 2>/dev/null || echo 0)"
fi

docker build -t "$IMAGE" "$RACINE/packaging/appimage"

mkdir -p "$RACINE/dist"
# Seuls les fichiers suivis par git (sous-module imgui compris) sont envoyés au conteneur :
# aucun fichier local non publié ne peut se retrouver dans l'AppImage.
git -C "$RACINE" ls-files -z --recurse-submodules \
  | tar -C "$RACINE" --null -T - -cf - \
  | docker run --rm -i \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e VERSION="${VERSION:-dev}" \
    -e SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" \
    -e TZ=UTC \
    -v "$RACINE/packaging/appimage/dans_le_conteneur.sh:/dans_le_conteneur.sh:ro" \
    -v "$RACINE/dist:/out" \
    "$IMAGE" sh /dans_le_conteneur.sh
