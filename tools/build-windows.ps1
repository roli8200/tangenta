# Baut TANGENTA als Release unter Windows (Qt/MinGW aus aqtinstall) und legt eine
# startfähige Fassung samt Qt-DLLs nach %USERPROFILE%\Tangenta.
#
#   powershell -ExecutionPolicy Bypass -File tools\build-windows.ps1

param([string]$Target = (Join-Path $env:USERPROFILE 'Tangenta'), [switch]$SkipTests)

$ErrorActionPreference = 'Stop'

$qtVersion = '6.10.3'
$qtRoot    = Join-Path $env:USERPROFILE 'Qt'
$qtDir     = Join-Path $qtRoot "$qtVersion\mingw_64"
$mingwBin  = Join-Path $qtRoot 'Tools\mingw1310_64\bin'
$source    = Split-Path -Parent $PSScriptRoot
$build     = Join-Path $env:USERPROFILE 'build\tangenta-release'
$target    = $Target

$env:PATH = "$mingwBin;$qtDir\bin;$env:PATH"
$ninja = python -c "import ninja,os;print(os.path.join(ninja.BIN_DIR,'ninja.exe'))"

python -m cmake -S $source -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" `
    -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$qtDir" `
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
if ($LASTEXITCODE -ne 0) { throw 'CMake-Konfiguration fehlgeschlagen' }
python -m cmake --build $build
if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen' }
if (-not $SkipTests) {
    python -m cmake -E chdir $build ctest --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests fehlgeschlagen' }
}

New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item (Join-Path $build 'tangenta.exe') $target -Force
# Warnungen auf stderr (z.B. fehlende dxcompiler.dll) sind kein Fehler; es zählt der Rückgabewert
$ErrorActionPreference = 'Continue'
& (Join-Path $qtDir 'bin\windeployqt.exe') --release --no-system-d3d-compiler `
    --no-opengl-sw (Join-Path $target 'tangenta.exe') 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { throw 'windeployqt fehlgeschlagen' }
# MinGW-Laufzeit, falls windeployqt sie nicht mitgenommen hat
# libgomp: OpenMP für das Entwickeln von Kamera-Rohdaten (LibRaw)
foreach ($dll in 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll', 'libgomp-1.dll') {
    Copy-Item (Join-Path $mingwBin $dll) $target -Force
}
# Wörterbücher der Rechtschreibprüfung
$dictTarget = Join-Path $target 'dictionaries'
New-Item -ItemType Directory -Force $dictTarget | Out-Null
Copy-Item (Join-Path $source 'resources\dictionaries\*') $dictTarget -Force
# Ghostscript (EPS/PS-Import) als Bibliothek im Programmordner, läuft im TANGENTA-Prozess.
# Quelle: offizielles Windows-Paket, einmalig nach %USERPROFILE%\ghostscript entpackt (7-Zip).
$gsVersion = '10.08.0'
$gsTag     = 'gs10080'
$gsDir     = Join-Path $env:USERPROFILE "ghostscript\$gsVersion"
if (-not (Test-Path (Join-Path $gsDir 'bin\gsdll64.dll'))) {
    $setup = Join-Path $env:USERPROFILE "ghostscript\dl\${gsTag}w64.exe"
    New-Item -ItemType Directory -Force (Split-Path $setup) | Out-Null
    if (-not (Test-Path $setup)) {
        Invoke-WebRequest "https://github.com/ArtifexSoftware/ghostpdl-downloads/releases/download/$gsTag/${gsTag}w64.exe" -OutFile $setup
    }
    & 'C:\Program Files\7-Zip\7z.exe' x -y "-o$gsDir" $setup 'bin\gsdll64.dll' 'doc\COPYING' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Ghostscript entpacken fehlgeschlagen' }
}
Copy-Item (Join-Path $gsDir 'bin\gsdll64.dll') $target -Force
$licTarget = Join-Path $target 'licenses'
New-Item -ItemType Directory -Force $licTarget | Out-Null
Copy-Item (Join-Path $gsDir 'doc\COPYING') (Join-Path $licTarget 'Ghostscript-AGPL-3.0.txt') -Force
# gsdll64.dll ist mit MSVC gebaut: dessen Laufzeit app-lokal beilegen (von Microsoft erlaubt)
foreach ($dll in 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') {
    Copy-Item (Join-Path $env:WINDIR "System32\$dll") $target -Force
}
Write-Host "Fertig: $target\tangenta.exe"
