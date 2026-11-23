# Téléchargement – TANGENTA

> Télécharger TANGENTA : setup et MSI pour Windows 10/11, image disque pour macOS 13 et plus récent, .deb pour Debian 12/13 et Ubuntu 24.04/26.04, et Flatpak. Gratuit, avec installation silencieuse.

URL: https://tangenta.ch/fr/download.html

Téléchargement · version 0.92

# Télécharger  TANGENTA.

Gratuit pour Windows, macOS et Linux. Tous les paquets se trouvent sur GitHub avec la version actuelle.

### Windows 10 / 11

Setup avec assistant ou MSI pour le déploiement en réseau, tous deux installables en silence.

[Télécharger le setup](https://github.com/roli8200/tangenta/releases/download/v0.92/TANGENTA-0.92-x64-setup.exe)

[MSI pour le déploiement →](https://github.com/roli8200/tangenta/releases/download/v0.92/TANGENTA-0.92-x64.msi)

```
TANGENTA-0.92-x64-setup.exe /quiet
msiexec /i TANGENTA-0.92-x64.msi /qn
```

### macOS 13 ou plus récent

Un programme pour les Mac avec puce Apple et avec processeur Intel, en image disque.

[Télécharger l’image disque](https://github.com/roli8200/tangenta/releases/download/v0.92/TANGENTA-0.92-macos.dmg)

```
xattr -dr com.apple.quarantine /Applications/TANGENTA.app
```

### Debian et Ubuntu

Paquets pour Debian 12, Debian 13, Ubuntu 24.04 et 26.04, avec leur propre Qt 6.10.

[Télécharger le .deb (Debian 13)](https://github.com/roli8200/tangenta/releases/download/v0.92/tangenta_0.92-1.debian13_amd64.deb)

Paquets par système :

- [Debian 12](https://github.com/roli8200/tangenta/releases/download/v0.92/tangenta_0.92-1.debian12_amd64.deb)
- [Debian 13](https://github.com/roli8200/tangenta/releases/download/v0.92/tangenta_0.92-1.debian13_amd64.deb)
- [Ubuntu 24.04](https://github.com/roli8200/tangenta/releases/download/v0.92/tangenta_0.92-1.ubuntu24.04_amd64.deb)
- [Ubuntu 26.04](https://github.com/roli8200/tangenta/releases/download/v0.92/tangenta_0.92-1.ubuntu26.04_amd64.deb)

```
sudo apt install ./tangenta_0.92-1.debian13_amd64.deb
```

### Autres systèmes Linux

Flatpak pour Fedora, openSUSE, Arch, Mint et tout système avec Flatpak. Le runtime vient de Flathub.

[Télécharger le Flatpak](https://github.com/roli8200/tangenta/releases/download/v0.92/TANGENTA-0.92-x86_64.flatpak)

```
flatpak install --user TANGENTA-0.92-x86_64.flatpak
flatpak run ch.tangenta.Tangenta
```

### GitHub

[Tous les fichiers et sommes de contrôle →](https://github.com/roli8200/tangenta/releases/tag/v0.92)

[SHA256SUMS-0.92.txt](https://github.com/roli8200/tangenta/releases/download/v0.92/SHA256SUMS-0.92.txt)

```
sha256sum --ignore-missing -c SHA256SUMS-0.92.txt
```

**01** / Windows

## Installer sous Windows

1. Télécharger **TANGENTA-0.92-x64-setup.exe** et le lancer.
2. Accepter la licence et choisir **Install**. Le setup demande les droits d’administrateur et installe pour tous les utilisateurs.
3. Ouvrir TANGENTA depuis le menu Démarrer. Les fichiers `.vek` s’ouvrent ensuite d’un double-clic.

Si Windows affiche « Windows a protégé votre ordinateur », cliquez sur **Informations complémentaires**, puis sur **Exécuter quand même**. Les paquets ne sont pas encore signés numériquement.

**02** / Linux

## Installer sous Linux

1. Télécharger le paquet pour votre système :
2. L’installer dans un terminal : `sudo apt install ./tangenta_0.92-1.debian12_amd64.deb` (adapter le nom du fichier).
3. Ouvrir TANGENTA depuis le menu des applications ou taper `tangenta`. Désinstaller avec `sudo apt remove tangenta`.

```
tangenta_0.92-1.debian12_amd64.deb
tangenta_0.92-1.debian13_amd64.deb
tangenta_0.92-1.ubuntu24.04_amd64.deb
tangenta_0.92-1.ubuntu26.04_amd64.deb
```

**03** / macOS

## Installer sur Mac

1. Télécharger **TANGENTA-0.92-macos.dmg** et l’ouvrir.
2. Glisser TANGENTA dans le dossier **Applications**.
3. Au premier lancement, macOS indique que le programme ne peut pas être ouvert. Ouvrir alors **Réglages Système › Confidentialité et sécurité** et choisir **Ouvrir quand même** pour TANGENTA.

La version Mac n’est pas encore notariée par Apple et a été moins testée que les versions Windows et Linux ; vos retours sont les bienvenus. L’autorisation fonctionne aussi dans un terminal : `xattr -dr com.apple.quarantine /Applications/TANGENTA.app`

**04** / Configuration requise

## Ce dont votre ordinateur a besoin.

|  | Minimum | Recommandé |
|---|---|---|
| Système | Windows 10 (64 bits), Debian 12, Ubuntu 24.04, macOS 13 | Windows 11, Debian 13, Ubuntu 26.04, macOS 15 |
| Processeur | x86-64 avec 2 cœurs ou puce Apple | 4 cœurs ou plus |
| Mémoire | 4 Go | 8 Go |
| Espace disque | 300 Mo | 1 Go pour les images et sauvegardes |
| Écran | 1366 × 768 | Full HD ou plus |

**05** / Pour l’informatique

## Installation silencieuse.

Pour GPO, Intune, SCCM ou vos propres scripts. Les deux paquets s’installent dans `C:\Program Files\TANGENTA` ; une version plus récente remplace l’ancienne.

| Setup silencieux | `TANGENTA-0.92-x64-setup.exe /quiet` |
|---|---|
| Setup avec progression | `TANGENTA-0.92-x64-setup.exe /passive` |
| Dossier et raccourci sur le bureau | `TANGENTA-0.92-x64-setup.exe /quiet InstallFolder="D:\Programmes\TANGENTA" DesktopShortcut=1` |
| MSI silencieux | `msiexec /i TANGENTA-0.92-x64.msi /qn` |
| MSI avec journal | `msiexec /i TANGENTA-0.92-x64.msi /qn /l*v C:\Temp\tangenta.log` |
| Désinstaller | `msiexec /x TANGENTA-0.92-x64.msi /qn` |

**06** / Nouveautés 0.92

## Une IA qui améliore vraiment vos photos.

- Optimiser l’image avec l’IA : un modèle d’image retouche la photo, TANGENTA reporte couleurs et lumière sur l’original en pleine résolution
- Détourage local ou via OpenAI, avec l’assistant de détourage pour les cheveux fins
- Vos propres barres d’outils par glisser-déposer et des icônes sur toutes les commandes de menu
- Grille de lignes de base pour le texte courant
- Export CDR (X6 ou X3) et import CDR amélioré avec transparence, PowerClip et dégradés
- Plusieurs fenêtres de document, presse-papiers avec d’autres programmes, écran de démarrage et menu Aide

**07** / Code source

## Vous préférez compiler vous-même ?

Le guide de compilation vous mène pas à pas d’un ordinateur vide à votre propre build, sous Windows et Linux. Il est rédigé en allemand.

```
git clone https://github.com/roli8200/tangenta.git
cd tangenta
make test && make run
```

[Vers le guide de compilation →](https://github.com/roli8200/tangenta/blob/main/BUILD.md)

## À vos crayons.

Gratuit, pour Windows, macOS et Linux, avec IA.

[Télécharger](https://tangenta.ch/fr/download.html) [Voir sur GitHub](https://github.com/roli8200/tangenta)
