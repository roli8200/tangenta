# Baut die Windows-Pakete von TANGENTA:
#   build\packages\TANGENTA-<version>-x64.msi        (msiexec /i ... /qn INSTALLFOLDER=...)
#   build\packages\TANGENTA-<version>-x64-setup.exe  (Setup mit /quiet, /passive, InstallFolder=...)
#
#   powershell -ExecutionPolicy Bypass -File tools\package-windows.ps1 [-SkipBuild] [-SkipTests]
#
# Voraussetzungen (ohne Administratorrechte, siehe BUILD.md):
#   dotnet tool install --global wix --version 4.0.6
#   wix extension add -g WixToolset.UI.wixext/4.0.6
#   wix extension add -g WixToolset.Bal.wixext/4.0.6
param([switch]$SkipBuild, [switch]$SkipTests)

$ErrorActionPreference = 'Stop'
$source   = Split-Path -Parent $PSScriptRoot
$out      = Join-Path $source 'build\packages'
$dist     = Join-Path $source 'build\dist\TANGENTA'
$work     = Join-Path $source 'build\wix'
$wix      = Join-Path $env:USERPROFILE '.dotnet\tools\wix.exe'
if (-not (Test-Path $wix)) { $wix = 'wix' }

# Version aus CMakeLists.txt (project(Tangenta VERSION x.y[.z] ...))
$cmake = Get-Content (Join-Path $source 'CMakeLists.txt') -Raw -Encoding UTF8
if ($cmake -notmatch 'project\(Tangenta VERSION ([0-9]+\.[0-9]+(?:\.[0-9]+)?)') { throw 'Version in CMakeLists.txt nicht gefunden' }
$version = $Matches[1]

# Feste Kennungen: nie ändern, sonst erkennt Windows Updates nicht als solche
$upgradeMsi    = '6F1A0C0E-6B9C-4E1D-9C59-3A7E5B2D8A41'
$upgradeBundle = 'B4D7E2A9-1F53-4C8E-A0B6-5E9D3C71F2A8'
$manufacturer  = 'Intersoft Networks'
$homepage      = 'https://tangenta.ch'

if (-not $SkipBuild) {
    if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
    $buildArgs = @('-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'build-windows.ps1'), '-Target', $dist)
    if ($SkipTests) { $buildArgs += '-SkipTests' }
    & powershell @buildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen' }
}
if (-not (Test-Path (Join-Path $dist 'tangenta.exe'))) { throw "Keine Programmfassung in $dist" }
Copy-Item (Join-Path $source 'LICENSE') (Join-Path $dist 'LICENSE.txt') -Force

New-Item -ItemType Directory -Force $out, $work | Out-Null

# Lizenz als RTF für den MSI-Dialog
$license = Get-Content (Join-Path $source 'LICENSE') -Raw -Encoding UTF8
$rtfBody = ($license -replace '\\', '\\' -replace '\{', '\{' -replace '\}', '\}') -replace "`r?`n", "\par`r`n"
Set-Content -Path (Join-Path $work 'license.rtf') -Encoding ASCII -Value ("{\rtf1\ansi\deff0{\fonttbl{\f0 Consolas;}}\fs16`r`n" + $rtfBody + '}')

# Verzeichnisbaum und Komponenten aus dem Programmordner erzeugen (WiX 4 kennt kein <Files>)
function Id([string]$prefix, [string]$path) {
    $sha = [System.Security.Cryptography.SHA1]::Create()
    $hash = ($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($path.ToLowerInvariant())) | ForEach-Object { $_.ToString('x2') }) -join ''
    return $prefix + $hash.Substring(0, 24)
}
function Esc([string]$s) { return [Security.SecurityElement]::Escape($s) }
$sb = New-Object Text.StringBuilder
function Emit-Directory([string]$dir, [string]$indent) {
    foreach ($file in Get-ChildItem -LiteralPath $dir -File | Sort-Object Name) {
        $rel = $file.FullName.Substring($dist.Length + 1)
        $fileId = if ($rel -eq 'tangenta.exe') { 'MainExe' } else { Id 'f' $rel }
        [void]$sb.AppendLine("$indent<Component Id=`"$(Id 'c' $rel)`" Bitness=`"always64`">")
        [void]$sb.AppendLine("$indent  <File Id=`"$fileId`" Source=`"$(Esc $file.FullName)`" KeyPath=`"yes`">")
        if ($rel -eq 'tangenta.exe') {
            [void]$sb.AppendLine("$indent    <Shortcut Id=`"StartMenu`" Directory=`"ProgramMenuFolder`" Name=`"TANGENTA`" Description=`"Vektorgrafik`" WorkingDirectory=`"INSTALLFOLDER`" Icon=`"TangentaIcon`" Advertise=`"yes`" />")
            [void]$sb.AppendLine("$indent  </File>")
            [void]$sb.AppendLine("$indent  <ProgId Id=`"TANGENTA.Document`" Description=`"TANGENTA-Zeichnung`" Icon=`"TangentaIcon`" Advertise=`"yes`">")
            [void]$sb.AppendLine("$indent    <Extension Id=`"vek`" ContentType=`"application/x-tangenta`">")
            [void]$sb.AppendLine("$indent      <Verb Id=`"open`" Command=`"Öffnen`" Argument=`"&quot;%1&quot;`" />")
            [void]$sb.AppendLine("$indent    </Extension>")
            [void]$sb.AppendLine("$indent  </ProgId>")
        } else {
            [void]$sb.AppendLine("$indent  </File>")
        }
        [void]$sb.AppendLine("$indent</Component>")
    }
    foreach ($sub in Get-ChildItem -LiteralPath $dir -Directory | Sort-Object Name) {
        $rel = $sub.FullName.Substring($dist.Length + 1)
        [void]$sb.AppendLine("$indent<Directory Id=`"$(Id 'd' $rel)`" Name=`"$(Esc $sub.Name)`">")
        Emit-Directory $sub.FullName ($indent + '  ')
        [void]$sb.AppendLine("$indent</Directory>")
    }
}
Emit-Directory $dist '        '
$icon = Join-Path $source 'resources\brand\tangenta.ico'

$product = @"
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs" xmlns:ui="http://wixtoolset.org/schemas/v4/wxs/ui">
  <Package Name="TANGENTA" Manufacturer="$manufacturer" Version="$version" UpgradeCode="$upgradeMsi"
           Scope="perMachine" Language="1031" Codepage="1252" Compressed="yes">
    <SummaryInformation Description="TANGENTA $version - Vektorgrafik" Manufacturer="$manufacturer" />
    <MajorUpgrade DowngradeErrorMessage="Eine neuere Version von TANGENTA ist bereits installiert." />
    <MediaTemplate EmbedCab="yes" CompressionLevel="high" />
    <Icon Id="TangentaIcon" SourceFile="$(Esc $icon)" />
    <Property Id="ARPPRODUCTICON" Value="TangentaIcon" />
    <Property Id="ARPURLINFOABOUT" Value="$homepage" />
    <Property Id="ARPHELPLINK" Value="$homepage" />
    <Property Id="DESKTOPSHORTCUT" Value="0" />
    <WixVariable Id="WixUILicenseRtf" Value="$(Esc (Join-Path $work 'license.rtf'))" />
    <ui:WixUI Id="WixUI_InstallDir" InstallDirectory="INSTALLFOLDER" />

    <StandardDirectory Id="ProgramFiles64Folder">
      <Directory Id="INSTALLFOLDER" Name="TANGENTA">
$($sb.ToString())      </Directory>
    </StandardDirectory>
    <StandardDirectory Id="ProgramMenuFolder" />
    <StandardDirectory Id="DesktopFolder">
      <Component Id="DesktopShortcut" Condition="DESKTOPSHORTCUT = 1" Bitness="always64">
        <Shortcut Id="Desktop" Name="TANGENTA" Target="[INSTALLFOLDER]tangenta.exe" WorkingDirectory="INSTALLFOLDER" Icon="TangentaIcon" />
        <RegistryValue Root="HKMU" Key="Software\TANGENTA" Name="DesktopShortcut" Type="integer" Value="1" KeyPath="yes" />
      </Component>
    </StandardDirectory>

    <Feature Id="Main" Title="TANGENTA" Level="1">
      <ComponentRef Id="DesktopShortcut" />
"@
# Alle Komponenten ins Feature
foreach ($m in [regex]::Matches($sb.ToString(), '<Component Id="([^"]+)"')) {
    $product += "      <ComponentRef Id=`"$($m.Groups[1].Value)`" />`r`n"
}
$product += @"
    </Feature>
  </Package>
</Wix>
"@
$productWxs = Join-Path $work 'tangenta.wxs'
Set-Content -Path $productWxs -Value $product -Encoding UTF8
$msi = Join-Path $out "TANGENTA-$version-x64.msi"
& $wix build $productWxs -arch x64 -ext WixToolset.UI.wixext -culture de-DE -o $msi
if ($LASTEXITCODE -ne 0) { throw 'MSI-Build fehlgeschlagen' }

# Setup.exe (WiX-Bootstrapper): /quiet, /passive, /log <datei>, InstallFolder=<ordner>, DesktopShortcut=1
$bundle = @"
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs" xmlns:bal="http://wixtoolset.org/schemas/v4/wxs/bal">
  <Bundle Name="TANGENTA $version" Manufacturer="$manufacturer" Version="$version" UpgradeCode="$upgradeBundle"
          IconSourceFile="$(Esc $icon)" AboutUrl="$homepage" HelpUrl="$homepage">
    <BootstrapperApplication>
      <bal:WixStandardBootstrapperApplication Theme="hyperlinkLicense" LicenseUrl="https://www.gnu.org/licenses/gpl-3.0.html"
                                              ShowVersion="yes" SuppressOptionsUI="no" />
    </BootstrapperApplication>
    <Variable Name="InstallFolder" Type="formatted" Value="[ProgramFiles64Folder]TANGENTA" bal:Overridable="yes" />
    <Variable Name="DesktopShortcut" Type="numeric" Value="0" bal:Overridable="yes" />
    <Chain>
      <MsiPackage SourceFile="$(Esc $msi)" Vital="yes">
        <MsiProperty Name="INSTALLFOLDER" Value="[InstallFolder]" />
        <MsiProperty Name="DESKTOPSHORTCUT" Value="[DesktopShortcut]" />
      </MsiPackage>
    </Chain>
  </Bundle>
</Wix>
"@
$bundleWxs = Join-Path $work 'bundle.wxs'
Set-Content -Path $bundleWxs -Value $bundle -Encoding UTF8
$setup = Join-Path $out "TANGENTA-$version-x64-setup.exe"
& $wix build $bundleWxs -ext WixToolset.Bal.wixext -o $setup
if ($LASTEXITCODE -ne 0) { throw 'Setup-Build fehlgeschlagen' }

Get-ChildItem $out -Filter "TANGENTA-$version-x64*" | ForEach-Object { '{0}  {1:N1} MB' -f $_.Name, ($_.Length / 1MB) }
