// Análise semântica por Tradução Dirigida por Sintaxe (TDS).
//
// Cada método abaixo é a ação semântica de uma produção da gramática
// (ver parser.y). As ações recebem os atributos sintetizados dos filhos
// (nomes, modificadores, listas de símbolos, posições), consultam/atualizam a
// tabela de símbolos e montam a representação intermediária (Automato),
// emitindo erros semânticos no momento da redução.
#pragma once

#include <string>
#include <vector>

#include "automato.hpp"
#include "diagnostico.hpp"

namespace dsl {

// Atributo sintetizado de  modificador / lista_modificadores  (conjunto de bits)
enum Modificador : unsigned {
    MOD_NENHUM = 0u,
    MOD_INICIAL = 1u << 0,
    MOD_ACEITACAO = 1u << 1,
};

// Atributo sintetizado de  estado : IDENT modificadores_opt
struct DeclEstado {
    std::string nome;
    unsigned modificadores;
    Local loc;
};

// Atributo sintetizado de  simbolo : SIMBOLO | EPSILON
struct Simbolo {
    char valor;  // EPS para EPSILON
    Local loc;
};

class AnalisadorSemantico {
public:
    AnalisadorSemantico(Automato& automato, Diagnosticos& diagnosticos);

    // cabecalho        : AUTOMATO STRING ';'
    void definirNome(const std::string& nome, const Local& loc);
    // comando_estados  : ESTADOS lista_estados
    void declararEstados(const std::vector<DeclEstado>& estados);
    // lista_modificadores : lista_modificadores ',' modificador
    void modificadorRepetido(unsigned modificador, const Local& loc);
    // comando_transicao: TRANSICAO IDENT SETA IDENT COM lista_simbolos
    void declararTransicao(const std::string& origem, const Local& locOrigem,
                           const std::string& destino, const Local& locDestino,
                           const std::vector<Simbolo>& simbolos);
    // comando_saida    : SAIDA STRING
    void definirSaida(const std::string& arquivo, const Local& loc);
    // comando_testar   : TESTAR lista_palavras
    void adicionarTestes(const std::vector<Teste>& testes);
    // programa         : cabecalho lista_comandos fim_opt   (verificações globais)
    void finalizar();

private:
    void declararEstado(const DeclEstado& e);
    int resolverEstado(const std::string& nome, const Local& loc, const char* papel);
    std::string nomesDeclarados() const;

    Automato& a_;
    Diagnosticos& d_;
    Local locNome_{0, 0, 0};
    Local locSaida_{0, 0, 0};
    bool saidaDefinida_ = false;
    bool finalizado_ = false;
};

}  // namespace dsl
