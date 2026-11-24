/*
 * Analisador léxico (Flex) da DSL de autômatos.
 *
 * Converte o texto-fonte em tokens para o Bison, preenchendo yylval (valor
 * semântico) e yylloc (linha/coluna). Erros léxicos são reportados e a
 * análise continua, para que o usuário veja vários erros de uma vez.
 *
 * Para adicionar uma palavra reservada: inclua uma regra na seção
 * "Palavras reservadas" retornando TK_<NOME> e declare %token <NOME> no
 * parser.y.
 */

%option noyywrap nounput noinput never-interactive nounistd
%x COMENTARIO

%{
#include <string>

#include "diagnostico.hpp"
#include "parser.hpp"

using dsl::diag;

static int linha = 1;
static int coluna = 1;
static dsl::Local inicioComentario{0, 0, 0};

#define YY_USER_ACTION                                  \
    yylloc.first_line = yylloc.last_line = linha;       \
    yylloc.first_column = coluna;                       \
    yylloc.last_column = coluna + (int)yyleng - 1;      \
    coluna += (int)yyleng;

static dsl::Local localAtual() {
    return dsl::Local{yylloc.first_line, yylloc.first_column, (int)yyleng};
}

static char escape(char c) {
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        default:  return c;  /* \\  \'  \"  e demais: o próprio caractere */
    }
}

static std::string processarEscapes(const char* s, size_t n) {
    std::string r;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == '\\' && i + 1 < n) r += escape(s[++i]);
        else r += s[i];
    }
    return r;
}
%}

IDENT       [A-Za-z_][A-Za-z0-9_]*
STRING      \"([^"\\\n]|\\.)*\"
SIMBOLO     '([^'\\\n]|\\.)'
UTF8        [\xC0-\xFF][\x80-\xBF]*

%%

 /* ---------------- Comentários e espaços ---------------- */
"//"[^\n]*                  { /* comentário de linha */ }
"/*"                        { inicioComentario = localAtual(); BEGIN(COMENTARIO); }
<COMENTARIO>"*/"            { BEGIN(INITIAL); }
<COMENTARIO>[^*\n]+         { }
<COMENTARIO>"*"             { }
<COMENTARIO>\n              { ++linha; coluna = 1; }
<COMENTARIO><<EOF>>         {
                                diag().erro(inicioComentario, "comentário de bloco não foi fechado (falta '*/')");
                                yyterminate();
                            }
[ \t\r]+                    { }
\n                          { ++linha; coluna = 1; }

 /* ---------------- Palavras reservadas ---------------- */
"AUTOMATO"|"AUTÔMATO"       { return TK_AUTOMATO; }
"ESTADOS"                   { return TK_ESTADOS; }
"TRANSICAO"|"TRANSIÇÃO"     { return TK_TRANSICAO; }
"COM"                       { return TK_COM; }
"SAIDA"|"SAÍDA"             { return TK_SAIDA; }
"INICIAL"                   { return TK_INICIAL; }
"ACEITACAO"|"ACEITAÇÃO"     { return TK_ACEITACAO; }
"EPSILON"|"ε"               { return TK_EPSILON; }
"TESTAR"                    { return TK_TESTAR; }

 /* ---------------- Operadores e pontuação ---------------- */
"->"                        { return TK_SETA; }
"("|")"|","|";"             { return yytext[0]; }

 /* ---------------- Identificadores e literais ---------------- */
{IDENT}                     { yylval.texto = new std::string(yytext, yyleng); return TK_IDENT; }

{STRING}                    {
                                yylval.texto = new std::string(processarEscapes(yytext + 1, yyleng - 2));
                                return TK_STRING;
                            }
\"([^"\\\n]|\\.)*           {
                                diag().erro(localAtual(), "texto entre aspas não foi fechado (falta '\"')");
                                yylval.texto = new std::string(processarEscapes(yytext + 1, yyleng - 1));
                                return TK_STRING;
                            }

{SIMBOLO}                   {
                                yylval.simbolo = (yytext[1] == '\\') ? escape(yytext[2]) : yytext[1];
                                return TK_SIMBOLO;
                            }
"''"                        {
                                diag().erro(localAtual(), "símbolo vazio; use EPSILON para transições vazias");
                                yylval.simbolo = '?';
                                return TK_SIMBOLO;
                            }
'[^'\n]+'                   {
                                diag().erro(localAtual(), std::string("símbolo ") + yytext +
                                            " inválido: um símbolo deve ter exatamente um caractere ASCII");
                                yylval.simbolo = yytext[1];
                                return TK_SIMBOLO;
                            }
'[^'\n]*                    {
                                diag().erro(localAtual(), "símbolo não foi fechado (falta \"'\")");
                                yylval.simbolo = yyleng > 1 ? yytext[1] : '?';
                                return TK_SIMBOLO;
                            }

 /* ---------------- Qualquer outra coisa é erro léxico ---------------- */
{UTF8}                      {
                                diag().erro(localAtual(), std::string("caractere inesperado '") + yytext + "'");
                            }
.                           {
                                diag().erro(localAtual(), std::string("caractere inesperado '") + yytext + "'");
                            }

%%
