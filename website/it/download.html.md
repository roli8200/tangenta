# Download – TANGENTA

> Scaricare TANGENTA: setup e MSI per Windows 10/11, .deb per Debian 12/13 e Ubuntu 24.04/26.04. Gratuito, con installazione silenziosa.

URL: https://tangenta.ch/it/download.html

Download · versione 0.91

# Scaricare  TANGENTA.

Gratuito per Windows, macOS e Linux. Tutti i pacchetti si trovano su GitHub con la versione attuale.

### Windows 10 / 11

Setup con procedura guidata o MSI per la distribuzione in rete, entrambi installabili in modo silenzioso.

[Scarica il setup](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x64-setup.exe)

[MSI per la distribuzione →](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x64.msi)

```
TANGENTA-0.91-x64-setup.exe /quiet
msiexec /i TANGENTA-0.91-x64.msi /qn
```

### macOS 13 o successivo

Un programma per Mac con chip Apple e con processore Intel, come immagine disco.

[Scarica l’immagine disco](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-macos.dmg)

```
xattr -dr com.apple.quarantine /Applications/TANGENTA.app
```

### Debian e Ubuntu

Pacchetti per Debian 12, Debian 13, Ubuntu 24.04 e 26.04, con il proprio Qt 6.10.

[Scarica il .deb (Debian 13)](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian13_amd64.deb)

Pacchetti per sistema:

- [Debian 12](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian12_amd64.deb)
- [Debian 13](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.debian13_amd64.deb)
- [Ubuntu 24.04](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.ubuntu24.04_amd64.deb)
- [Ubuntu 26.04](https://github.com/roli8200/tangenta/releases/download/v0.91/tangenta_0.91-1.ubuntu26.04_amd64.deb)

```
sudo apt install ./tangenta_0.91-1.debian13_amd64.deb
```

### Altri sistemi Linux

Flatpak per Fedora, openSUSE, Arch, Mint e ogni sistema con Flatpak. Il runtime arriva da Flathub.

[Scarica il Flatpak](https://github.com/roli8200/tangenta/releases/download/v0.91/TANGENTA-0.91-x86_64.flatpak)

```
flatpak install --user TANGENTA-0.91-x86_64.flatpak
flatpak run ch.tangenta.Tangenta
```

### GitHub

[Tutti i file e i checksum →](https://github.com/roli8200/tangenta/releases/tag/v0.91)

[SHA256SUMS-0.91.txt](https://github.com/roli8200/tangenta/releases/download/v0.91/SHA256SUMS-0.91.txt)

```
sha256sum --ignore-missing -c SHA256SUMS-0.91.txt
```

**01** / Windows

## Installare su Windows

1. Scaricare **TANGENTA-0.91-x64-setup.exe** e avviarlo.
2. Accettare la licenza e scegliere **Install**. Il setup richiede i diritti di amministratore e installa per tutti gli utenti.
3. Aprire TANGENTA dal menu Start. I file `.vek` si aprono poi con un doppio clic.

Se Windows mostra «PC protetto da Windows», faccia clic su **Ulteriori informazioni** e poi su **Esegui comunque**. I pacchetti non sono ancora firmati digitalmente.

**02** / Linux

## Installare su Linux

1. Scaricare il pacchetto per il proprio sistema:
2. Installarlo in un terminale: `sudo apt install ./tangenta_0.91-1.debian12_amd64.deb` (adattare il nome del file).
3. Aprire TANGENTA dal menu delle applicazioni o digitare `tangenta`. Rimuoverlo con `sudo apt remove tangenta`.

```
tangenta_0.91-1.debian12_amd64.deb
tangenta_0.91-1.debian13_amd64.deb
tangenta_0.91-1.ubuntu24.04_amd64.deb
tangenta_0.91-1.ubuntu26.04_amd64.deb
```

**03** / macOS

## Installare su Mac

1. Scaricare **TANGENTA-0.91-macos.dmg** e aprirlo.
2. Trascinare TANGENTA nella cartella **Applicazioni**.
3. Al primo avvio macOS segnala che il programma non può essere aperto. Aprire allora **Impostazioni di Sistema › Privacy e sicurezza** e scegliere **Apri comunque** accanto a TANGENTA.

La versione per Mac non è ancora autenticata da Apple ed è stata provata meno delle versioni per Windows e Linux; i riscontri sono benvenuti. L’autorizzazione funziona anche in un terminale: `xattr -dr com.apple.quarantine /Applications/TANGENTA.app`

**04** / Requisiti di sistema

## Cosa serve al suo computer.

|  | Minimo | Consigliato |
|---|---|---|
| Sistema | Windows 10 (64 bit), Debian 12, Ubuntu 24.04, macOS 13 | Windows 11, Debian 13, Ubuntu 26.04, macOS 15 |
| Processore | x86-64 con 2 core o chip Apple | 4 core o più |
| Memoria | 4 GB | 8 GB |
| Spazio su disco | 300 MB | 1 GB per immagini e salvataggi |
| Schermo | 1366 × 768 | Full HD o superiore |

**05** / Per l’informatica

## Installazione silenziosa.

Per GPO, Intune, SCCM o script propri. Entrambi i pacchetti si installano in `C:\Program Files\TANGENTA`; una versione più recente sostituisce quella precedente.

| Setup silenzioso | `TANGENTA-0.91-x64-setup.exe /quiet` |
|---|---|
| Setup con avanzamento | `TANGENTA-0.91-x64-setup.exe /passive` |
| Cartella e collegamento sul desktop | `TANGENTA-0.91-x64-setup.exe /quiet InstallFolder="D:\Programmi\TANGENTA" DesktopShortcut=1` |
| MSI silenzioso | `msiexec /i TANGENTA-0.91-x64.msi /qn` |
| MSI con registro | `msiexec /i TANGENTA-0.91-x64.msi /qn /l*v C:\Temp\tangenta.log` |
| Disinstallare | `msiexec /x TANGENTA-0.91-x64.msi /qn` |

**06** / Novità 0.91

## Ora anche su Mac.

- Versione macOS per Mac con chip Apple e con processore Intel
- Fusione lungo un tracciato, con sfumature e ordine invertito
- Effetti bitmap direttamente sugli oggetti, effetto cornice, pagina arrotolata e correzioni del colore
- Mosaico di foto, retino a punti, ombra a blocco, pennello e spruzzatore
- Più pagine: finestra Pagine e vista a più pagine
- Codici QR, scorciatoie da tastiera proprie e molti miglioramenti nati dai video tutorial

**07** / Codice sorgente

## Preferisce compilare da sé?

La guida alla compilazione porta passo dopo passo da un computer vuoto alla propria build, su Windows e Linux. È scritta in tedesco.

```
git clone https://github.com/roli8200/tangenta.git
cd tangenta
make test && make run
```

[Alla guida alla compilazione →](https://github.com/roli8200/tangenta/blob/main/BUILD.md)

## Inizi a disegnare.

Gratuito, per Windows, macOS e Linux, con IA.

[Scarica](https://tangenta.ch/it/download.html) [Vedere su GitHub](https://github.com/roli8200/tangenta)
