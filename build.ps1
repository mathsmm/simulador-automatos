# Compila o automatoc no Windows.
# Requer: win_flex/win_bison (ou flex/bison) e g++ (MinGW-w64) no PATH.
#
#   .\build.ps1            compila em build\automatoc.exe
#   .\build.ps1 -Limpar    apaga a pasta build antes
param([switch]$Limpar)

$ErrorActionPreference = 'Stop'
$raiz = $PSScriptRoot
$build = Join-Path $raiz 'build'
$src = Join-Path $raiz 'src'

function Find-Ferramenta([string[]]$nomes) {
    foreach ($n in $nomes) {
        $cmd = Get-Command $n -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    throw "Nenhuma destas ferramentas foi encontrada no PATH: $($nomes -join ', ')"
}

function Invoke-Etapa([string]$descricao, [string]$exe, [string[]]$argumentos) {
    Write-Host "==> $descricao"
    & $exe @argumentos
    if ($LASTEXITCODE -ne 0) { throw "falhou: $descricao" }
}

$flex = Find-Ferramenta @('win_flex', 'flex')
$bison = Find-Ferramenta @('win_bison', 'bison')
$cxx = Find-Ferramenta @('g++', 'clang++')

if ($Limpar -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }
New-Item -ItemType Directory -Force $build | Out-Null

Invoke-Etapa 'bison: parser.y -> parser.cpp' $bison @(
    '-Wall', "--defines=$build\parser.hpp", '-o', "$build\parser.cpp", "$src\parser.y")
Invoke-Etapa 'flex: lexer.l -> lexer.cpp' $flex @('-o', "$build\lexer.cpp", "$src\lexer.l")

$fontes = @(Get-ChildItem "$src\*.cpp" | ForEach-Object { $_.FullName }) +
          @("$build\parser.cpp", "$build\lexer.cpp")
Invoke-Etapa 'g++: compilando automatoc.exe' $cxx (@(
    '-std=c++17', '-O2', '-Wall', '-Wextra', '-Wno-unused-function',
    "-I$build", "-I$src") + $fontes + @('-o', "$build\automatoc.exe", '-static'))

Write-Host "OK: $build\automatoc.exe"
