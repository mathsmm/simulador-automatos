// Diagnósticos (erros, avisos e notas) emitidos por todas as fases do
// compilador, no formato  arquivo:linha:coluna: tipo: mensagem
// seguido do trecho do código-fonte com a posição sublinhada.
#pragma once

#include <string>
#include <vector>

namespace dsl {

// Posição no código-fonte. Agregado simples (sem construtores) para poder
// ser usado dentro do %union do Bison.
struct Local {
    int linha;        // 1-based; 0 = sem posição
    int coluna;       // 1-based
    int comprimento;  // nº de caracteres a sublinhar
};

class Diagnosticos {
public:
    void carregarFonte(const std::string& arquivo, const std::string& conteudo);
    void usarCores(bool sim) { cores_ = sim; }

    void erro(const Local& loc, const std::string& msg);
    void aviso(const Local& loc, const std::string& msg);
    void nota(const Local& loc, const std::string& msg);

    int erros() const { return erros_; }
    int avisos() const { return avisos_; }

private:
    void emitir(const char* tipo, const char* cor, const Local& loc,
                const std::string& msg);

    std::string arquivo_;
    std::vector<std::string> linhas_;
    Local ultimo_{0, 0, 0};
    bool cores_ = false;
    int erros_ = 0;
    int avisos_ = 0;
};

// Instância única, compartilhada pelo léxico (Flex), sintático (Bison) e
// semântico.
Diagnosticos& diag();

}  // namespace dsl
