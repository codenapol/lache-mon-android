# Lâche mon Android ! — environnement de construction de l'AppImage
# Copyright (C) 2026 codenapol — GPL-3.0-or-later
#
# Une AppImage n'embarque pas la glibc : elle utilise celle du système de l'utilisateur.
# On compile donc sur une distribution ancienne (Ubuntu 20.04, glibc 2.31) pour que le
# binaire fonctionne sur toutes les distributions plus récentes.
#
# Tous les téléchargements sont épinglés (version + empreinte SHA-256).

FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++-10 make perl pkg-config \
        libsdl2-dev libsdl2-image-dev libgl-dev \
        curl ca-certificates unzip file xz-utils \
        fonts-noto-core \
    && rm -rf /var/lib/apt/lists/*

# --- OpenSSL 3.5 LTS, en bibliothèque statique ---
# Ubuntu 20.04 fournit OpenSSL 1.1, dont la licence est incompatible avec la GPL pour la
# distribution d'un binaire. OpenSSL 3 (Apache-2.0) l'est : on le compile et on le lie
# statiquement (seule libcrypto est utilisée).
ARG OPENSSL_VERSION=3.5.9
ARG OPENSSL_SHA256=603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a
RUN cd /tmp \
    && curl -sSfL -o openssl.tar.gz \
       "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz" \
    && echo "${OPENSSL_SHA256}  openssl.tar.gz" | sha256sum -c - \
    && tar xf openssl.tar.gz && cd "openssl-${OPENSSL_VERSION}" \
    && ./Configure linux-x86_64 no-shared no-tests no-apps no-docs no-module \
         --prefix=/opt/openssl --libdir=lib \
    && make -j"$(nproc)" build_libs \
    && make install_dev \
    && cp LICENSE.txt /opt/openssl/LICENSE.txt \
    && cd / && rm -rf /tmp/openssl*

# --- adb (Android SDK Platform-Tools) ---
# Version récente nécessaire pour le débogage sans fil (adb pair, connexion TLS).
# Ne dépend que de glibc 2.16 : fonctionne partout.
ARG PLATFORM_TOOLS_VERSION=37.0.3
ARG PLATFORM_TOOLS_SHA256=214548146cadb2ca3508143d8a1d4be4c59de5bb837253e3ede89ed74cf450d1
RUN cd /tmp \
    && curl -sSfL -o pt.zip \
       "https://dl.google.com/android/repository/platform-tools_r${PLATFORM_TOOLS_VERSION}-linux.zip" \
    && echo "${PLATFORM_TOOLS_SHA256}  pt.zip" | sha256sum -c - \
    && unzip -q pt.zip \
    && mkdir -p /opt/adb \
    && cp platform-tools/adb platform-tools/NOTICE.txt /opt/adb/ \
    && rm -rf /tmp/pt.zip /tmp/platform-tools

# --- linuxdeploy (copie les bibliothèques et fabrique l'AppImage) ---
ARG LINUXDEPLOY_VERSION=1-alpha-20251107-1
ARG LINUXDEPLOY_SHA256=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
RUN curl -sSfL -o /opt/linuxdeploy.AppImage \
       "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-x86_64.AppImage" \
    && echo "${LINUXDEPLOY_SHA256}  /opt/linuxdeploy.AppImage" | sha256sum -c - \
    && chmod 755 /opt/linuxdeploy.AppImage \
    && cd /opt && ./linuxdeploy.AppImage --appimage-extract >/dev/null \
    && mv squashfs-root linuxdeploy && rm linuxdeploy.AppImage \
    && chmod -R a+rX /opt/linuxdeploy
# (extrait une fois pour toutes : pas de FUSE dans un conteneur)

# OpenSSL statique trouvé avant celui du système par -lcrypto et #include <openssl/...>
ENV CPLUS_INCLUDE_PATH=/opt/openssl/include LIBRARY_PATH=/opt/openssl/lib
