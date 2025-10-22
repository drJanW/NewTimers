#Requires -Version 5.1
param([string]$Suffix = "")

$ErrorActionPreference = 'Stop'

# --- paden en namen ---
$root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
Set-Location -LiteralPath $root

$ts     = Get-Date -Format 'yyyy-MM-dd_HHmm'
$outDir = Join-Path -Path $root -ChildPath 'backup'
$stage  = Join-Path -Path $root -ChildPath '_backup_stage'
$list   = Join-Path -Path $root -ChildPath 'libs_list.txt'
$zip    = Join-Path -Path $outDir -ChildPath ("Backup_{0}{1}.zip" -f $ts, $(if($Suffix){"_$Suffix"}))

# --- voorbereiden ---
if (-not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

if (-not (Test-Path -LiteralPath $list)) { throw "libs_list.txt niet gevonden: $list" }

# --- helper: kies laatste lib\KEY???????? map ---
function Get-LatestLibDir([string]$key) {
  $libRoot = Join-Path -Path $root -ChildPath 'lib'
  if (-not (Test-Path -LiteralPath $libRoot)) { return $null }
  $dirs = Get-ChildItem -LiteralPath $libRoot -Directory -ErrorAction SilentlyContinue |
          Where-Object { $_.Name -like ($key + '????????') } |
          Sort-Object Name
  if ($dirs) { return ('lib\' + $dirs[-1].Name) }
  return $null
}

# --- verzamel items ---
$items = New-Object System.Collections.Generic.List[string]
if (Test-Path -LiteralPath 'src\main.cpp') { $items.Add('F|src\main.cpp') }
if (Test-Path -LiteralPath 'include')      { $items.Add('D|include') }

Get-Content -LiteralPath $list | ForEach-Object {
  $line = $_.Trim()
  if (-not $line) { return }
  if ($line[0] -eq ';') { return }

  if ($line[0] -eq '+') {
    $p = $line.Substring(1).Trim()
    if (Test-Path -LiteralPath ($p + '\*')) { $items.Add('D|' + $p); Write-Host ('+DIR  ' + $p) }
    elseif (Test-Path -LiteralPath $p)      { $items.Add('F|' + $p); Write-Host ('+FILE ' + $p) }
    else { Write-Warning ("niet gevonden: {0}" -f $p) }
  } else {
    $found = Get-LatestLibDir -key $line
    if ($found) { $items.Add('D|' + $found); Write-Host ('+LIB  ' + $found) }
    else { Write-Warning ("geen map voor lib\{0}*" -f $line) }
  }
}

# --- kopiëren naar staging ---
foreach ($entry in $items) {
  $type,$rel = $entry.Split('|',2)
  if (-not (Test-Path -LiteralPath $rel)) { Write-Warning ("skip {0}" -f $rel); continue }
  $dest = Join-Path -Path $stage -ChildPath $rel
  New-Item -ItemType Directory -Force -Path (Split-Path -LiteralPath $dest) | Out-Null
  if ($type -eq 'D') { Copy-Item -LiteralPath $rel -Destination $dest -Recurse -Force }
  else { Copy-Item -LiteralPath $rel -Destination $dest -Force }
}

# --- zip ---
Write-Host ('Maak ZIP: "' + $zip + '"')
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path (Join-Path -Path $stage -ChildPath '*') -DestinationPath $zip -Force

# --- opruimen ---
Remove-Item -LiteralPath $stage -Recurse -Force
Write-Host ('OK: "' + $zip + '"')
