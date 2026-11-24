# Suíte de testes do automatoc (Windows / PowerShell).
#  - todo exemplos\*.aut deve compilar (código de saída 0);
#  - todo testes\invalidos\*.aut deve falhar e a saída deve conter o texto
#    indicado na primeira linha do arquivo:  // ESPERA: <trecho da mensagem>
$ErrorActionPreference = 'Stop'
$raiz = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $raiz 'build\automatoc.exe'
if (-not (Test-Path $exe)) { throw "Compile antes com .\build.ps1 ($exe não existe)" }

$tmp = Join-Path ([IO.Path]::GetTempPath()) 'automatoc-testes'
New-Item -ItemType Directory -Force $tmp | Out-Null

function Invoke-Automatoc([string]$arquivo, [string]$svg) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = "`"$arquivo`" --nao-abrir -o `"$svg`""
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = [Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [Text.Encoding]::UTF8
    $p = [Diagnostics.Process]::Start($psi)
    $saida = $p.StandardOutput.ReadToEndAsync()
    $erros = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    [pscustomobject]@{ Codigo = $p.ExitCode; Texto = $saida.Result + $erros }
}

$ok = 0; $falhas = 0
function Write-Resultado([bool]$passou, [string]$nome, [string]$detalhe) {
    if ($passou) { $script:ok++; Write-Host "  ok     $nome" -ForegroundColor Green }
    else { $script:falhas++; Write-Host "  FALHOU $nome  $detalhe" -ForegroundColor Red }
}

Write-Host 'Programas válidos (exemplos):'
foreach ($f in Get-ChildItem (Join-Path $raiz 'exemplos\*.aut')) {
    $r = Invoke-Automatoc $f.FullName (Join-Path $tmp ($f.BaseName + '.svg'))
    Write-Resultado ($r.Codigo -eq 0) $f.Name "(código $($r.Codigo))`n$($r.Texto)"
}

Write-Host 'Programas inválidos:'
foreach ($f in Get-ChildItem (Join-Path $PSScriptRoot 'invalidos\*.aut')) {
    $primeira = [IO.File]::ReadAllLines($f.FullName, [Text.Encoding]::UTF8)[0]
    $esperado = $primeira -replace '^//\s*ESPERA:\s*', ''
    $r = Invoke-Automatoc $f.FullName (Join-Path $tmp ($f.BaseName + '.svg'))
    if ($r.Codigo -eq 0) { Write-Resultado $false $f.Name '(deveria ter falhado)' }
    elseif (-not $r.Texto.Contains($esperado)) {
        Write-Resultado $false $f.Name "(esperava '$esperado')`n$($r.Texto)"
    } else { Write-Resultado $true $f.Name '' }
}

Write-Host "`n$ok passaram, $falhas falharam"
if ($falhas -gt 0) { exit 1 }
