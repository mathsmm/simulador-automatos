// Representação intermediária (RI) do programa: o autômato finito montado
// pelas ações semânticas durante a análise sintática. A tabela de símbolos
// dos estados fica aqui dentro.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "diagnostico.hpp"

namespace dsl {

// Símbolo usado para representar a transição vazia (EPSILON).
constexpr char EPS = '\0';

struct Estado {
    std::string nome;
    bool inicial;
    bool aceitacao;
    Local loc;
};

struct Transicao {
    int origem;
    int destino;
    char simbolo;  // EPS para transição vazia
    Local loc;
};

struct Teste {
    std::string palavra;
    Local loc;
};

enum class TipoAutomato { AFD, AFN, AFN_EPSILON };

struct ResultadoTeste {
    std::string palavra;
    bool aceita;
    // trilha[0] = estados iniciais; trilha[i] = estados após ler o i-ésimo símbolo
    std::vector<std::set<int>> trilha;
};

class Automato {
public:
    std::string nome;
    std::vector<Estado> estados;
    std::vector<Transicao> transicoes;
    std::vector<Teste> testes;
    std::string saida;
    int inicial = -1;

    // Tabela de símbolos
    int adicionarEstado(const Estado& e);
    int buscarEstado(const std::string& nome) const;  // -1 se ausente

    bool existeTransicao(int origem, int destino, char simbolo) const;
    std::set<char> alfabeto() const;  // sem EPSILON
    TipoAutomato tipo() const;
    bool completo() const;            // toda (estado, símbolo) tem transição?
    std::vector<bool> alcancaveis() const;

    // Simulação (funciona para AFD, AFN e AFN-ε via conjunto de estados).
    ResultadoTeste simular(const std::string& palavra) const;

    std::string descreverTipo() const;
    std::string descreverAlfabeto() const;
    std::string descreverTrilha(const ResultadoTeste& r) const;

private:
    std::set<int> fechoEpsilon(std::set<int> estados) const;
    std::string descreverConjunto(const std::set<int>& conjunto) const;

    std::map<std::string, int> tabela_;  // nome do estado -> índice
};

std::string descreverSimbolo(char c);

}  // namespace dsl
