<#
.SYNOPSIS
  Genera assets\sora.ico a partir de assets\sora.svg (16 a 256 px, fondo transparente).
.DESCRIPTION
  Dibuja cada tamaño con Microsoft Edge sin ventana y junta los PNG en un .ico. Solo hace falta
  volver a ejecutarlo si cambia el SVG; el .ico generado se guarda en el repositorio.
#>
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$svg = Join-Path $root "assets\sora.svg"
$ico = Join-Path $root "assets\sora.ico"
$edge = @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe", "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $edge) { throw "Hace falta Microsoft Edge para dibujar el icono." }

$work = Join-Path $env:TEMP "sora-icon"
New-Item -ItemType Directory -Force -Path $work | Out-Null
$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = @()
foreach ($size in $sizes) {
    $html = Join-Path $work "icon-$size.html"
    $png = Join-Path $work "icon-$size.png"
    Set-Content -Encoding utf8 -Path $html -Value "<html><body style='margin:0;background:transparent'><img src='file:///$($svg.Replace('\','/'))' width='$size' height='$size'></body></html>"
    Remove-Item $png -ErrorAction SilentlyContinue
    & $edge --headless=new --disable-gpu "--user-data-dir=$work\profile" --default-background-color=00000000 --hide-scrollbars `
        "--window-size=$size,$size" "--screenshot=$png" ("file:///" + $html.Replace('\','/')) 2>$null
    for ($i = 0; $i -lt 50 -and -not (Test-Path $png); $i++) { Start-Sleep -Milliseconds 100 }
    if (-not (Test-Path $png)) { throw "Edge no generó el icono de $size px." }
    $pngs += , [IO.File]::ReadAllBytes($png)
}

# Formato .ico: cabecera, una entrada por tamaño y los PNG tal cual (Windows Vista+).
$ms = New-Object IO.MemoryStream
$w = New-Object IO.BinaryWriter $ms
$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $w.Write([byte]($s % 256)); $w.Write([byte]($s % 256)) # 256 se escribe como 0
    $w.Write([byte]0); $w.Write([byte]0); $w.Write([UInt16]1); $w.Write([UInt16]32)
    $w.Write([UInt32]$pngs[$i].Length); $w.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $w.Write($p) }
$w.Flush()
[IO.File]::WriteAllBytes($ico, $ms.ToArray())
Write-Host "Icono listo: $ico ($($sizes -join ', ') px)"
