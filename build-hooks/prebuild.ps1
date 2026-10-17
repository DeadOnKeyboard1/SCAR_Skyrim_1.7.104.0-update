$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $env:SKSE_PROJECT_ROOT
$python = $env:SKSE_PYTHON_EXE
if (!$python) { $python = (Get-Command python.exe -ErrorAction Stop).Source }
& $python -B scripts/Preflight.py
if ($LASTEXITCODE) { throw 'SCAR preflight failed; inspect the redacted preflight log.' }
