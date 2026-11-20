<#
.SYNOPSIS
  Compara el arranque y la memoria de Sora con los de Chrome en este PC.

.DESCRIPTION
  Para cada navegador hace una ejecución de calentamiento (descartada) y luego -Runs ejecuciones:
    - Arranque: tiempo desde que se lanza el proceso hasta que tiene ventana (MainWindowHandle).
    - Memoria: suma de la memoria privada de todo el árbol de procesos, -SettleSeconds segundos
      después de abrir -Url.
  Chrome usa un perfil propio y temporal (no toca tu Chrome). Sora usa su perfil normal, con
  las listas de bloqueo ya descargadas, igual que en el uso diario.
  Compatible con Windows PowerShell 5.1, sin módulos externos.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File bench.ps1 -Sora "C:\Sora\sora.exe" -Runs 10
#>
param(
    [Parameter(Mandatory = $true)][string]$Sora,
    [string]$Chrome = "",
    [int]$Runs = 10,
    [string]$Url = "https://es.wikipedia.org/wiki/Navegador_web",
    [int]$SettleSeconds = 15,
    [int]$StartTimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

function Find-Chrome {
    $candidates = @(
        "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
        "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
        "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe"
    )
    foreach ($c in $candidates) { if ($c -and (Test-Path $c)) { return $c } }
    throw "No se encuentra Chrome. Indica la ruta con -Chrome."
}

function Get-ProcessTree([int]$RootId) {
    $all = Get-CimInstance Win32_Process | Select-Object ProcessId, ParentProcessId
    $ids = New-Object System.Collections.Generic.List[int]
    $ids.Add($RootId)
    $i = 0
    while ($i -lt $ids.Count) {
        $parent = $ids[$i]
        foreach ($p in $all) {
            if ($p.ParentProcessId -eq $parent -and -not $ids.Contains([int]$p.ProcessId)) { $ids.Add([int]$p.ProcessId) }
        }
        $i++
    }
    return $ids
}

function Get-TreeMemoryMB([int]$RootId) {
    $total = 0
    foreach ($id in (Get-ProcessTree $RootId)) {
        $p = Get-Process -Id $id -ErrorAction SilentlyContinue
        if ($p) { $total += $p.PrivateMemorySize64 }
    }
    return [math]::Round($total / 1MB, 1)
}

function Stop-Tree([int]$RootId) {
    foreach ($id in (Get-ProcessTree $RootId)) { Stop-Process -Id $id -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 2
}

function Measure-Once([string]$Exe, [string[]]$BrowserArgs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $proc = Start-Process -FilePath $Exe -ArgumentList $BrowserArgs -PassThru
    $startMs = $null
    while ($sw.Elapsed.TotalSeconds -lt $StartTimeoutSeconds) {
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { $startMs = $sw.ElapsedMilliseconds; break }
        Start-Sleep -Milliseconds 5
    }
    if ($null -eq $startMs) {
        Stop-Tree $proc.Id
        throw "$Exe no mostró ventana en $StartTimeoutSeconds s."
    }
    Start-Sleep -Seconds $SettleSeconds
    $memMB = Get-TreeMemoryMB $proc.Id
    Stop-Tree $proc.Id
    return [pscustomobject]@{ StartMs = $startMs; MemoryMB = $memMB }
}

function Get-Percentile([double[]]$Values, [double]$P) {
    $sorted = $Values | Sort-Object
    $index = [math]::Ceiling($P * $sorted.Count) - 1
    if ($index -lt 0) { $index = 0 }
    return $sorted[$index]
}

function Measure-Browser([string]$Name, [string]$Exe, [string[]]$BrowserArgs) {
    Write-Host "== ${Name}: calentamiento" -ForegroundColor Cyan
    Measure-Once $Exe $BrowserArgs | Out-Null
    $results = @()
    for ($i = 1; $i -le $Runs; $i++) {
        $r = Measure-Once $Exe $BrowserArgs
        Write-Host ("   {0}/{1}: arranque {2} ms, memoria {3} MB" -f $i, $Runs, $r.StartMs, $r.MemoryMB)
        $results += $r
    }
    $start = [double[]]($results | ForEach-Object { $_.StartMs })
    $mem = [double[]]($results | ForEach-Object { $_.MemoryMB })
    return [pscustomobject]@{
        Navegador          = $Name
        'Arranque mediana' = "{0} ms" -f (Get-Percentile $start 0.5)
        'Arranque p90'     = "{0} ms" -f (Get-Percentile $start 0.9)
        'Memoria mediana'  = "{0} MB" -f (Get-Percentile $mem 0.5)
        'Memoria p90'      = "{0} MB" -f (Get-Percentile $mem 0.9)
    }
}

if (-not (Test-Path $Sora)) { throw "No se encuentra Sora en $Sora" }
if (-not $Chrome) { $Chrome = Find-Chrome }

$chromeProfile = Join-Path $env:TEMP "sora-bench-chrome-profile"
New-Item -ItemType Directory -Force -Path $chromeProfile | Out-Null

$chromeVersion = (Get-Item $Chrome).VersionInfo.ProductVersion
$cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name
$ramGB = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
$os = (Get-CimInstance Win32_OperatingSystem).Caption

$summary = @()
$summary += Measure-Browser "Sora" $Sora @($Url)
$summary += Measure-Browser "Chrome $chromeVersion" $Chrome @("--user-data-dir=`"$chromeProfile`"", "--no-first-run", "--no-default-browser-check", $Url)

Write-Host ""
Write-Host "Equipo: $cpu, $ramGB GB RAM, $os"
Write-Host "URL: $Url · ejecuciones: $Runs · espera para memoria: $SettleSeconds s"
$summary | Format-Table -AutoSize
