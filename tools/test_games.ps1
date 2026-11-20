# Lancia i giochi-modello e i due giochi veri per 30 secondi.
# ROSSO se nel log c'e' un errore Lua o il processo muore.
# Non modifica i progetti (dead-rails compreso).

$ErrorActionPreference = "Continue"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$exe = Join-Path $root "build\bin\BlockyGameEngine.exe"
$shotDir = Join-Path $root "build\shots"
if (-not (Test-Path $exe)) {
    Write-Host "ROSSO: manca BlockyGameEngine.exe"
    exit 1
}
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null

$games = @(
    @{ n = "picchiaduro"; rojo = (Join-Path $root "giochi\modelli\picchiaduro\default.project.json") },
    @{ n = "sandbox";     rojo = (Join-Path $root "giochi\modelli\sandbox\default.project.json") },
    @{ n = "citta";       rojo = (Join-Path $root "giochi\modelli\citta\default.project.json") },
    @{ n = "escape";      rojo = (Join-Path $root "giochi\modelli\escape\default.project.json") },
    @{ n = "platform";    rojo = (Join-Path $root "giochi\modelli\platform\default.project.json") },
    @{ n = "party";       rojo = (Join-Path $root "giochi\modelli\party\default.project.json") },
    @{ n = "ultimo";      rojo = (Join-Path $root "giochi\UltimoBinario\default.project.json") },
    @{ n = "deadrails";   rojo = "C:\Users\User\Documents\dead-rails\default.project.json" }
)

$pattern = '(?m)^.*(?:Lua error:|PlayerAdded:|CharacterAdded:|Heartbeat:|Heartbeat error:|Stepped:|RenderStepped:|attempt to |va usato dentro|stack traceback).*$'
$fail = New-Object System.Collections.Generic.List[string]

foreach ($g in $games) {
    if (-not (Test-Path $g.rojo)) {
        $fail.Add("$($g.n) (manca $($g.rojo))")
        Write-Host "FAIL  $($g.n) progetto assente"
        continue
    }
    $log = Join-Path $shotDir ($g.n + ".log")
    $png = Join-Path $shotDir ($g.n + ".png")
    if (Test-Path $log) { Remove-Item $log -Force }
    Write-Host "---- $($g.n) ----"
    $cmd = "set BLOCKY_SHOT=$png&& set BLOCKY_SHOT_SEC=30&& set BLOCKY_SHOT_PLACE=baseplate&& set BLOCKY_SHOT_PLAY=1&& set BLOCKY_ROJO=$($g.rojo)&& `"$exe`" > `"$log`" 2>&1"
    cmd /c $cmd
    $code = $LASTEXITCODE
    $text = ""
    if (Test-Path $log) {
        $bytes = [System.IO.File]::ReadAllBytes($log)
        $text = [System.Text.Encoding]::Unicode.GetString($bytes)
        if ($text -notmatch "Blocky" -and $text -notmatch "error" -and $text -notmatch "Lua") {
            $text = [System.Text.Encoding]::UTF8.GetString($bytes)
        }
        $text = $text.Replace([string][char]0, "")
    }
    $hit = [regex]::Match($text, $pattern)
    if ($code -ne 0 -or $hit.Success) {
        $why = if ($hit.Success) { $hit.Value.Trim() } else { "exit $code" }
        $fail.Add("$($g.n): $why")
        Write-Host "FAIL  $($g.n): $why"
    } else {
        Write-Host "OK    $($g.n)"
    }
}

Write-Host ""
if ($fail.Count -eq 0) {
    Write-Host "VERDE  ($($games.Count) giochi)"
    exit 0
}
Write-Host "ROSSO  ($($fail.Count) giochi)"
foreach ($f in $fail) { Write-Host "  $f" }
exit 1
