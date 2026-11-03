#!/bin/bash
# Baut TANGENTA in einem frischen Container und packt ein .deb mit eigenem Qt.
# Läuft IM Container (siehe package-linux.sh):
#   /src    Quellcode (nur lesend)
#   /cache  Qt aus aqtinstall (wird von allen Distributionen geteilt)
#   /out    fertige Pakete
#
#   build-deb.sh <ziel>    ziel = debian12 | debian13 | ubuntu24.04 | ubuntu26.04
#
# Warum Qt mitkommt: Debian 12 und Ubuntu 24.04 haben nur Qt 6.4, TANGENTA braucht 6.5 oder
# neuer. Mit dem eigenen Qt 6.10 verhält sich das Programm auf allen Systemen gleich wie unter
# Windows. Alles liegt unter /opt/tangenta, /usr/bin/tangenta ist ein Verweis darauf.
set -euo pipefail

TARGET=${1:?Ziel fehlt (debian12, debian13, ubuntu24.04, ubuntu26.04)}
QT_VERSION=${QT_VERSION:-6.10.3}
QT_DIR=/cache/qt/$QT_VERSION/gcc_64
PREFIX=/opt/tangenta
export DEBIAN_FRONTEND=noninteractive

log() { printf '\n=== %s\n' "$*"; }

# Erstes vorhandene Paket aus einer Liste (Namen mit t64 ab Debian 13/Ubuntu 24.04)
pick() { for p in "$@"; do apt-cache show "$p" >/dev/null 2>&1 && { echo "$p"; return; }; done; echo "$1"; }

log "Werkzeuge und Bibliotheken ($TARGET)"
apt-get update -q
apt-get install -y -q --no-install-recommends \
    g++ cmake ninja-build python3 python3-venv patchelf file dpkg-dev ca-certificates fakeroot xz-utils \
    libgl1-mesa-dev libegl-dev libopengl0 libxkbcommon-dev libxkbcommon-x11-0 libfontconfig1-dev libfreetype-dev \
    libx11-xcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 \
    libxcb-shape0 libxcb-sync1 libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 libxcb-shm0 libxcb-render0 \
    "$(pick libxcb-util1 libxcb-util0)" libdbus-1-3 "$(pick libcups2t64 libcups2)" "$(pick libglib2.0-0t64 libglib2.0-0)" \
    "$(pick libssl3t64 libssl3)" libwayland-client0 libwayland-cursor0 libwayland-egl1 libgomp1 zlib1g \
    fonts-dejavu-core fonts-liberation fontconfig "$(pick libgs10 libgs9)"

log "Qt $QT_VERSION"
if [ ! -x "$QT_DIR/bin/qmake" ] && [ ! -x "$QT_DIR/bin/qtpaths" ]; then
    python3 -m venv /tmp/aqt
    /tmp/aqt/bin/pip install -q aqtinstall
    /tmp/aqt/bin/aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 -m qtimageformats -O /cache/qt
fi

# Das TIFF-Plugin aus aqtinstall braucht libtiff.so.5 (RHEL 9), Debian und Ubuntu haben nur .so.6:
# einmal je System aus den Qt-Quellen bauen, mit eingebautem libtiff
OVERLAY=/cache/imageformats-$TARGET
if [ ! -f "$OVERLAY/imageformats/libqtiff.so" ]; then
    log "TIFF-Plugin aus den Qt-Quellen"
    SRCURL="https://download.qt.io/official_releases/qt/${QT_VERSION%.*}/$QT_VERSION/submodules/qtimageformats-everywhere-src-$QT_VERSION.tar.xz"
    rm -rf /tmp/qif && mkdir -p /tmp/qif && cd /tmp/qif
    python3 -c "import urllib.request,sys; urllib.request.urlretrieve(sys.argv[1], 'src.tar.xz')" "$SRCURL"
    tar xf src.tar.xz
    mkdir build && cd build
    "$QT_DIR/bin/qt-configure-module" ../qtimageformats-everywhere-src-$QT_VERSION -- -G Ninja         -DFEATURE_system_tiff=OFF -DFEATURE_system_webp=OFF -DFEATURE_jasper=OFF -DFEATURE_mng=OFF >/dev/null
    cmake --build . --target QTiffPlugin
    mkdir -p "$OVERLAY/imageformats"
    cp "$(find . -name libqtiff.so | head -1)" "$OVERLAY/imageformats/"
    cd / && rm -rf /tmp/qif
fi
export QT_PLUGIN_PATH="$OVERLAY:$QT_DIR/plugins"

log "Bauen und testen"
rm -rf /build && mkdir -p /build
cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_DIR" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib' -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF
cmake --build /build -j"$(nproc)"
(cd /build && QT_QPA_PLATFORM=offscreen VEK_REQUIRE_TIFF=1 LD_LIBRARY_PATH="$QT_DIR/lib" ctest --output-on-failure)

log "Paketinhalt"
STAGE=/tmp/stage
rm -rf "$STAGE" && mkdir -p "$STAGE"
DESTDIR="$STAGE" cmake --install /build
# Startmenü, Symbole und Dateityp gehören nach /usr/share, das Programm bleibt unter /opt
mkdir -p "$STAGE/usr/share" "$STAGE/usr/bin" "$STAGE/usr/share/mime/packages"
mv "$STAGE$PREFIX/share/applications" "$STAGE/usr/share/"
mv "$STAGE$PREFIX/share/icons" "$STAGE/usr/share/"
cp /src/packaging/linux/tangenta-mime.xml "$STAGE/usr/share/mime/packages/tangenta.xml"
ln -s "$PREFIX/bin/tangenta" "$STAGE/usr/bin/tangenta"
mkdir -p "$STAGE/usr/share/doc/tangenta"
cp /src/LICENSE "$STAGE/usr/share/doc/tangenta/copyright"

# Qt beilegen: Bibliotheken, die das Programm und die Plugins aus $QT_DIR/lib brauchen
LIB="$STAGE$PREFIX/lib"
PLUG="$STAGE$PREFIX/plugins"
mkdir -p "$LIB" "$PLUG"
for d in platforms xcbglintegrations imageformats iconengines tls printsupport platforminputcontexts \
         wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration networkinformation; do
    [ -d "$QT_DIR/plugins/$d" ] && cp -a "$QT_DIR/plugins/$d" "$PLUG/"
done
cp "$OVERLAY/imageformats/libqtiff.so" "$PLUG/imageformats/"
# Nur Plattformen, die wir unterstützen (X11, Wayland, ohne Bildschirm)
find "$PLUG/platforms" -name '*.so' ! -name 'libqxcb.so' ! -name 'libqwayland*.so' ! -name 'libqoffscreen.so' -delete
rm -f "$PLUG/platforms/libqwayland-egl.so.debug" 2>/dev/null || true
copy_qt_deps() {
    local changed=1
    while [ $changed = 1 ]; do
        changed=0
        while read -r so; do
            for dep in $(LD_LIBRARY_PATH="$QT_DIR/lib" ldd "$so" 2>/dev/null | awk '/=>/ {print $3}' | grep "^$QT_DIR/lib/" || true); do
                base=$(basename "$dep")
                if [ ! -e "$LIB/$base" ]; then
                    cp -L "$dep" "$LIB/$base"
                    changed=1
                fi
            done
        done < <(find "$STAGE$PREFIX/bin" "$LIB" "$PLUG" -type f \( -name '*.so*' -o -perm -u+x \))
    done
}
copy_qt_deps
# Wo Programm und Plugins Qt finden
for so in "$LIB"/*.so*; do patchelf --set-rpath '$ORIGIN' "$so"; done
find "$PLUG" -name '*.so' -exec patchelf --set-rpath '$ORIGIN/../../lib' {} \;
patchelf --set-rpath '$ORIGIN/../lib' "$STAGE$PREFIX/bin/tangenta"
cat > "$STAGE$PREFIX/bin/qt.conf" <<EOF
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
Translations = translations
EOF
if [ -d "$QT_DIR/translations" ]; then
    mkdir -p "$STAGE$PREFIX/translations"
    for lang in de en fr it es pt_BR zh_CN; do
        cp "$QT_DIR"/translations/qt*_$lang.qm "$STAGE$PREFIX/translations/" 2>/dev/null || true
    done
fi
strip --strip-unneeded "$STAGE$PREFIX/bin/tangenta" "$LIB"/*.so* 2>/dev/null || true
find "$PLUG" -name '*.so' -exec strip --strip-unneeded {} \; 2>/dev/null || true

log "Abhängigkeiten ermitteln"
VERSION=$(sed -n 's/^project(Tangenta VERSION \([0-9.]*\).*/\1/p' /src/CMakeLists.txt)
WORK=/tmp/shlibs
rm -rf "$WORK" && mkdir -p "$WORK/debian"
printf 'Source: tangenta\n\nPackage: tangenta\nArchitecture: amd64\n' > "$WORK/debian/control"
mapfile -t ELFS < <(find "$STAGE$PREFIX" -type f -exec sh -c 'file -b "$1" | grep -q "^ELF" && echo "$1"' _ {} \;)
(cd "$WORK" && dpkg-shlibdeps -O --ignore-missing-info -l"$LIB" "${ELFS[@]}" 2>/dev/null) > "$WORK/substvars"
DEPENDS=$(sed -n 's/^shlibs:Depends=//p' "$WORK/substvars")
[ -n "$DEPENDS" ] || { echo "Keine Abhängigkeiten ermittelt"; exit 1; }
# Laufzeit-Plugins von xcb, die ldd nicht als direkte Abhängigkeit sieht, stehen schon drin; dazu Schriften
DEPENDS="$DEPENDS, fontconfig, fonts-dejavu-core | fonts-liberation, shared-mime-info, desktop-file-utils"

case "$TARGET" in
    debian12) SUFFIX="debian12" ;;
    debian13) SUFFIX="debian13" ;;
    ubuntu24.04) SUFFIX="ubuntu24.04" ;;
    ubuntu26.04) SUFFIX="ubuntu26.04" ;;
    *) SUFFIX="$TARGET" ;;
esac
SIZE=$(du -sk "$STAGE" | cut -f1)
mkdir -p "$STAGE/DEBIAN"
cat > "$STAGE/DEBIAN/control" <<EOF
Package: tangenta
Version: ${VERSION}-1~${SUFFIX}
Architecture: amd64
Maintainer: Intersoft Networks <info@intersoft-networks.ch>
Installed-Size: $SIZE
Depends: $DEPENDS
Recommends: libgs10 | libgs9
Section: graphics
Priority: optional
Homepage: https://tangenta.ch
Description: Vektorgrafik-Editor im klassischen Stil
 TANGENTA ist ein freier Vektorgrafik-Editor (GPLv3) für Linux und Windows mit
 Seiten und Ebenen, Kurvenwerkzeugen, Text, Tabellen, Effekten, SuperTrace
 (Bitmaps nachzeichnen), Bildersuche, KI-Assistent, PDF-, SVG- und Bildexport.
 Bringt ein eigenes Qt 6 mit (unter /opt/tangenta).
EOF
cat > "$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = configure ]; then
    command -v update-mime-database >/dev/null && update-mime-database /usr/share/mime || true
    command -v update-desktop-database >/dev/null && update-desktop-database -q /usr/share/applications || true
    command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
fi
EOF
cp "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm"
sed -i 's/\[ "\$1" = configure \]/[ "$1" = remove ] || [ "$1" = purge ]/' "$STAGE/DEBIAN/postrm"
chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm"
OUT="/out/tangenta_${VERSION}-1~${SUFFIX}_amd64.deb"
dpkg-deb --root-owner-group -Zxz --build "$STAGE" "$OUT"
dpkg-deb --info "$OUT" | sed -n '/Package:/,/Description:/p'
ls -l "$OUT"
