<#
.SYNOPSIS
  Compara el arranque y la memoria de Sora con los de Chrome en este PC.

.DESCRIPTION
  Para cada navegador hace una ejecución de calentamiento (descartada) y luego -Runs ejecuciones:
    - Arranque: tiempo desde que se lanza el proceso hasta que tiene ventana (MainWindowHandle).
    - Carga: desde el lanzamiento hasta el evento load de -Url, leído por DevTools en el puerto
      local -DebugPort (los dos navegadores se miden igual).
    - Memoria: suma de la memoria privada de todo el árbol de procesos, -SettleSeconds segundos
      después de que aparezca la ventana.
  Al terminar cada ejecución se cierra la ventana con normalidad y solo se fuerza si no sale.
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
    [int]$StartTimeoutSeconds = 30,
    [int]$DebugPort = 9333
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

function Stop-Tree([System.Diagnostics.Process]$Proc) {
    # Cerrar la ventana deja que el navegador guarde su perfil; solo se fuerza si no sale.
    $ids = Get-ProcessTree $Proc.Id
    if ($Proc.CloseMainWindow()) { $Proc.WaitForExit(10000) | Out-Null }
    foreach ($id in $ids) { Stop-Process -Id $id -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 2
}

# Evalúa una expresión de JavaScript en la página por el protocolo de DevTools (sin módulos externos).
function Invoke-CdpEval([string]$WsUrl, [string]$Expression) {
    $ws = New-Object System.Net.WebSockets.ClientWebSocket
    $none = [Threading.CancellationToken]::None
    try {
        $ws.ConnectAsync([Uri]$WsUrl, $none).Wait(2000) | Out-Null
        $msg = @{ id = 1; method = "Runtime.evaluate"; params = @{ expression = $Expression; returnByValue = $true } } | ConvertTo-Json -Compress -Depth 5
        $bytes = [Text.Encoding]::UTF8.GetBytes($msg)
        $ws.SendAsync((New-Object 'System.ArraySegment[byte]' -ArgumentList (, $bytes)), "Text", $true, $none).Wait(2000) | Out-Null
        $buffer = New-Object byte[] 65536
        $text = ""
        do {
            $task = $ws.ReceiveAsync((New-Object 'System.ArraySegment[byte]' -ArgumentList (, $buffer)), $none)
            if (-not $task.Wait(2000)) { return $null }
            $text += [Text.Encoding]::UTF8.GetString($buffer, 0, $task.Result.Count)
        } while (-not $task.Result.EndOfMessage)
        return ($text | ConvertFrom-Json).result.result.value
    } catch { return $null } finally { $ws.Dispose() }
}

# Momento (ms Unix) en que terminó de cargar la página, o $null si no termina a tiempo.
function Wait-PageLoad([int]$Port, [double]$TimeoutSeconds) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        try { $targets = Invoke-RestMethod "http://127.0.0.1:$Port/json/list" -TimeoutSec 1 } catch { $targets = $null }
        $page = $targets | Where-Object { $_.type -eq "page" -and $_.url -like "http*" } | Select-Object -First 1
        if ($page) {
            $end = Invoke-CdpEval $page.webSocketDebuggerUrl "performance.timing.loadEventEnd"
            if ($end -gt 0) { return [double]$end }
        }
        Start-Sleep -Milliseconds 100
    }
    return $null
}

function Measure-Once([string]$Exe, [string[]]$BrowserArgs) {
    $launched = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $proc = Start-Process -FilePath $Exe -ArgumentList $BrowserArgs -PassThru
    $startMs = $null
    while ($sw.Elapsed.TotalSeconds -lt $StartTimeoutSeconds) {
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { $startMs = $sw.ElapsedMilliseconds; break }
        Start-Sleep -Milliseconds 5
    }
    if ($null -eq $startMs) {
        Stop-Tree $proc
        throw "$Exe no mostró ventana en $StartTimeoutSeconds s."
    }
    $loadEnd = Wait-PageLoad $DebugPort $StartTimeoutSeconds
    $loadMs = if ($loadEnd) { [math]::Round($loadEnd - $launched) } else { $null }
    $remaining = $SettleSeconds - $sw.Elapsed.TotalSeconds + $startMs / 1000
    if ($remaining -gt 0) { Start-Sleep -Milliseconds ([int]($remaining * 1000)) }
    $memMB = Get-TreeMemoryMB $proc.Id
    Stop-Tree $proc
    return [pscustomobject]@{ StartMs = $startMs; LoadMs = $loadMs; MemoryMB = $memMB }
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
        $load = if ($null -ne $r.LoadMs) { "$($r.LoadMs) ms" } else { "sin dato" }
        Write-Host ("   {0}/{1}: arranque {2} ms, carga {3}, memoria {4} MB" -f $i, $Runs, $r.StartMs, $load, $r.MemoryMB)
        $results += $r
    }
    $start = [double[]]($results | ForEach-Object { $_.StartMs })
    $loads = [double[]]($results | Where-Object { $null -ne $_.LoadMs } | ForEach-Object { $_.LoadMs })
    $mem = [double[]]($results | ForEach-Object { $_.MemoryMB })
    return [pscustomobject]@{
        Navegador          = $Name
        'Arranque mediana' = "{0} ms" -f (Get-Percentile $start 0.5)
        'Arranque p90'     = "{0} ms" -f (Get-Percentile $start 0.9)
        'Carga mediana'    = if ($loads.Count) { "{0} ms" -f (Get-Percentile $loads 0.5) } else { "sin dato" }
        'Carga p90'        = if ($loads.Count) { "{0} ms" -f (Get-Percentile $loads 0.9) } else { "sin dato" }
        'Memoria mediana'  = "{0} MB" -f (Get-Percentile $mem 0.5)
        'Memoria p90'      = "{0} MB" -f (Get-Percentile $mem 0.9)
    }
}

if (-not (Test-Path $Sora)) { throw "No se encuentra Sora en $Sora" }
if (-not $Chrome) { $Chrome = Find-Chrome }

$chromeProfile = Join-Path $env:TEMP "sora-bench-chrome-profile"
New-Item -ItemType Directory -Force -Path $chromeProfile | Out-Null

# Con un navegador ya abierto los datos no valen: Sora comparte perfil y Chrome reutiliza su proceso.
$soraName = [IO.Path]::GetFileNameWithoutExtension($Sora)
if (Get-Process -Name $soraName -ErrorAction SilentlyContinue) { throw "Cierra Sora antes de medir." }
$chromeBusy = Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*sora-bench-chrome-profile*" }
if ($chromeBusy) { throw "Hay un Chrome abierto con el perfil de pruebas; ciérralo antes de medir." }
$portBusy = $true
try { (New-Object Net.Sockets.TcpClient).Connect("127.0.0.1", $DebugPort) } catch { $portBusy = $false }
if ($portBusy) { throw "El puerto $DebugPort está ocupado; usa -DebugPort." }

# DevTools en el puerto local para medir la carga de página igual en los dos navegadores.
$env:QTWEBENGINE_REMOTE_DEBUGGING = "127.0.0.1:$DebugPort"

$chromeVersion = (Get-Item $Chrome).VersionInfo.ProductVersion
$cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name
$ramGB = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
$os = (Get-CimInstance Win32_OperatingSystem).Caption

$summary = @()
$summary += Measure-Browser "Sora" $Sora @($Url)
$summary += Measure-Browser "Chrome $chromeVersion" $Chrome @("--user-data-dir=`"$chromeProfile`"", "--no-first-run", "--no-default-browser-check", "--remote-debugging-port=$DebugPort", $Url)
Remove-Item Env:QTWEBENGINE_REMOTE_DEBUGGING

Write-Host ""
Write-Host "Equipo: $cpu, $ramGB GB RAM, $os"
Write-Host "URL: $Url · ejecuciones: $Runs · espera para memoria: $SettleSeconds s"
Write-Host "Perfiles: Sora usa el suyo normal (con caché y listas); Chrome uno temporal en $chromeProfile"
Write-Host "Arranque: hasta que el proceso tiene ventana. Carga: desde el lanzamiento hasta el evento load de la página."
$summary | Format-Table -AutoSize
