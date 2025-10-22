# rendirs.ps1
param(
    [switch]$n = $false
)

$Root = Get-Location
$Target = Join-Path $Root "lib"

if (-not (Test-Path $Target)) {
    Write-Error "fout: map 'lib' niet gevonden naast dit script"
    exit 1
}

# 1) pio run -t clean
if ($n) {
    Write-Host "zou uitvoeren: 'pio run -t clean'"
} else {
    if (Get-Command pio -ErrorAction SilentlyContinue) {
        Write-Host "pio clean..."
        pio run -t clean 2>$null
    }
}

# 2) verwijder .pio map
$PioPath = Join-Path $Root ".pio"
if ($n) {
    Write-Host "zou verwijderen: '$PioPath'"
} else {
    if (Test-Path $PioPath) {
        Write-Host "verwijderen: '$PioPath'"
        Remove-Item $PioPath -Recurse -Force -ErrorAction SilentlyContinue
        if (Test-Path $PioPath) {
            Write-Host "waarschuwing: '.pio' is nog in gebruik; sla schoonmaken over"
        }
    }
}

# 3) verwerk directories in lib map
Get-ChildItem $Target -Directory | ForEach-Object {
    $Dir = $_.FullName
    $Name = $_.Name
    
    # Zoek laatste wijzigingsdatum van bestanden
    $files = Get-ChildItem $Dir -Recurse -File -Force -ErrorAction SilentlyContinue
    if (-not $files) {
        Write-Host "overslaan: `"$Name`" heeft geen bestanden"
        return
    }
    
    $latestFile = $files | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $stamp = $latestFile.LastWriteTime.ToString("yyyyMMdd")
    
    # Strip bestaande datum
    $base = $Name
    $last8 = $base.Substring($base.Length - 8)
    if ($last8 -match "^\d{8}$" -and [int]$last8 -ge 20200000 -and [int]$last8 -le 20300000) {
        $base = $base.Substring(0, $base.Length - 8)
    }
    
    # Verwijder trailing karakters
    $base = $base.TrimEnd(' ', '-', '_')
    
    $newName = $base + $stamp
    
    if ($newName -eq $Name) {
        Write-Host "up-to-date: `"$Name`""
    } else {
        if ($n) {
            Write-Host "zou hernoemen: `"$Name`"  ->  `"$newName`""
        } else {
            $newPath = Join-Path $Target $newName
            if (Test-Path $newPath) {
                Write-Host "conflict: `"$newName`" bestaat al"
            } else {
                Write-Host "rename: `"$Name`"  ->  `"$newName`""
                Rename-Item $Dir $newName
            }
        }
    }
}