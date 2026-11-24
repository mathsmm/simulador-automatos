#include "diagnostico.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>

namespace dsl {

Diagnosticos& diag() {
    static Diagnosticos instancia;
    return instancia;
}

void Diagnosticos::carregarFonte(const std::string& arquivo,
                                 const std::string& conteudo) {
    arquivo_ = arquivo;
    linhas_.clear();
    std::istringstream in(conteudo);
    std::string linha;
    while (std::getline(in, linha)) {
        if (!linha.empty() && linha.back() == '\r') linha.pop_back();
        linhas_.push_back(linha);
    }
}

void Diagnosticos::erro(const Local& loc, const std::string& msg) {
    ++erros_;
    emitir("erro", "\033[31m", loc, msg);
}

void Diagnosticos::aviso(const Local& loc, const std::string& msg) {
    ++avisos_;
    emitir("aviso", "\033[33m", loc, msg);
}

void Diagnosticos::nota(const Local& loc, const std::string& msg) {
    emitir("nota", "\033[36m", loc, msg);
}

void Diagnosticos::emitir(const char* tipo, const char* cor, const Local& loc,
                          const std::string& msg) {
    std::cout.flush();
    std::ostream& out = std::cerr;
    const char* negrito = cores_ ? "\033[1m" : "";
    const char* reset = cores_ ? "\033[0m" : "";
    const char* c = cores_ ? cor : "";

    out << negrito << arquivo_;
    if (loc.linha > 0) out << ':' << loc.linha << ':' << loc.coluna;
    out << ": " << reset << c << negrito << tipo << ": " << reset << negrito
        << msg << reset << '\n';

    // Não repete o trecho se a posição é a mesma do diagnóstico anterior.
    const bool repetido = loc.linha == ultimo_.linha && loc.coluna == ultimo_.coluna;
    ultimo_ = loc;
    if (repetido || loc.linha <= 0 || loc.linha > static_cast<int>(linhas_.size())) return;

    const std::string& texto = linhas_[loc.linha - 1];
    char numero[16];
    std::snprintf(numero, sizeof numero, "%5d", loc.linha);
    out << numero << " | " << texto << '\n' << "      | ";
    for (int i = 0; i < loc.coluna - 1 && i < static_cast<int>(texto.size()); ++i) {
        unsigned char ch = static_cast<unsigned char>(texto[i]);
        if (ch >= 0x80 && ch < 0xC0) continue;  // byte de continuação UTF-8
        out << (ch == '\t' ? '\t' : ' ');
    }
    out << c << '^';
    for (int i = 1; i < loc.comprimento; ++i) out << '~';
    out << reset << '\n';
}

}  // namespace dsl
