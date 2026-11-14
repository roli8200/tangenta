# Download – TANGENTA

> TANGENTA herunterladen: Setup und MSI für Windows 10/11, .deb für Debian 12/13 und Ubuntu 24.04/26.04. Kostenlos, mit stiller Installation.

URL: https://tangenta.ch/download.html

Download · Version 0.91

# TANGENTA  herunterladen.

Kostenlos für Windows, macOS und Linux. Alle Pakete liegen auf GitHub bei der aktuellen Version.

### Windows 10 / 11

Setup mit Assistent oder MSI für die Verteilung im Netzwerk, still installierbar.

[Setup herunterladen](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x64-setup.exe)

[MSI für die Verteilung →](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x64.msi)

```
TANGENTA-0.91-x64-setup.exe /quiet
msiexec /i TANGENTA-0.91-x64.msi /qn
```

### macOS 13 oder neuer

Ein Programm für Macs mit Apple-Chip und mit Intel-Prozessor, als Disk-Image.

[Disk-Image herunterladen](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-macos.dmg)

```
xattr -dr com.apple.quarantine /Applications/TANGENTA.app
```

### Debian und Ubuntu

Pakete für Debian 12, Debian 13, Ubuntu 24.04 und 26.04, mit eigenem Qt 6.10.

[.deb herunterladen (Debian 13)](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian13_amd64.deb)

Pakete je System:

- [Debian 12](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian12_amd64.deb)
- [Debian 13](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian13_amd64.deb)
- [Ubuntu 24.04](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.ubuntu24.04_amd64.deb)
- [Ubuntu 26.04](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.ubuntu26.04_amd64.deb)

```
sudo apt install ./tangenta_0.91-1.debian13_amd64.deb
```

### Andere Linux-Systeme

Flatpak für Fedora, openSUSE, Arch, Mint und alle Systeme mit Flatpak. Die Laufzeit kommt von Flathub.

[Flatpak herunterladen](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x86_64.flatpak)

```
flatpak install --user TANGENTA-0.91-x86_64.flatpak
flatpak run ch.tangenta.Tangenta
```

### GitHub

[Alle Dateien und Prüfsummen →](https://github.com/roli8200/tangenta/releases/tag/v0.91)

[SHA256SUMS-0.91.txt](https://github.com/roli8200/tangenta/releases/download/v0.91/SHA256SUMS-0.91.txt)

```
sha256sum --ignore-missing -c SHA256SUMS-0.91.txt
```

**01** / Windows

## Installieren unter Windows

1. **TANGENTA-0.91-x64-setup.exe** herunterladen und starten.
2. Lizenz bestätigen und **Installieren** wählen. Das Setup braucht Administratorrechte und installiert für alle Benutzer.
3. TANGENTA im Startmenü öffnen. `.vek`-Dateien öffnen sich danach per Doppelklick.

Zeigt Windows „Der Computer wurde durch Windows geschützt“, klicken Sie auf **Weitere Informationen** und dann **Trotzdem ausführen**. Die Pakete sind noch nicht digital signiert.

**02** / Linux

## Installieren unter Linux

1. Das Paket für Ihr System herunterladen:
2. Im Terminal installieren: `sudo apt install ./tangenta_0.91-1.debian12_amd64.deb` (Dateiname anpassen).
3. TANGENTA im Startmenü öffnen oder `tangenta` eingeben. Entfernen mit `sudo apt remove tangenta`.

```
tangenta_0.91-1.debian12_amd64.deb
tangenta_0.91-1.debian13_amd64.deb
tangenta_0.91-1.ubuntu24.04_amd64.deb
tangenta_0.91-1.ubuntu26.04_amd64.deb
```

**03** / macOS

## Installieren auf dem Mac

1. **TANGENTA-0.91-macos.dmg** herunterladen und öffnen.
2. TANGENTA in den Ordner **Programme** ziehen.
3. Beim ersten Start meldet macOS, dass sich das Programm nicht öffnen lässt. Dann **Systemeinstellungen › Datenschutz & Sicherheit** öffnen und bei TANGENTA **Dennoch öffnen** wählen.

Die Mac-Fassung ist noch nicht von Apple beglaubigt und weniger erprobt als die Fassungen für Windows und Linux; Rückmeldungen sind willkommen. Freigeben geht auch im Terminal: `xattr -dr com.apple.quarantine /Applications/TANGENTA.app`

**04** / Systemanforderungen

## Was Ihr Rechner braucht.

|  | Mindestens | Empfohlen |
|---|---|---|
| System | Windows 10 (64 Bit), Debian 12, Ubuntu 24.04, macOS 13 | Windows 11, Debian 13, Ubuntu 26.04, macOS 15 |
| Prozessor | x86-64 mit 2 Kernen oder Apple-Chip | 4 Kerne oder mehr |
| Arbeitsspeicher | 4 GB | 8 GB |
| Speicher | 300 MB | 1 GB für Bilder und Zwischenstände |
| Bildschirm | 1366 × 768 | Full HD oder mehr |

**05** / Für die IT

## Still installieren.

Für GPO, Intune, SCCM oder eigene Skripte. Beide Pakete installieren nach `C:\Program Files\TANGENTA`; eine neuere Version ersetzt die ältere.

| Setup still | `TANGENTA-0.91-x64-setup.exe /quiet` |
|---|---|
| Setup mit Fortschritt | `TANGENTA-0.91-x64-setup.exe /passive` |
| Zielordner, Desktop-Verknüpfung | `TANGENTA-0.91-x64-setup.exe /quiet InstallFolder="D:\Programme\TANGENTA" DesktopShortcut=1` |
| MSI still | `msiexec /i TANGENTA-0.91-x64.msi /qn` |
| MSI mit Protokoll | `msiexec /i TANGENTA-0.91-x64.msi /qn /l*v C:\Temp\tangenta.log` |
| Entfernen | `msiexec /x TANGENTA-0.91-x64.msi /qn` |

**06** / Neu in 0.91

## Jetzt auch für den Mac.

- macOS-Fassung für Macs mit Apple-Chip und mit Intel-Prozessor
- Überblendung entlang eines Pfads, mit Verläufen und umgekehrter Reihenfolge
- Bitmap-Effekte direkt auf Objekten, Rahmen-Effekt, Seite aufrollen und Farbkorrekturen
- Fotomosaik, Punktraster, Blockschatten, Pinsel und Sprühdose
- Mehrere Seiten: Seiten-Andockfenster und Mehrseitenansicht
- QR-Codes, eigene Tastenkürzel und viele Verbesserungen aus den Tutorial-Videos

**07** / Quellcode

## Lieber selbst bauen?

Die Bauanleitung führt Schritt für Schritt vom leeren Rechner zum eigenen Build, unter Windows und Linux.

```
git clone https://github.com/roli8200/tangenta.git
cd tangenta
make test && make run
```

[Zur Bauanleitung →](https://github.com/roli8200/tangenta/blob/main/BUILD.md)

## Zeichnen Sie los.

Kostenlos, für Windows, macOS und Linux, mit KI.

[Herunterladen](https://tangenta.ch/download.html) [Auf GitHub ansehen](https://github.com/roli8200/tangenta)
