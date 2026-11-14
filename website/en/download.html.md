# Download – TANGENTA

> Download TANGENTA: setup and MSI for Windows 10/11, .deb for Debian 12/13 and Ubuntu 24.04/26.04. Free of charge, with silent installation.

URL: https://tangenta.ch/en/download.html

Download · version 0.91

# Download  TANGENTA.

Free of charge for Windows, macOS and Linux. All packages are on GitHub with the current release.

### Windows 10 / 11

Setup with a wizard, or an MSI for network deployment. Both install silently.

[Download setup](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x64-setup.exe)

[MSI for deployment →](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x64.msi)

```
TANGENTA-0.91-x64-setup.exe /quiet
msiexec /i TANGENTA-0.91-x64.msi /qn
```

### macOS 13 or newer

One program for Macs with Apple silicon and with Intel processors, as a disk image.

[Download disk image](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-macos.dmg)

```
xattr -dr com.apple.quarantine /Applications/TANGENTA.app
```

### Debian and Ubuntu

Packages for Debian 12, Debian 13, Ubuntu 24.04 and 26.04, with their own Qt 6.10.

[Download .deb (Debian 13)](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian13_amd64.deb)

Packages per system:

- [Debian 12](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian12_amd64.deb)
- [Debian 13](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian13_amd64.deb)
- [Ubuntu 24.04](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.ubuntu24.04_amd64.deb)
- [Ubuntu 26.04](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.ubuntu26.04_amd64.deb)

```
sudo apt install ./tangenta_0.91-1.debian13_amd64.deb
```

### Other Linux systems

Flatpak for Fedora, openSUSE, Arch, Mint and every system with Flatpak. The runtime comes from Flathub.

[Download Flatpak](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x86_64.flatpak)

```
flatpak install --user TANGENTA-0.91-x86_64.flatpak
flatpak run ch.tangenta.Tangenta
```

### GitHub

[All files and checksums →](https://github.com/roli8200/tangenta/releases/tag/v0.91)

[SHA256SUMS-0.91.txt](https://github.com/roli8200/tangenta/releases/download/v0.91/SHA256SUMS-0.91.txt)

```
sha256sum --ignore-missing -c SHA256SUMS-0.91.txt
```

**01** / Windows

## Installing on Windows

1. Download **TANGENTA-0.91-x64-setup.exe** and run it.
2. Accept the licence and choose **Install**. The setup needs administrator rights and installs for all users.
3. Open TANGENTA from the Start menu. From then on, `.vek` files open with a double-click.

If Windows shows “Windows protected your PC”, click **More info** and then **Run anyway**. The packages are not yet digitally signed.

**02** / Linux

## Installing on Linux

1. Download the package for your system:
2. Install it in a terminal: `sudo apt install ./tangenta_0.91-1.debian12_amd64.deb` (adjust the file name).
3. Open TANGENTA from the application menu or type `tangenta`. Remove it with `sudo apt remove tangenta`.

```
tangenta_0.91-1.debian12_amd64.deb
tangenta_0.91-1.debian13_amd64.deb
tangenta_0.91-1.ubuntu24.04_amd64.deb
tangenta_0.91-1.ubuntu26.04_amd64.deb
```

**03** / macOS

## Installing on a Mac

1. Download **TANGENTA-0.91-macos.dmg** and open it.
2. Drag TANGENTA into the **Applications** folder.
3. On first start macOS reports that the program cannot be opened. Open **System Settings › Privacy & Security** and choose **Open Anyway** next to TANGENTA.

The Mac version is not yet notarized by Apple and has had less testing than the Windows and Linux versions; feedback is welcome. You can also allow it in a terminal: `xattr -dr com.apple.quarantine /Applications/TANGENTA.app`

**04** / System requirements

## What your computer needs.

|  | Minimum | Recommended |
|---|---|---|
| System | Windows 10 (64-bit), Debian 12, Ubuntu 24.04, macOS 13 | Windows 11, Debian 13, Ubuntu 26.04, macOS 15 |
| Processor | x86-64 with 2 cores or Apple silicon | 4 cores or more |
| Memory | 4 GB | 8 GB |
| Disk space | 300 MB | 1 GB for images and autosaves |
| Display | 1366 × 768 | Full HD or higher |

**05** / For IT

## Silent installation.

For GPO, Intune, SCCM or your own scripts. Both packages install to `C:\Program Files\TANGENTA`; a newer version replaces an older one.

| Setup, silent | `TANGENTA-0.91-x64-setup.exe /quiet` |
|---|---|
| Setup with progress | `TANGENTA-0.91-x64-setup.exe /passive` |
| Folder and desktop shortcut | `TANGENTA-0.91-x64-setup.exe /quiet InstallFolder="D:\Programs\TANGENTA" DesktopShortcut=1` |
| MSI, silent | `msiexec /i TANGENTA-0.91-x64.msi /qn` |
| MSI with log | `msiexec /i TANGENTA-0.91-x64.msi /qn /l*v C:\Temp\tangenta.log` |
| Uninstall | `msiexec /x TANGENTA-0.91-x64.msi /qn` |

**06** / New in 0.91

## Now on the Mac too.

- macOS version for Macs with Apple silicon and with Intel processors
- Blend along a path, with gradients and reversed order
- Bitmap effects directly on objects, frame effect, page curl and colour corrections
- Photo mosaic, point raster, block shadow, brush and sprayer
- Multiple pages: Pages docker and multi-page view
- QR codes, your own keyboard shortcuts and many improvements from the tutorial videos

**07** / Source code

## Prefer to build it yourself?

The build guide takes you step by step from an empty computer to your own build, on Windows and Linux. It is written in German.

```
git clone https://github.com/roli8200/tangenta.git
cd tangenta
make test && make run
```

[Go to the build guide →](https://github.com/roli8200/tangenta/blob/main/BUILD.md)

## Start drawing.

Free of charge, for Windows, macOS and Linux, with AI.

[Download](https://tangenta.ch/en/download.html) [View on GitHub](https://github.com/roli8200/tangenta)
