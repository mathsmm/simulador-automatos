#!/bin/sh
# Suíte de testes do automatoc (Linux / macOS / MSYS2). Ver executar.ps1.
raiz=$(cd "$(dirname "$0")/.." && pwd)
exe="$raiz/build/automatoc"
[ -x "$exe" ] || exe="$raiz/build/automatoc.exe"
tmp=$(mktemp -d)
ok=0; falhas=0

echo "Programas válidos (exemplos):"
for f in "$raiz"/exemplos/*.aut; do
    nome=$(basename "$f")
    if saida=$("$exe" "$f" --nao-abrir -o "$tmp/${nome%.aut}.svg" 2>&1); then
        ok=$((ok+1)); echo "  ok     $nome"
    else
        falhas=$((falhas+1)); echo "  FALHOU $nome"; echo "$saida"
    fi
done

echo "Programas inválidos:"
for f in "$raiz"/testes/invalidos/*.aut; do
    nome=$(basename "$f")
    esperado=$(head -n 1 "$f" | sed 's#^//[[:space:]]*ESPERA:[[:space:]]*##')
    if saida=$("$exe" "$f" --nao-abrir -o "$tmp/${nome%.aut}.svg" 2>&1); then
        falhas=$((falhas+1)); echo "  FALHOU $nome (deveria ter falhado)"
    elif printf '%s' "$saida" | grep -qF -- "$esperado"; then
        ok=$((ok+1)); echo "  ok     $nome"
    else
        falhas=$((falhas+1)); echo "  FALHOU $nome (esperava '$esperado')"; echo "$saida"
    fi
done

rm -rf "$tmp"
echo; echo "$ok passaram, $falhas falharam"
[ "$falhas" -eq 0 ]
