# Download – TANGENTA

> Scaricare TANGENTA: setup e MSI per Windows 10/11, .deb per Debian 12/13 e Ubuntu 24.04/26.04. Gratuito, con installazione silenziosa.

URL: https://tangenta.ch/it/download.html

Download · versione 0.86

# Scaricare  TANGENTA.

Gratuito per Windows e Linux. Tutti i pacchetti si trovano su GitHub con la versione attuale.

### Windows 10 / 11

Setup con procedura guidata o MSI per la distribuzione in rete, entrambi installabili in modo silenzioso.

[Scarica il setup](https://github.com/roli8200/tangenta/releases/latest)

```
TANGENTA-0.86-x64-setup.exe /quiet
msiexec /i TANGENTA-0.86-x64.msi /qn
```

### Debian e Ubuntu

Pacchetti per Debian 12, Debian 13, Ubuntu 24.04 e 26.04, con il proprio Qt 6.10.

[Scarica il .deb](https://github.com/roli8200/tangenta/releases/latest)

```
sudo apt install ./tangenta_0.86-1~debian12_amd64.deb
```

**01** / Windows

## Installare su Windows

1. Scaricare **TANGENTA-0.86-x64-setup.exe** e avviarlo.
2. Accettare la licenza e scegliere **Install**. Il setup richiede i diritti di amministratore e installa per tutti gli utenti.
3. Aprire TANGENTA dal menu Start. I file `.vek` si aprono poi con un doppio clic.

Se Windows mostra «PC protetto da Windows», faccia clic su **Ulteriori informazioni** e poi su **Esegui comunque**. I pacchetti non sono ancora firmati digitalmente.

**02** / Linux

## Installare su Linux

1. Scaricare il pacchetto per il proprio sistema:
2. Installarlo in un terminale: `sudo apt install ./tangenta_0.86-1~debian12_amd64.deb` (adattare il nome del file).
3. Aprire TANGENTA dal menu delle applicazioni o digitare `tangenta`. Rimuoverlo con `sudo apt remove tangenta`.

```
tangenta_0.86-1~debian12_amd64.deb
tangenta_0.86-1~debian13_amd64.deb
tangenta_0.86-1~ubuntu24.04_amd64.deb
tangenta_0.86-1~ubuntu26.04_amd64.deb
```

**03** / Requisiti di sistema

## Cosa serve al suo computer.

|  | Minimo | Consigliato |
|---|---|---|
| Sistema | Windows 10 (64 bit), Debian 12, Ubuntu 24.04 | Windows 11, Debian 13, Ubuntu 26.04 |
| Processore | x86-64 con 2 core | 4 core o più |
| Memoria | 4 GB | 8 GB |
| Spazio su disco | 300 MB | 1 GB per immagini e salvataggi |
| Schermo | 1366 × 768 | Full HD o superiore |

**04** / Per l’informatica

## Installazione silenziosa.

Per GPO, Intune, SCCM o script propri. Entrambi i pacchetti si installano in `C:\Program Files\TANGENTA`; una versione più recente sostituisce quella precedente.

| Setup silenzioso | `TANGENTA-0.86-x64-setup.exe /quiet` |
|---|---|
| Setup con avanzamento | `TANGENTA-0.86-x64-setup.exe /passive` |
| Cartella e collegamento sul desktop | `TANGENTA-0.86-x64-setup.exe /quiet InstallFolder="D:\Programmi\TANGENTA" DesktopShortcut=1` |
| MSI silenzioso | `msiexec /i TANGENTA-0.86-x64.msi /qn` |
| MSI con registro | `msiexec /i TANGENTA-0.86-x64.msi /qn /l*v C:\Temp\tangenta.log` |
| Disinstallare | `msiexec /x TANGENTA-0.86-x64.msi /qn` |

**05** / Novità 0.86

## Apre i file CDR.

- Apertura diretta di file CDR e CMX, dalla versione 5 a X6 e successive
- Testo CDR con rotazione, compressione, grassetto e corsivo; immagini scontornate
- File/Apri legge anche PDF, AI, SVG (anche da Inkscape), EPS e DXF
- SuperTrace conserva molto meglio le scritte piccole
- I file grandi con foto si caricano molto più velocemente

**06** / Codice sorgente

## Preferisce compilare da sé?

La guida alla compilazione porta passo dopo passo da un computer vuoto alla propria build, su Windows e Linux. È scritta in tedesco.

```
git clone https://github.com/roli8200/tangenta.git
cd tangenta
make test && make run
```

[Alla guida alla compilazione →](https://github.com/roli8200/tangenta/blob/main/BUILD.md)

## Inizi a disegnare.

Gratuito, per Windows e Linux, con IA.

[Scarica](https://tangenta.ch/it/download.html) [Vedere su GitHub](https://github.com/roli8200/tangenta)
