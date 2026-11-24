#include "semantico.hpp"

#include <cctype>

namespace dsl {

namespace {

std::string aspas(const std::string& s) { return "'" + s + "'"; }

bool terminaCom(const std::string& s, const std::string& sufixo) {
    if (s.size() < sufixo.size()) return false;
    for (size_t i = 0; i < sufixo.size(); ++i) {
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[s.size() - sufixo.size() + i])));
        if (a != sufixo[i]) return false;
    }
    return true;
}

// "Meu Autômato 1" -> "meu-aut-mato-1.svg" (só ASCII, para nome de arquivo)
std::string nomeArquivoPadrao(const std::string& nome) {
    std::string s;
    for (unsigned char c : nome) {
        if (std::isalnum(c)) s += static_cast<char>(std::tolower(c));
        else if (!s.empty() && s.back() != '-') s += '-';
    }
    while (!s.empty() && s.back() == '-') s.pop_back();
    return (s.empty() ? "automato" : s) + ".svg";
}

}  // namespace

AnalisadorSemantico::AnalisadorSemantico(Automato& automato, Diagnosticos& diagnosticos)
    : a_(automato), d_(diagnosticos) {}

void AnalisadorSemantico::definirNome(const std::string& nome, const Local& loc) {
    locNome_ = loc;
    if (nome.empty()) d_.erro(loc, "o nome do autômato não pode ser vazio");
    a_.nome = nome;
}

void AnalisadorSemantico::declararEstados(const std::vector<DeclEstado>& estados) {
    for (const auto& e : estados) declararEstado(e);
}

void AnalisadorSemantico::declararEstado(const DeclEstado& e) {
    int anterior = a_.buscarEstado(e.nome);
    if (anterior >= 0) {
        d_.erro(e.loc, "estado " + aspas(e.nome) + " já foi declarado");
        d_.nota(a_.estados[anterior].loc, "a declaração anterior está aqui");
        return;
    }

    Estado novo{e.nome, (e.modificadores & MOD_INICIAL) != 0,
                (e.modificadores & MOD_ACEITACAO) != 0, e.loc};

    if (novo.inicial && a_.inicial >= 0) {
        d_.erro(e.loc, "apenas um estado pode ser INICIAL, mas " + aspas(e.nome) +
                           " também foi marcado");
        d_.nota(a_.estados[a_.inicial].loc,
                "o estado inicial " + aspas(a_.estados[a_.inicial].nome) + " foi declarado aqui");
        novo.inicial = false;
    }

    int indice = a_.adicionarEstado(novo);
    if (novo.inicial) a_.inicial = indice;
}

void AnalisadorSemantico::modificadorRepetido(unsigned modificador, const Local& loc) {
    d_.aviso(loc, std::string("modificador ") +
                      (modificador == MOD_INICIAL ? "INICIAL" : "ACEITACAO") +
                      " repetido no mesmo estado");
}

std::string AnalisadorSemantico::nomesDeclarados() const {
    std::string s;
    for (const auto& e : a_.estados) s += (s.empty() ? "" : ", ") + e.nome;
    return s;
}

int AnalisadorSemantico::resolverEstado(const std::string& nome, const Local& loc,
                                        const char* papel) {
    int indice = a_.buscarEstado(nome);
    if (indice < 0) {
        d_.erro(loc, std::string("estado de ") + papel + " " + aspas(nome) +
                         " não foi declarado");
        if (a_.estados.empty())
            d_.nota(loc, "declare os estados com ESTADOS antes de usá-los em TRANSICAO");
        else
            d_.nota(loc, "estados declarados até aqui: " + nomesDeclarados());
    }
    return indice;
}

void AnalisadorSemantico::declararTransicao(const std::string& origem,
                                            const Local& locOrigem,
                                            const std::string& destino,
                                            const Local& locDestino,
                                            const std::vector<Simbolo>& simbolos) {
    int o = resolverEstado(origem, locOrigem, "origem");
    int d = resolverEstado(destino, locDestino, "destino");
    if (o < 0 || d < 0) return;

    for (const auto& s : simbolos) {
        if (a_.existeTransicao(o, d, s.valor)) {
            d_.aviso(s.loc, "transição duplicada ignorada: " + origem + " -> " +
                                destino + " com " + aspas(descreverSimbolo(s.valor)));
            continue;
        }
        a_.transicoes.push_back(Transicao{o, d, s.valor, s.loc});
    }
}

void AnalisadorSemantico::definirSaida(const std::string& arquivo, const Local& loc) {
    if (saidaDefinida_) {
        d_.erro(loc, "SAIDA já foi definida para este autômato");
        d_.nota(locSaida_, "a definição anterior está aqui");
        return;
    }
    saidaDefinida_ = true;
    locSaida_ = loc;
    if (arquivo.empty()) {
        d_.erro(loc, "o nome do arquivo de SAIDA não pode ser vazio");
        return;
    }
    if (!terminaCom(arquivo, ".svg"))
        d_.aviso(loc, "o arquivo de saída é gerado em formato SVG; considere usar a extensão .svg");
    a_.saida = arquivo;
}

void AnalisadorSemantico::adicionarTestes(const std::vector<Teste>& testes) {
    for (const auto& t : testes) a_.testes.push_back(t);
}

void AnalisadorSemantico::finalizar() {
    if (finalizado_) return;  // a recuperação de erros pode reduzir `programa` de novo
    finalizado_ = true;

    // Com erros anteriores, o SVG não será gerado e estas verificações globais
    // costumam ser só consequência do erro (ex.: um ESTADOS descartado pela
    // recuperação sintática geraria "nenhum estado foi declarado").
    if (d_.erros() > 0) return;

    if (a_.estados.empty()) {
        d_.erro(locNome_, "nenhum estado foi declarado (use ESTADOS)");
        return;
    }
    if (a_.inicial < 0)
        d_.erro(locNome_, "nenhum estado foi marcado como INICIAL");

    if (d_.erros() > 0) return;

    bool temAceitacao = false;
    for (const auto& e : a_.estados) temAceitacao = temAceitacao || e.aceitacao;
    if (!temAceitacao)
        d_.aviso(locNome_, "nenhum estado de ACEITACAO: o autômato não reconhece nenhuma palavra");

    if (a_.inicial >= 0) {
        std::vector<bool> alcancavel = a_.alcancaveis();
        for (size_t i = 0; i < a_.estados.size(); ++i)
            if (!alcancavel[i])
                d_.aviso(a_.estados[i].loc, "estado " + aspas(a_.estados[i].nome) +
                                                " é inalcançável a partir do estado inicial");
    }

    std::set<char> sigma = a_.alfabeto();
    for (const auto& t : a_.testes) {
        for (char c : t.palavra) {
            if (!sigma.count(c)) {
                d_.aviso(t.loc, "a palavra contém " + aspas(descreverSimbolo(c)) +
                                    ", que não pertence ao alfabeto " + a_.descreverAlfabeto() +
                                    " (será rejeitada)");
                break;
            }
        }
    }

    if (!saidaDefinida_) {
        a_.saida = nomeArquivoPadrao(a_.nome);
        d_.aviso(locNome_, "SAIDA não especificada; o diagrama será gravado em " + aspas(a_.saida));
    }
}

}  // namespace dsl
