# Téléchargement – TANGENTA

> Télécharger TANGENTA : setup et MSI pour Windows 10/11, .deb pour Debian 12/13 et Ubuntu 24.04/26.04. Gratuit, avec installation silencieuse.

URL: https://tangenta.ch/fr/download.html

Téléchargement · version 0.86

# Télécharger  TANGENTA.

Gratuit pour Windows et Linux. Tous les paquets se trouvent sur GitHub avec la version actuelle.

### Windows 10 / 11

Setup avec assistant ou MSI pour le déploiement en réseau, tous deux installables en silence.

[Télécharger le setup](https://github.com/roli8200/tangenta/releases/download/v0.86/TANGENTA-0.86-x64-setup.exe)

[MSI pour le déploiement →](https://github.com/roli8200/tangenta/releases/download/v0.86/TANGENTA-0.86-x64.msi)

```
TANGENTA-0.86-x64-setup.exe /quiet
msiexec /i TANGENTA-0.86-x64.msi /qn
```

### Debian et Ubuntu

Paquets pour Debian 12, Debian 13, Ubuntu 24.04 et 26.04, avec leur propre Qt 6.10.

[Télécharger le .deb (Debian 13)](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.debian13_amd64.deb)

Paquets par système :

- [Debian 12](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.debian12_amd64.deb)
- [Debian 13](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.debian13_amd64.deb)
- [Ubuntu 24.04](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.ubuntu24.04_amd64.deb)
- [Ubuntu 26.04](https://github.com/roli8200/tangenta/releases/download/v0.86/tangenta_0.86-1.ubuntu26.04_amd64.deb)

```
sudo apt install ./tangenta_0.86-1.debian13_amd64.deb
```

### Autres systèmes Linux

Flatpak pour Fedora, openSUSE, Arch, Mint et tout système avec Flatpak. Le runtime vient de Flathub.

[Télécharger le Flatpak](https://github.com/roli8200/tangenta/releases/download/v0.86/TANGENTA-0.86-x86_64.flatpak)

```
flatpak install --user TANGENTA-0.86-x86_64.flatpak
flatpak run ch.tangenta.Tangenta
```

### GitHub

[Tous les fichiers et sommes de contrôle →](https://github.com/roli8200/tangenta/releases/tag/v0.86)

[SHA256SUMS-0.86.txt](https://github.com/roli8200/tangenta/releases/download/v0.86/SHA256SUMS-0.86.txt)

```
sha256sum --ignore-missing -c SHA256SUMS-0.86.txt
```

**01** / Windows

## Installer sous Windows

1. Télécharger **TANGENTA-0.86-x64-setup.exe** et le lancer.
2. Accepter la licence et choisir **Install**. Le setup demande les droits d’administrateur et installe pour tous les utilisateurs.
3. Ouvrir TANGENTA depuis le menu Démarrer. Les fichiers `.vek` s’ouvrent ensuite d’un double-clic.

Si Windows affiche « Windows a protégé votre ordinateur », cliquez sur **Informations complémentaires**, puis sur **Exécuter quand même**. Les paquets ne sont pas encore signés numériquement.

**02** / Linux

## Installer sous Linux

1. Télécharger le paquet pour votre système :
2. L’installer dans un terminal : `sudo apt install ./tangenta_0.86-1.debian12_amd64.deb` (adapter le nom du fichier).
3. Ouvrir TANGENTA depuis le menu des applications ou taper `tangenta`. Désinstaller avec `sudo apt remove tangenta`.

```
tangenta_0.86-1.debian12_amd64.deb
tangenta_0.86-1.debian13_amd64.deb
tangenta_0.86-1.ubuntu24.04_amd64.deb
tangenta_0.86-1.ubuntu26.04_amd64.deb
```

**03** / Configuration requise

## Ce dont votre ordinateur a besoin.

|  | Minimum | Recommandé |
|---|---|---|
| Système | Windows 10 (64 bits), Debian 12, Ubuntu 24.04 | Windows 11, Debian 13, Ubuntu 26.04 |
| Processeur | x86-64 avec 2 cœurs | 4 cœurs ou plus |
| Mémoire | 4 Go | 8 Go |
| Espace disque | 300 Mo | 1 Go pour les images et sauvegardes |
| Écran | 1366 × 768 | Full HD ou plus |

**04** / Pour l’informatique

## Installation silencieuse.

Pour GPO, Intune, SCCM ou vos propres scripts. Les deux paquets s’installent dans `C:\Program Files\TANGENTA` ; une version plus récente remplace l’ancienne.

| Setup silencieux | `TANGENTA-0.86-x64-setup.exe /quiet` |
|---|---|
| Setup avec progression | `TANGENTA-0.86-x64-setup.exe /passive` |
| Dossier et raccourci sur le bureau | `TANGENTA-0.86-x64-setup.exe /quiet InstallFolder="D:\Programmes\TANGENTA" DesktopShortcut=1` |
| MSI silencieux | `msiexec /i TANGENTA-0.86-x64.msi /qn` |
| MSI avec journal | `msiexec /i TANGENTA-0.86-x64.msi /qn /l*v C:\Temp\tangenta.log` |
| Désinstaller | `msiexec /x TANGENTA-0.86-x64.msi /qn` |

**05** / Nouveautés 0.86

## Ouvre les fichiers CDR.

- Ouverture directe des fichiers CDR et CMX, de la version 5 à X6 et au-delà
- Texte CDR avec rotation, compression, gras et italique ; images détourées conservées
- Fichier/Ouvrir lit aussi PDF, AI, SVG (y compris Inkscape), EPS et DXF
- SuperTrace conserve bien mieux les petits textes
- Les gros fichiers avec photos se chargent plusieurs fois plus vite

**06** / Code source

## Vous préférez compiler vous-même ?

Le guide de compilation vous mène pas à pas d’un ordinateur vide à votre propre build, sous Windows et Linux. Il est rédigé en allemand.

```
git clone https://github.com/roli8200/tangenta.git
cd tangenta
make test && make run
```

[Vers le guide de compilation →](https://github.com/roli8200/tangenta/blob/main/BUILD.md)

## À vos crayons.

Gratuit, pour Windows et Linux, avec IA.

[Télécharger](https://tangenta.ch/fr/download.html) [Voir sur GitHub](https://github.com/roli8200/tangenta)
