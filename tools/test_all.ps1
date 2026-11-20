# Compila e lancia tutti i test di Blocky, tranne test_hang_repro e test_pool.
# Stampa VERDE se tutto passa, ROSSO se qualcosa fallisce.
# Uscita 0 solo con VERDE. Lo lancia anche il hook git pre-commit.

$ErrorActionPreference = "Continue"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $root

$skip = @("test_hang_repro", "test_pool")

if (-not (Test-Path (Join-Path $root "build\build.ninja"))) {
    Write-Host "ROSSO: manca build\. Lancia cmake dalla cartella del progetto."
    exit 1
}

$raw = & ninja -C build -t targets all 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "ROSSO: ninja non elenca i target."
    exit 1
}

$targets = New-Object System.Collections.Generic.List[string]
foreach ($line in $raw) {
    $name = ($line.ToString() -split ":", 2)[0].Trim()
    if ($name -match "^test_[A-Za-z0-9_]+$" -and $name -notlike "*.exe" -and $skip -notcontains $name) {
        if (-not $targets.Contains($name)) { $targets.Add($name) }
    }
}
if ($targets.Count -eq 0) {
    Write-Host "ROSSO: nessun target di test."
    exit 1
}

Write-Host "=== Compilo $($targets.Count) test ==="
$cmakeArgs = @("--build", "build")
foreach ($t in $targets) { $cmakeArgs += @("--target", $t) }
$cmakeArgs += @("-j", "8")
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "ROSSO: compilazione fallita"
    exit 1
}

$fail = New-Object System.Collections.Generic.List[string]
$bin = Join-Path $root "build\bin"
foreach ($t in $targets) {
    $exe = Join-Path $bin ($t + ".exe")
    if (-not (Test-Path $exe)) {
        $fail.Add("$t (exe assente)")
        Write-Host "FAIL  $t (exe assente)"
        continue
    }
    Write-Host "---- $t ----"
    & $exe
    if ($LASTEXITCODE -ne 0) {
        $fail.Add("$t (exit $LASTEXITCODE)")
        Write-Host "FAIL  $t"
    } else {
        Write-Host "OK    $t"
    }
}

Write-Host "---- giochi ----"
& (Join-Path $PSScriptRoot "test_games.ps1")
if ($LASTEXITCODE -ne 0) { $fail.Add("giochi (exit $LASTEXITCODE)") }

Write-Host ""
if ($fail.Count -eq 0) {
    Write-Host "VERDE  ($($targets.Count) test + giochi)"
    exit 0
}
Write-Host "ROSSO  ($($fail.Count) falliti su $($targets.Count))"
foreach ($f in $fail) { Write-Host "  $f" }
exit 1
