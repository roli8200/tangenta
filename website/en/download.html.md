# Download – TANGENTA

> Download TANGENTA: setup and MSI for Windows 10/11, .deb for Debian 12/13 and Ubuntu 24.04/26.04. Free of charge, with silent installation.

URL: https://tangenta.ch/en/download.html

Download · version 0.86

# Download  TANGENTA.

Free of charge for Windows and Linux. All packages are on GitHub with the current release.

### Windows 10 / 11

Setup with a wizard, or an MSI for network deployment. Both install silently.

[Download setup](https://github.com/roli8200/tangenta/releases/download/v0.86/TANGENTA-0.86-x64-setup.exe)

[MSI for deployment →](https://github.com/roli8200/tangenta/releases/download/v0.86/TANGENTA-0.86-x64.msi)

```
TANGENTA-0.86-x64-setup.exe /quiet
msiexec /i TANGENTA-0.86-x64.msi /qn
```

### Debian and Ubuntu

Packages for Debian 12, Debian 13, Ubuntu 24.04 and 26.04, with their own Qt 6.10.

[Download .deb (Debian 13)](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.debian13_amd64.deb)

Packages per system:

- [Debian 12](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.debian12_amd64.deb)
- [Debian 13](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.debian13_amd64.deb)
- [Ubuntu 24.04](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.ubuntu24.04_amd64.deb)
- [Ubuntu 26.04](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.ubuntu26.04_amd64.deb)

```
sudo apt install ./tangenta_0.86-1.debian13_amd64.deb
```

### Other Linux systems

Flatpak for Fedora, openSUSE, Arch, Mint and every system with Flatpak. The runtime comes from Flathub.

[Download Flatpak](https://github.com/roli8200/tangenta/releases/download/v0.86/TANGENTA-0.86-x86_64.flatpak)

```
flatpak install --user TANGENTA-0.86-x86_64.flatpak
flatpak run ch.tangenta.Tangenta
```

### GitHub

[All files and checksums →](https://github.com/roli8200/tangenta/releases/tag/v0.86)

[SHA256SUMS-0.86.txt](https://github.com/roli8200/tangenta/releases/download/v0.86/SHA256SUMS-0.86.txt)

```
sha256sum --ignore-missing -c SHA256SUMS-0.86.txt
```

**01** / Windows

## Installing on Windows

1. Download **TANGENTA-0.86-x64-setup.exe** and run it.
2. Accept the licence and choose **Install**. The setup needs administrator rights and installs for all users.
3. Open TANGENTA from the Start menu. From then on, `.vek` files open with a double-click.

If Windows shows “Windows protected your PC”, click **More info** and then **Run anyway**. The packages are not yet digitally signed.

**02** / Linux

## Installing on Linux

1. Download the package for your system:
2. Install it in a terminal: `sudo apt install ./tangenta_0.86-1.debian12_amd64.deb` (adjust the file name).
3. Open TANGENTA from the application menu or type `tangenta`. Remove it with `sudo apt remove tangenta`.

```
tangenta_0.86-1.debian12_amd64.deb
tangenta_0.86-1.debian13_amd64.deb
tangenta_0.86-1.ubuntu24.04_amd64.deb
tangenta_0.86-1.ubuntu26.04_amd64.deb
```

**03** / System requirements

## What your computer needs.

|  | Minimum | Recommended |
|---|---|---|
| System | Windows 10 (64-bit), Debian 12, Ubuntu 24.04 | Windows 11, Debian 13, Ubuntu 26.04 |
| Processor | x86-64 with 2 cores | 4 cores or more |
| Memory | 4 GB | 8 GB |
| Disk space | 300 MB | 1 GB for images and autosaves |
| Display | 1366 × 768 | Full HD or higher |

**04** / For IT

## Silent installation.

For GPO, Intune, SCCM or your own scripts. Both packages install to `C:\Program Files\TANGENTA`; a newer version replaces an older one.

| Setup, silent | `TANGENTA-0.86-x64-setup.exe /quiet` |
|---|---|
| Setup with progress | `TANGENTA-0.86-x64-setup.exe /passive` |
| Folder and desktop shortcut | `TANGENTA-0.86-x64-setup.exe /quiet InstallFolder="D:\Programs\TANGENTA" DesktopShortcut=1` |
| MSI, silent | `msiexec /i TANGENTA-0.86-x64.msi /qn` |
| MSI with log | `msiexec /i TANGENTA-0.86-x64.msi /qn /l*v C:\Temp\tangenta.log` |
| Uninstall | `msiexec /x TANGENTA-0.86-x64.msi /qn` |

**05** / New in 0.86

## Opens CDR files.

- Open CDR and CMX files directly, from version 5 to X6 and newer
- Text from CDR keeps rotation, condensing, bold and italic; masked images stay cut out
- File/Open also reads PDF, AI, SVG (including Inkscape), EPS and DXF
- SuperTrace keeps small lettering much better
- Large files with photos load several times faster

**06** / Source code

## Prefer to build it yourself?

The build guide takes you step by step from an empty computer to your own build, on Windows and Linux. It is written in German.

```
git clone https://github.com/roli8200/tangenta.git
cd tangenta
make test && make run
```

[Go to the build guide →](https://github.com/roli8200/tangenta/blob/main/BUILD.md)

## Start drawing.

Free of charge, for Windows and Linux, with AI.

[Download](https://tangenta.ch/en/download.html) [View on GitHub](https://github.com/roli8200/tangenta)
