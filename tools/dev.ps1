# Entwicklungsbefehle unter Windows (vom Makefile aufgerufen, geht auch direkt):
#
#   powershell -ExecutionPolicy Bypass -File tools\dev.ps1 <deps|configure|build|test|run|clean> [-BuildType Release|Debug]
#
# Alles ohne Administratorrechte: Qt und MinGW über aqtinstall nach %USERPROFILE%\Qt,
# CMake und Ninja über pip.
param(
    [Parameter(Mandatory = $true)][ValidateSet('deps', 'configure', 'build', 'test', 'run', 'clean')][string]$Action,
    [ValidateSet('Release', 'Debug')][string]$BuildType = 'Release'
)

$ErrorActionPreference = 'Stop'
$qtVersion = '6.10.3'
$qtDir     = Join-Path $env:USERPROFILE "Qt\$qtVersion\mingw_64"
$mingwBin  = Join-Path $env:USERPROFILE 'Qt\Tools\mingw1310_64\bin'
$source    = Split-Path -Parent $PSScriptRoot
$build     = Join-Path $source ("build\" + $BuildType.ToLower())

function Run([string]$what) {
    if ($LASTEXITCODE -ne 0) { throw "$what fehlgeschlagen (Code $LASTEXITCODE)" }
}

switch ($Action) {
    'deps' {
        python -m pip install --user --upgrade aqtinstall cmake ninja; Run 'pip'
        python -m aqt install-qt windows desktop $qtVersion win64_mingw -m qtimageformats -O (Join-Path $env:USERPROFILE 'Qt'); Run 'Qt'
        python -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O (Join-Path $env:USERPROFILE 'Qt'); Run 'MinGW'
        Write-Host "Qt: $qtDir`nMinGW: $mingwBin"
        return
    }
    'clean' {
        if (Test-Path (Join-Path $source 'build')) { Remove-Item -Recurse -Force (Join-Path $source 'build') }
        return
    }
}

if (-not (Test-Path (Join-Path $qtDir 'bin\windeployqt.exe'))) { throw "Qt fehlt unter $qtDir - zuerst: mingw32-make deps" }
$env:PATH = "$mingwBin;$qtDir\bin;$env:PATH"
$ninja = python -c "import ninja,os;print(os.path.join(ninja.BIN_DIR,'ninja.exe'))"

if ($Action -in 'configure', 'build', 'test', 'run' -and -not (Test-Path (Join-Path $build 'build.ninja'))) {
    python -m cmake -S $source -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_BUILD_TYPE=$BuildType" `
        "-DCMAKE_PREFIX_PATH=$qtDir" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++; Run 'CMake'
}
if ($Action -in 'build', 'test', 'run') {
    python -m cmake --build $build; Run 'Build'
}
if ($Action -eq 'test') {
    python -m cmake -E chdir $build ctest --output-on-failure; Run 'Tests'
}
if ($Action -eq 'run') {
    & (Join-Path $build 'tangenta.exe')
}
