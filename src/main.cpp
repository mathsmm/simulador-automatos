// automatoc — compilador da DSL de autômatos finitos.
//
// Pipeline de um passo (one-step):
//   1. análise léxica (Flex) + sintática (Bison) + semântica (TDS), num único
//      percurso sobre o código-fonte;
//   2. execução dos comandos TESTAR (simulação do autômato);
//   3. geração de código: tradução para SVG;
//   4. execução: o SVG é aberto no visualizador padrão do sistema.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "automato.hpp"
#include "diagnostico.hpp"
#include "gerador_svg.hpp"
#include "plataforma.hpp"
#include "semantico.hpp"
#include "parser.hpp"

extern FILE* yyin;

namespace fs = std::filesystem;

namespace {

const char* const VERSAO = "1.0.0";

struct Opcoes {
    std::string entrada;
    std::string saida;  // -o: sobrescreve SAIDA
    bool abrir = true;
};

void mostrarUso(std::ostream& out) {
    out << "Uso: automatoc <arquivo.aut> [opções]\n"
           "\n"
           "Compila e executa uma especificação de autômato finito: valida o código,\n"
           "simula as palavras de TESTAR, gera o diagrama SVG indicado em SAIDA e o\n"
           "abre no visualizador padrão do sistema.\n"
           "\n"
           "Opções:\n"
           "  -o, --saida <arquivo>  grava o SVG neste caminho (ignora SAIDA)\n"
           "      --nao-abrir        não abre o SVG ao final\n"
           "  -h, --ajuda            mostra esta ajuda\n"
           "  -v, --versao           mostra a versão\n";
}

// Retorna -1 para continuar, ou o código de saída do programa.
int lerOpcoes(int argc, char** argv, Opcoes& op) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--ajuda" || arg == "--help") {
            mostrarUso(std::cout);
            return 0;
        } else if (arg == "-v" || arg == "--versao" || arg == "--version") {
            std::cout << "automatoc " << VERSAO << '\n';
            return 0;
        } else if (arg == "--nao-abrir" || arg == "--no-open") {
            op.abrir = false;
        } else if (arg == "-o" || arg == "--saida") {
            if (i + 1 >= argc) {
                std::cerr << "automatoc: a opção " << arg << " exige um arquivo\n";
                return 2;
            }
            op.saida = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "automatoc: opção desconhecida '" << arg << "'\n\n";
            mostrarUso(std::cerr);
            return 2;
        } else if (op.entrada.empty()) {
            op.entrada = arg;
        } else {
            std::cerr << "automatoc: apenas um arquivo de entrada é aceito\n";
            return 2;
        }
    }
    if (op.entrada.empty()) {
        mostrarUso(std::cerr);
        return 2;
    }
    return -1;
}

std::string plural(int n, const char* singular, const char* pluralForma) {
    return std::to_string(n) + " " + (n == 1 ? singular : pluralForma);
}

}  // namespace

int main(int argc, char** argv) {
    dsl::configurarConsole();

    Opcoes op;
    int codigo = lerOpcoes(argc, argv, op);
    if (codigo >= 0) return codigo;

    const bool cores = dsl::terminalInterativo(stdout) && dsl::terminalInterativo(stderr);
    const char* verde = cores ? "\033[32m" : "";
    const char* vermelho = cores ? "\033[31m" : "";
    const char* negrito = cores ? "\033[1m" : "";
    const char* reset = cores ? "\033[0m" : "";

    // Lê o fonte (para os diagnósticos mostrarem a linha com o erro)
    std::ifstream arquivo(op.entrada, std::ios::binary);
    if (!arquivo) {
        std::cerr << "automatoc: não foi possível abrir '" << op.entrada << "'\n";
        return 1;
    }
    std::ostringstream conteudo;
    conteudo << arquivo.rdbuf();
    arquivo.close();

    dsl::Diagnosticos& diag = dsl::diag();
    diag.carregarFonte(op.entrada, conteudo.str());
    diag.usarCores(cores);

    yyin = std::fopen(op.entrada.c_str(), "rb");
    if (!yyin) {
        std::cerr << "automatoc: não foi possível abrir '" << op.entrada << "'\n";
        return 1;
    }

    // ---- Fase 1: análise léxica, sintática e semântica (TDS) ----
    std::cout << negrito << "automatoc" << reset << ": compilando " << op.entrada << '\n';
    dsl::Automato automato;
    dsl::AnalisadorSemantico semantico(automato, diag);
    int resultado = yyparse(semantico);
    std::fclose(yyin);

    if (resultado != 0 || diag.erros() > 0) {
        int erros = diag.erros() > 0 ? diag.erros() : 1;
        std::cerr << vermelho << negrito << "compilação falhou" << reset << ": "
                  << plural(erros, "erro", "erros") << ", "
                  << plural(diag.avisos(), "aviso", "avisos") << '\n';
        return 1;
    }
    std::cout << "  " << verde << "✓" << reset << " análise léxica, sintática e semântica ("
              << plural(diag.avisos(), "aviso", "avisos") << ")\n";
    std::cout << "  autômato \"" << automato.nome << "\": " << automato.descreverTipo() << ", "
              << plural(static_cast<int>(automato.estados.size()), "estado", "estados") << ", "
              << plural(static_cast<int>(automato.transicoes.size()), "transição", "transições")
              << ", Σ = " << automato.descreverAlfabeto() << '\n';

    // ---- Fase 2: execução dos testes ----
    std::vector<dsl::ResultadoTeste> resultados;
    for (const auto& t : automato.testes) resultados.push_back(automato.simular(t.palavra));
    if (!resultados.empty()) {
        std::cout << "  testes:\n";
        for (const auto& r : resultados) {
            std::cout << "    " << (r.aceita ? verde : vermelho) << (r.aceita ? "✓ ACEITA   " : "✗ REJEITA  ")
                      << reset << "\"" << r.palavra << "\"  " << automato.descreverTrilha(r) << '\n';
        }
    }

    // ---- Fase 3: geração de código (SVG) ----
    fs::path destino = op.saida.empty()
                           ? fs::path(op.entrada).parent_path() / fs::u8path(automato.saida)
                           : fs::path(op.saida);
    std::error_code ec;
    if (destino.has_parent_path()) fs::create_directories(destino.parent_path(), ec);

    const std::string svg =
        dsl::gerarSVG(automato, resultados, fs::path(op.entrada).filename().string());
    std::ofstream out(destino, std::ios::binary);
    if (!out || !(out << svg)) {
        std::cerr << "automatoc: não foi possível gravar '" << destino.string() << "'\n";
        return 1;
    }
    out.close();
    std::cout << "  " << verde << "✓" << reset << " SVG gerado: " << destino.string() << '\n';

    // ---- Fase 4: execução (abre o resultado) ----
    if (op.abrir) {
        fs::path absoluto = fs::absolute(destino, ec);
        std::cout << "  → abrindo no visualizador padrão...\n";
        std::cout.flush();
        if (!dsl::abrirNoVisualizador((ec ? destino : absoluto).string()))
            std::cerr << "automatoc: não foi possível abrir o SVG automaticamente\n";
    }
    return 0;
}
