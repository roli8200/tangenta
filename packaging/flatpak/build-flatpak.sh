#!/bin/bash
# Baut TANGENTA als Flatpak und legt ein einzelnes Installationspaket ab:
#   build/packages/TANGENTA-<version>-x86_64.flatpak
#
#   packaging/flatpak/build-flatpak.sh
#
# Voraussetzungen: flatpak und flatpak-builder (sudo apt install flatpak flatpak-builder).
# Die KDE-Laufzeit kommt von Flathub; beim Installieren des Pakets holt flatpak sie selbst:
#   flatpak install --user TANGENTA-<version>-x86_64.flatpak
set -euo pipefail
SRC=$(cd "$(dirname "$0")/../.." && pwd)
APP=ch.tangenta.Tangenta
VERSION=$(sed -n 's/^project(Tangenta VERSION \([0-9.]*\).*/\1/p' "$SRC/CMakeLists.txt")
WORK=${TANGENTA_FLATPAK_WORK:-$HOME/.cache/tangenta-flatpak}
OUT="$SRC/build/packages"
mkdir -p "$WORK" "$OUT"

flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user -y --noninteractive flathub org.kde.Platform//6.10 org.kde.Sdk//6.10

flatpak-builder --user --force-clean --disable-rofiles-fuse \
    --state-dir="$WORK/state" --repo="$WORK/repo" \
    "$WORK/app" "$SRC/packaging/flatpak/$APP.yml"

flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    "$WORK/repo" "$OUT/TANGENTA-$VERSION-x86_64.flatpak" "$APP"
echo "Fertig: $OUT/TANGENTA-$VERSION-x86_64.flatpak"
