/*
 * Analisador sintático (Bison, LALR(1)) da DSL de autômatos, com as ações
 * semânticas da Tradução Dirigida por Sintaxe (TDS).
 *
 * Cada não-terminal sintetiza um atributo ($$) a partir dos atributos dos
 * filhos ($1, $2, ...). As produções de comando entregam esses atributos ao
 * AnalisadorSemantico (semantico.hpp), que valida e monta o autômato.
 *
 * Gramática (EBNF resumida; a versão completa está no README.md):
 *
 *   programa        -> cabecalho lista_comandos [ ';' ]
 *   cabecalho       -> AUTOMATO STRING ';'
 *   lista_comandos  -> comando { ';' comando }
 *   comando         -> estados | transicao | saida | testar
 *   estados         -> ESTADOS estado { ',' estado }
 *   estado          -> IDENT [ '(' modificador { ',' modificador } ')' ]
 *   modificador     -> INICIAL | ACEITACAO
 *   transicao       -> TRANSICAO IDENT '->' IDENT COM simbolo { ',' simbolo }
 *   simbolo         -> SIMBOLO | EPSILON
 *   saida           -> SAIDA STRING
 *   testar          -> TESTAR STRING { ',' STRING }
 *
 * Para adicionar um comando novo:
 *   1. lexer.l  : regra para a palavra reservada  -> return TK_NOVO;
 *   2. parser.y : %token NOVO "NOVO"  e uma alternativa em `comando`;
 *   3. semantico: um método com a ação semântica, chamado pela produção.
 */

%code requires {
#include <string>
#include <vector>

#include "semantico.hpp"
}

%code {
#include "diagnostico.hpp"

int yylex(void);
void yyerror(dsl::AnalisadorSemantico& sem, const char* msg);

static dsl::Local L(const YYLTYPE& l) {
    int comprimento = l.first_line == l.last_line ? l.last_column - l.first_column + 1 : 1;
    return dsl::Local{l.first_line, l.first_column, comprimento > 0 ? comprimento : 1};
}
}

%define api.token.prefix {TK_}
%define parse.error verbose
/* Só reduz por padrão em estados sem alternativa de shift: assim o erro é
   detectado no estado certo e a lista de tokens esperados fica precisa. */
%define lr.default-reduction consistent
%locations
%parse-param { dsl::AnalisadorSemantico& sem }

%union {
    std::string*                   texto;
    char                           simbolo;
    unsigned                       mods;
    dsl::Simbolo                   simb;
    dsl::DeclEstado*               estado;
    std::vector<dsl::DeclEstado>*  estados;
    std::vector<dsl::Simbolo>*     simbolos;
    std::vector<dsl::Teste>*       palavras;
}

/* ------------------------------ Tokens ------------------------------ */
%token FIM 0          "fim do arquivo"
%token AUTOMATO       "AUTOMATO"
%token ESTADOS        "ESTADOS"
%token TRANSICAO      "TRANSICAO"
%token COM            "COM"
%token SAIDA          "SAIDA"
%token INICIAL        "INICIAL"
%token ACEITACAO      "ACEITACAO"
%token EPSILON        "EPSILON"
%token TESTAR         "TESTAR"
%token SETA           "->"
%token <texto>   IDENT    "identificador"
%token <texto>   STRING   "texto entre aspas"
%token <simbolo> SIMBOLO  "simbolo"

/* ------------------ Atributos sintetizados (tipos) ------------------ */
%type <estado>   estado
%type <estados>  lista_estados
%type <mods>     modificadores_opt lista_modificadores modificador comando
%type <simb>     simbolo
%type <simbolos> lista_simbolos
%type <palavras> lista_palavras

/* Libera memória de atributos descartados durante a recuperação de erros */
%destructor { delete $$; } <texto> <estado> <estados> <simbolos> <palavras>

%start programa

%%

programa
    : cabecalho lista_comandos fim_opt
        { sem.finalizar(); }
    ;

cabecalho
    : AUTOMATO STRING ';'
        { sem.definirNome(*$2, L(@2)); delete $2; }
    | error ';'
        { yyerrok; }
    ;

lista_comandos
    : comando
    | lista_comandos ';' comando
    | lista_comandos comando
        /* Produção de erro: ';' esquecido entre dois comandos. Como todo
           comando começa por palavra reservada, aceitar a ausência não gera
           conflito; o comando é mantido e o erro é apontado com precisão. */
        {
            if ($2) {
                dsl::Local depois{@1.last_line, @1.last_column + 1, 1};
                dsl::diag().erro(depois, "falta ';' entre os comandos");
            }
        }
    ;

fim_opt
    : %empty
    | ';'
    ;

/* Ponto de extensão: novos comandos entram como novas alternativas aqui. */
comando                             /* atributo: 1 = comando válido, 0 = erro */
    : comando_estados               { $$ = 1; }
    | comando_transicao             { $$ = 1; }
    | comando_saida                 { $$ = 1; }
    | comando_testar                { $$ = 1; }
    | error                         { $$ = 0; }
        /* Recuperação: o Bison descarta tokens até conseguir continuar
           (normalmente no próximo ';'). Sem yyerrok aqui: como a regra não
           termina num token de sincronização, yyerrok faria o parser
           reportar o mesmo token para sempre sem descartá-lo. */
    ;

/* ESTADOS q0 (INICIAL), q1 (ACEITACAO) */
comando_estados
    : ESTADOS lista_estados
        { sem.declararEstados(*$2); delete $2; }
    ;

lista_estados
    : estado
        { $$ = new std::vector<dsl::DeclEstado>{ std::move(*$1) }; delete $1; }
    | lista_estados ',' estado
        { $1->push_back(std::move(*$3)); delete $3; $$ = $1; }
    ;

estado
    : IDENT modificadores_opt
        { $$ = new dsl::DeclEstado{ std::move(*$1), $2, L(@1) }; delete $1; }
    ;

modificadores_opt
    : %empty                            { $$ = dsl::MOD_NENHUM; }
    | '(' lista_modificadores ')'       { $$ = $2; }
    ;

lista_modificadores
    : modificador                       { $$ = $1; }
    | lista_modificadores ',' modificador
        {
            if ($1 & $3) sem.modificadorRepetido($3, L(@3));
            $$ = $1 | $3;
        }
    ;

modificador
    : INICIAL                           { $$ = dsl::MOD_INICIAL; }
    | ACEITACAO                         { $$ = dsl::MOD_ACEITACAO; }
    ;

/* TRANSICAO q0 -> q1 COM 'a', 'b' */
comando_transicao
    : TRANSICAO IDENT SETA IDENT COM lista_simbolos
        {
            sem.declararTransicao(*$2, L(@2), *$4, L(@4), *$6);
            delete $2; delete $4; delete $6;
        }
    ;

lista_simbolos
    : simbolo
        { $$ = new std::vector<dsl::Simbolo>{ $1 }; }
    | lista_simbolos ',' simbolo
        { $1->push_back($3); $$ = $1; }
    ;

simbolo
    : SIMBOLO                           { $$ = dsl::Simbolo{ $1, L(@1) }; }
    | EPSILON                           { $$ = dsl::Simbolo{ dsl::EPS, L(@1) }; }
    ;

/* SAIDA "automato.svg" */
comando_saida
    : SAIDA STRING
        { sem.definirSaida(*$2, L(@2)); delete $2; }
    ;

/* TESTAR "ab", "abb"   (extensão: simula o autômato sobre cada palavra) */
comando_testar
    : TESTAR lista_palavras
        { sem.adicionarTestes(*$2); delete $2; }
    ;

lista_palavras
    : STRING
        { $$ = new std::vector<dsl::Teste>{ dsl::Teste{ std::move(*$1), L(@1) } }; delete $1; }
    | lista_palavras ',' STRING
        { $1->push_back(dsl::Teste{ std::move(*$3), L(@3) }); delete $3; $$ = $1; }
    ;

%%

/* Traduz a mensagem padrão do Bison
 *   "syntax error, unexpected X, expecting A or B"
 * para
 *   "erro de sintaxe: encontrado X, mas era esperado A ou B"            */
void yyerror(dsl::AnalisadorSemantico&, const char* msg) {
    std::string m = msg;
    std::string saida;

    const std::string prefixo = "syntax error, unexpected ";
    const std::string esperando = ", expecting ";
    if (m.compare(0, prefixo.size(), prefixo) == 0) {
        std::string resto = m.substr(prefixo.size());
        std::string encontrado = resto, esperados;
        size_t p = resto.find(esperando);
        if (p != std::string::npos) {
            encontrado = resto.substr(0, p);
            esperados = resto.substr(p + esperando.size());
            for (size_t q; (q = esperados.find(" or ")) != std::string::npos;)
                esperados.replace(q, 4, " ou ");
        }
        saida = "erro de sintaxe: encontrado " + encontrado;
        if (!esperados.empty()) saida += ", mas era esperado " + esperados;
    } else if (m == "syntax error") {
        saida = "erro de sintaxe";
    } else if (m == "memory exhausted") {
        saida = "memória esgotada durante a análise sintática";
    } else {
        saida = m;
    }
    dsl::diag().erro(L(yylloc), saida);
}
