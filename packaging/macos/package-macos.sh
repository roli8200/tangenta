#!/bin/bash
# Macht aus dem gebauten TANGENTA.app ein eigenständiges Programmpaket und ein Disk-Image.
#
#   packaging/macos/package-macos.sh <build-ordner> <qt-ordner>
#
# Ohne MACOS_SIGN_IDENTITY wird ad hoc signiert (startet nach Rechtsklick > Öffnen); mit einer
# «Developer ID Application»-Identität im Schlüsselbund wird mit Hardened Runtime signiert.
# Ergebnis: <build-ordner>/TANGENTA-<version>-macos.dmg
set -euo pipefail

BUILD=$(cd "$1" && pwd)
QT=$(cd "$2" && pwd)
APP="$BUILD/TANGENTA.app"
[ -d "$APP" ] || { echo "Fehlt: $APP"; exit 1; }
VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$APP/Contents/Info.plist")

# Qt-Bibliotheken und Plugins (Bildformate, Plattform, Druck, SVG) ins Paket
"$QT/bin/macdeployqt" "$APP" -always-overwrite

# Qt-eigene Übersetzungen (Abbrechen, Ja/Nein, Dateidialog) für die Oberflächensprachen
mkdir -p "$APP/Contents/Resources/translations"
for lang in en fr it es pt_BR zh_CN de; do
    for f in "$QT/translations/qt_$lang.qm" "$QT/translations/qtbase_$lang.qm"; do
        [ -f "$f" ] && cp "$f" "$APP/Contents/Resources/translations/"
    done
done

if [ -n "${MACOS_SIGN_IDENTITY:-}" ]; then
    codesign --force --deep --options runtime --timestamp --sign "$MACOS_SIGN_IDENTITY" "$APP"
else
    codesign --force --deep --sign - "$APP"
fi
codesign --verify --deep --strict "$APP"

# Disk-Image mit Verknüpfung auf den Programme-Ordner zum Hineinziehen
STAGE="$BUILD/dmg"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
DMG="$BUILD/TANGENTA-$VERSION-macos.dmg"
rm -f "$DMG"
hdiutil create -volname "TANGENTA $VERSION" -srcfolder "$STAGE" -fs HFS+ -format UDZO "$DMG"
rm -rf "$STAGE"
echo "$DMG"
