# automatoc: compilador de uma DSL para autômatos finitos

`automatoc` compila uma linguagem de domínio específico (DSL) para descrever
autômatos finitos (AFD, AFN e AFN-ε) e traduz o programa para **SVG**, o
diagrama de estados. O pipeline tem **um passo só**: um comando analisa o
código, simula as palavras de teste, gera o SVG e o abre no visualizador
padrão. O usuário não mexe com a linguagem de destino em nenhum momento.

```aut
AUTOMATO "Reconhecedor";
ESTADOS q0 (INICIAL), q1 (ACEITACAO);
TRANSICAO q0 -> q1 COM 'a';
TRANSICAO q1 -> q1 COM 'b';
SAIDA "automato-1.svg"
```

```powershell
.\build\automatoc.exe exemplos\reconhecedor.aut
```

```
automatoc: compilando exemplos\reconhecedor.aut
  ✓ análise léxica, sintática e semântica (0 avisos)
  autômato "Reconhecedor": AFD (parcial), 2 estados, 2 transições, Σ = {a, b}
  ✓ SVG gerado: exemplos\automato-1.svg
  → abrindo no visualizador padrão...
```

Ferramentas: **C++17**, **Flex** (análise léxica), **Bison** (análise
sintática LALR(1)) e **Tradução Dirigida por Sintaxe** (análise semântica).

---

## 1. Compilação

### Windows

Pré-requisitos: g++ (MinGW-w64) e WinFlexBison. Com o `winget`:

```powershell
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

```powershell
winget install WinFlexBison.win_flex_bison
```

Para compilar e testar:

```powershell
.\build.ps1
```

```powershell
.\testes\executar.ps1
```

### Linux, macOS ou MSYS2

```sh
make            # gera build/automatoc
make testes     # roda a suíte de testes
make exemplos   # compila todos os exemplos sem abrir os SVGs
```

## 2. Uso

```
automatoc <arquivo.aut> [opções]

  -o, --saida <arquivo>  grava o SVG neste caminho (ignora SAIDA)
      --nao-abrir        não abre o SVG ao final
  -h, --ajuda            mostra a ajuda
  -v, --versao           mostra a versão
```

O caminho de `SAIDA` é relativo à pasta do arquivo `.aut`. O programa
retorna 0 em caso de sucesso e 1 se houver erros.

---

## 3. A linguagem

| Comando | Significado |
|---|---|
| `AUTOMATO "nome";` | Cabeçalho obrigatório; dá nome ao autômato. |
| `ESTADOS q0 (INICIAL), q1 (ACEITACAO), q2;` | Declara estados. Modificadores: `INICIAL`, `ACEITACAO` (podem ser combinados: `(INICIAL, ACEITACAO)`). Pode aparecer mais de uma vez. |
| `TRANSICAO q0 -> q1 COM 'a';` | Transição de `q0` para `q1` lendo `a`. Aceita vários símbolos: `COM 'a', 'b'`. `EPSILON` (ou `ε`) indica transição vazia. |
| `SAIDA "arquivo.svg"` | Arquivo gerado. Se omitido, usa o nome do autômato. |
| `TESTAR "ab", "abb";` | *(extensão)* Simula o autômato sobre cada palavra e mostra ACEITA/REJEITA com a trilha de estados, no terminal e no SVG. |

* Comandos são separados por `;`. O `;` depois do último comando é opcional.
* Comentários: `// até o fim da linha` e `/* bloco */`.
* Palavras reservadas são em maiúsculas. As grafias acentuadas também valem
  (`AUTÔMATO`, `TRANSIÇÃO`, `ACEITAÇÃO`, `SAÍDA`).
* Escapes em símbolos e textos: `'\''`, `'\\'`, `'\n'`, `'\t'`, `"\""`.

Mais exemplos em [exemplos/](exemplos): AFD completo, AFD de múltiplos de 3
em binário, AFN e AFN-ε.

---

## 4. Arquitetura

```
 arquivo.aut
     │
     ▼
 ┌────────────┐  tokens   ┌──────────────────────────────┐   RI (Automato)
 │ lexer.l    │ ────────▶ │ parser.y  (LALR(1))          │ ───────────────┐
 │ Flex       │  yylval   │  + ações semânticas (TDS) ──▶│ semantico.cpp  │
 └────────────┘  yylloc   └──────────────────────────────┘ tabela de      │
                                                           símbolos       ▼
                          ┌──────────────┐      ┌───────────────┐   ┌───────────┐
                          │ abre o SVG   │ ◀─── │ gerador_svg   │ ◀─│ simulação │
                          │ (plataforma) │      │ (código alvo) │   │ (TESTAR)  │
                          └──────────────┘      └───────────────┘   └───────────┘
```

| Arquivo | Papel |
|---|---|
| [src/lexer.l](src/lexer.l) | Análise léxica (Flex): tokens, posições, erros léxicos. |
| [src/parser.y](src/parser.y) | Análise sintática (Bison) e esquema de tradução (ações semânticas). |
| [src/semantico.cpp](src/semantico.cpp) | Ações da TDS: tabela de símbolos e regras semânticas. |
| [src/automato.cpp](src/automato.cpp) | Representação intermediária, classificação AFD/AFN e simulação. |
| [src/gerador_svg.cpp](src/gerador_svg.cpp) | Geração de código: layout em camadas (BFS) e emissão do SVG. |
| [src/diagnostico.cpp](src/diagnostico.cpp) | Mensagens `arquivo:linha:coluna`, com o trecho do código sublinhado. |
| [src/main.cpp](src/main.cpp) | Driver do pipeline de um passo. |

---

## 5. Especificação léxica (tokens)

| Token | Expressão regular (Flex) | Valor semântico |
|---|---|---|
| `AUTOMATO` | `"AUTOMATO"\|"AUTÔMATO"` | — |
| `ESTADOS` | `"ESTADOS"` | — |
| `TRANSICAO` | `"TRANSICAO"\|"TRANSIÇÃO"` | — |
| `COM` | `"COM"` | — |
| `SAIDA` | `"SAIDA"\|"SAÍDA"` | — |
| `INICIAL` | `"INICIAL"` | — |
| `ACEITACAO` | `"ACEITACAO"\|"ACEITAÇÃO"` | — |
| `EPSILON` | `"EPSILON"\|"ε"` | — |
| `TESTAR` | `"TESTAR"` | — |
| `SETA` | `"->"` | — |
| `IDENT` | `[A-Za-z_][A-Za-z0-9_]*` | `std::string*` (nome) |
| `STRING` | `\"([^"\\\n]\|\\.)*\"` | `std::string*` (sem aspas, escapes resolvidos) |
| `SIMBOLO` | `'([^'\\\n]\|\\.)'` | `char` |
| `'(' ')' ',' ';'` | literais | — |
| *(ignorados)* | `[ \t\r\n]+`, `//[^\n]*`, `/* ... */` | — |

Palavras reservadas vêm antes de `IDENT` no arquivo Flex. Quando duas regras
casam o mesmo comprimento, o Flex escolhe a primeira, então `ESTADOS` nunca
vira identificador.

---

## 6. Gramática

Gramática livre de contexto **G = (N, T, P, programa)**, LALR(1), sem
conflitos (o Bison gera o parser sem nenhum aviso de shift/reduce ou
reduce/reduce).

### 6.1 BNF (exatamente as produções de `parser.y`)

```bnf
<programa>            ::= <cabecalho> <lista_comandos> <fim_opt>

<cabecalho>           ::= AUTOMATO STRING ";"

<lista_comandos>      ::= <comando>
                        | <lista_comandos> ";" <comando>

<fim_opt>             ::= ε
                        | ";"

<comando>             ::= <comando_estados>
                        | <comando_transicao>
                        | <comando_saida>
                        | <comando_testar>

<comando_estados>     ::= ESTADOS <lista_estados>
<lista_estados>       ::= <estado>
                        | <lista_estados> "," <estado>
<estado>              ::= IDENT <modificadores_opt>
<modificadores_opt>   ::= ε
                        | "(" <lista_modificadores> ")"
<lista_modificadores> ::= <modificador>
                        | <lista_modificadores> "," <modificador>
<modificador>         ::= INICIAL
                        | ACEITACAO

<comando_transicao>   ::= TRANSICAO IDENT SETA IDENT COM <lista_simbolos>
<lista_simbolos>      ::= <simbolo>
                        | <lista_simbolos> "," <simbolo>
<simbolo>             ::= SIMBOLO
                        | EPSILON

<comando_saida>       ::= SAIDA STRING

<comando_testar>      ::= TESTAR <lista_palavras>
<lista_palavras>      ::= STRING
                        | <lista_palavras> "," STRING
```

O `parser.y` tem ainda três **produções de erro**. Elas não fazem parte da
linguagem; servem para o compilador reportar vários erros numa execução só:

| Produção | Efeito |
|---|---|
| `<cabecalho> ::= error ";"` | Cabeçalho inválido: descarta até o `;` e continua. |
| `<comando> ::= error` | Comando inválido: o Bison descarta tokens até conseguir seguir (em geral, no próximo `;`). |
| `<lista_comandos> ::= <lista_comandos> <comando>` | `;` esquecido entre comandos: o comando é **mantido** e o erro "falta ';' entre os comandos" aponta a posição exata. Não gera conflito porque todo comando começa por palavra reservada. |

Com `%define lr.default-reduction consistent`, o Bison só aplica reduções
padrão em estados sem alternativa de *shift*. Assim, o erro é detectado no
estado em que ocorreu, e a lista de tokens esperados na mensagem fica
exata (ex.: `encontrado simbolo, mas era esperado COM`).

### 6.2 EBNF resumida

```ebnf
programa   = "AUTOMATO" STRING ";" comando { ";" comando } [ ";" ] ;
comando    = estados | transicao | saida | testar ;
estados    = "ESTADOS" estado { "," estado } ;
estado     = IDENT [ "(" modificador { "," modificador } ")" ] ;
modificador= "INICIAL" | "ACEITACAO" ;
transicao  = "TRANSICAO" IDENT "->" IDENT "COM" simbolo { "," simbolo } ;
simbolo    = SIMBOLO | "EPSILON" ;
saida      = "SAIDA" STRING ;
testar     = "TESTAR" STRING { "," STRING } ;
```

A gramática usa recursão à esquerda nas listas, que é o caso eficiente
para um parser LR (pilha de tamanho constante), e põe a palavra reservada no
início de cada comando. Essa escolha mantém a gramática LALR(1) e simplifica
a extensão (seção 8).

---

## 7. Análise semântica: Tradução Dirigida por Sintaxe

A análise semântica acontece **durante a análise sintática**, nas ações
associadas às produções. Todos os atributos são **sintetizados** (definição
S-atribuída), o que se encaixa na avaliação ascendente do Bison: na redução
de `A → X₁…Xₙ`, os atributos `$1…$n` dos filhos já estão na pilha e a ação
calcula `$$`.

### 7.1 Atributos

| Símbolo | Atributo sintetizado |
|---|---|
| `IDENT`, `STRING` | `texto` (lexema) e `loc` (linha/coluna, via `@n`) |
| `SIMBOLO` | `simbolo : char` |
| `modificador`, `lista_modificadores`, `modificadores_opt` | `mods`: conjunto de bits {INICIAL, ACEITACAO} |
| `estado` | `DeclEstado{nome, mods, loc}` |
| `lista_estados` | lista de `DeclEstado` |
| `simbolo` | `Simbolo{valor, loc}` (`valor = ε` para EPSILON) |
| `lista_simbolos` | lista de `Simbolo` |
| `lista_palavras` | lista de `Teste{palavra, loc}` |

### 7.2 Esquema de tradução

| Produção | Ação semântica |
|---|---|
| `modificador → INICIAL` | `modificador.mods = {INICIAL}` |
| `modificador → ACEITACAO` | `modificador.mods = {ACEITACAO}` |
| `lista_modificadores → lista₁ , modificador` | se `lista₁.mods ∩ modificador.mods ≠ ∅` então **aviso** (repetido); `mods = lista₁.mods ∪ modificador.mods` |
| `modificadores_opt → ε` | `mods = ∅` |
| `estado → IDENT modificadores_opt` | `estado.decl = (IDENT.texto, modificadores_opt.mods, @IDENT)` |
| `lista_estados → lista₁ , estado` | `lista = lista₁ ++ [estado.decl]` |
| `comando_estados → ESTADOS lista_estados` | para cada `d` em `lista_estados`: **declararEstado(d)** |
| `simbolo → SIMBOLO` / `EPSILON` | `simbolo.val = SIMBOLO.char` / `ε` |
| `comando_transicao → TRANSICAO IDENT₁ -> IDENT₂ COM lista_simbolos` | **declararTransicao(IDENT₁, IDENT₂, lista_simbolos)** |
| `comando_saida → SAIDA STRING` | **definirSaida(STRING.texto)** |
| `comando_testar → TESTAR lista_palavras` | **adicionarTestes(lista_palavras)** |
| `cabecalho → AUTOMATO STRING ;` | **definirNome(STRING.texto)** |
| `programa → cabecalho lista_comandos fim_opt` | **finalizar()**: verificações globais |

### 7.3 Regras semânticas verificadas

Erros (impedem a geração):

| Regra | Onde |
|---|---|
| Estado declarado duas vezes | `declararEstado` (consulta a tabela de símbolos) |
| Mais de um estado `INICIAL` | `declararEstado` |
| Estado usado em `TRANSICAO` sem ter sido declarado antes (declarar antes de usar) | `declararTransicao` |
| `SAIDA` definida mais de uma vez, ou vazia | `definirSaida` |
| Nome do autômato vazio | `definirNome` |
| Nenhum estado declarado / nenhum estado `INICIAL` | `finalizar` (só roda se não houve erro antes, para evitar erros em cascata) |

Avisos (o SVG é gerado mesmo assim):

| Regra | Onde |
|---|---|
| Modificador repetido, ex. `(INICIAL, INICIAL)` | `lista_modificadores` |
| Transição duplicada (mesma origem, destino e símbolo) | `declararTransicao` |
| Nenhum estado de aceitação | `finalizar` |
| Estado inalcançável a partir do inicial | `finalizar` (BFS) |
| Palavra de `TESTAR` com símbolo fora do alfabeto | `finalizar` |
| `SAIDA` ausente (usa o nome do autômato) ou sem extensão `.svg` | `finalizar` / `definirSaida` |

O alfabeto Σ é **inferido** dos símbolos usados nas transições. O tipo do
autômato também: é **AFD** se toda dupla (estado, símbolo) tiver no máximo
um destino, **AFN** se alguma tiver mais de um, **AFN-ε** se houver
transição vazia. Um AFD é *parcial* se faltar alguma transição.

Exemplo de diagnóstico:

```
testes/invalidos/estado-nao-declarado.aut:4:17: erro: estado de destino 'q2' não foi declarado
    4 | TRANSICAO q0 -> q2 COM 'a';
      |                 ^~
testes/invalidos/estado-nao-declarado.aut:4:17: nota: estados declarados até aqui: q0, q1
```

---

## 8. Como estender a linguagem com novos comandos

Todo comando começa com uma palavra reservada própria e entra como uma
alternativa de `<comando>`. Por isso, um comando novo não cria conflito com
os existentes. O `TESTAR` foi adicionado assim. Um exemplo hipotético,
`ALFABETO 'a', 'b', 'c';`, para declarar o alfabeto explicitamente:

1. **Léxico** ([src/lexer.l](src/lexer.l)), na seção de palavras reservadas:
   ```lex
   "ALFABETO"                  { return TK_ALFABETO; }
   ```
2. **Sintaxe** ([src/parser.y](src/parser.y)): declarar o token e a produção,
   reaproveitando `lista_simbolos`:
   ```yacc
   %token ALFABETO "ALFABETO"
   ...
   comando
       : ...
       | comando_alfabeto
       ;

   comando_alfabeto
       : ALFABETO lista_simbolos
           { sem.declararAlfabeto(*$2); delete $2; }
       ;
   ```
3. **Semântica** ([src/semantico.hpp](src/semantico.hpp) e `.cpp`): criar
   `declararAlfabeto` com as regras (por exemplo, erro se uma transição usar
   símbolo fora do alfabeto declarado) e guardar o dado no `Automato`.
4. **Geração** ([src/gerador_svg.cpp](src/gerador_svg.cpp)), se o comando
   mudar o desenho.
5. **Testes**: um exemplo em `exemplos/` e, para cada erro novo, um arquivo em
   `testes/invalidos/` com `// ESPERA: <trecho da mensagem>` na primeira linha.

Outras extensões que cabem no mesmo molde: `MINIMIZAR;`, `DETERMINIZAR;`
(construção de subconjuntos), `COR q1 "#f00";`, `LAYOUT CIRCULAR;`.

---

## 9. Testes

`testes/executar.ps1` (ou `make testes`) verifica que:

* todos os programas de [exemplos/](exemplos) compilam;
* cada programa de [testes/invalidos/](testes/invalidos) **falha** e mostra a
  mensagem esperada. São 16 casos que cobrem erros léxicos, sintáticos e
  semânticos, incluindo vários erros num mesmo arquivo.
