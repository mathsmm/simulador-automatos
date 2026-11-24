#include "automato.hpp"

#include <queue>
#include <utility>

namespace dsl {

std::string descreverSimbolo(char c) {
    switch (c) {
        case EPS:  return "ε";
        case ' ':  return "␣";
        case '\n': return "\\n";
        case '\t': return "\\t";
        default:   return std::string(1, c);
    }
}

int Automato::adicionarEstado(const Estado& e) {
    int indice = static_cast<int>(estados.size());
    estados.push_back(e);
    tabela_[e.nome] = indice;
    return indice;
}

int Automato::buscarEstado(const std::string& nomeEstado) const {
    auto it = tabela_.find(nomeEstado);
    return it == tabela_.end() ? -1 : it->second;
}

bool Automato::existeTransicao(int origem, int destino, char simbolo) const {
    for (const auto& t : transicoes)
        if (t.origem == origem && t.destino == destino && t.simbolo == simbolo)
            return true;
    return false;
}

std::set<char> Automato::alfabeto() const {
    std::set<char> sigma;
    for (const auto& t : transicoes)
        if (t.simbolo != EPS) sigma.insert(t.simbolo);
    return sigma;
}

TipoAutomato Automato::tipo() const {
    std::set<std::pair<int, char>> vistos;
    bool naoDeterministico = false;
    for (const auto& t : transicoes) {
        if (t.simbolo == EPS) return TipoAutomato::AFN_EPSILON;
        if (!vistos.insert({t.origem, t.simbolo}).second) naoDeterministico = true;
    }
    return naoDeterministico ? TipoAutomato::AFN : TipoAutomato::AFD;
}

bool Automato::completo() const {
    std::set<std::pair<int, char>> definidas;
    for (const auto& t : transicoes) definidas.insert({t.origem, t.simbolo});
    for (int q = 0; q < static_cast<int>(estados.size()); ++q)
        for (char c : alfabeto())
            if (!definidas.count({q, c})) return false;
    return true;
}

std::vector<bool> Automato::alcancaveis() const {
    std::vector<bool> visitado(estados.size(), false);
    if (inicial < 0) return visitado;
    std::queue<int> fila;
    visitado[inicial] = true;
    fila.push(inicial);
    while (!fila.empty()) {
        int u = fila.front();
        fila.pop();
        for (const auto& t : transicoes) {
            if (t.origem == u && !visitado[t.destino]) {
                visitado[t.destino] = true;
                fila.push(t.destino);
            }
        }
    }
    return visitado;
}

std::set<int> Automato::fechoEpsilon(std::set<int> conjunto) const {
    std::vector<int> pilha(conjunto.begin(), conjunto.end());
    while (!pilha.empty()) {
        int u = pilha.back();
        pilha.pop_back();
        for (const auto& t : transicoes)
            if (t.origem == u && t.simbolo == EPS && conjunto.insert(t.destino).second)
                pilha.push_back(t.destino);
    }
    return conjunto;
}

ResultadoTeste Automato::simular(const std::string& palavra) const {
    ResultadoTeste r{palavra, false, {}};
    std::set<int> atual;
    if (inicial >= 0) atual = fechoEpsilon({inicial});
    r.trilha.push_back(atual);

    for (char c : palavra) {
        std::set<int> proximo;
        for (int q : atual)
            for (const auto& t : transicoes)
                if (t.origem == q && t.simbolo == c) proximo.insert(t.destino);
        atual = fechoEpsilon(proximo);
        r.trilha.push_back(atual);
    }

    for (int q : atual)
        if (estados[q].aceitacao) r.aceita = true;
    return r;
}

std::string Automato::descreverTipo() const {
    switch (tipo()) {
        case TipoAutomato::AFD:         return completo() ? "AFD" : "AFD (parcial)";
        case TipoAutomato::AFN:         return "AFN";
        case TipoAutomato::AFN_EPSILON: return "AFN-ε";
    }
    return "?";
}

std::string Automato::descreverAlfabeto() const {
    std::string s = "{";
    bool primeiro = true;
    for (char c : alfabeto()) {
        if (!primeiro) s += ", ";
        s += descreverSimbolo(c);
        primeiro = false;
    }
    return s + "}";
}

std::string Automato::descreverConjunto(const std::set<int>& conjunto) const {
    if (conjunto.empty()) return "∅";
    if (conjunto.size() == 1 && tipo() == TipoAutomato::AFD)
        return estados[*conjunto.begin()].nome;
    std::string s = "{";
    bool primeiro = true;
    for (int q : conjunto) {
        if (!primeiro) s += ",";
        s += estados[q].nome;
        primeiro = false;
    }
    return s + "}";
}

std::string Automato::descreverTrilha(const ResultadoTeste& r) const {
    std::string s = descreverConjunto(r.trilha[0]);
    for (size_t i = 0; i < r.palavra.size(); ++i) {
        s += " -" + descreverSimbolo(r.palavra[i]) + "-> ";
        s += descreverConjunto(r.trilha[i + 1]);
        if (r.trilha[i + 1].empty()) {  // morreu: o resto não muda nada
            if (i + 1 < r.palavra.size()) s += " (para)";
            break;
        }
    }
    return s;
}

}  // namespace dsl
