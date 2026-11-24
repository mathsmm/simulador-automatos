// Funções dependentes do sistema operacional (console e abertura do resultado).
#pragma once

#include <cstdio>
#include <string>

namespace dsl {

void configurarConsole();                       // UTF-8 + cores ANSI no Windows
bool terminalInterativo(std::FILE* fluxo);      // saída é um terminal?
bool abrirNoVisualizador(const std::string& caminho);

}  // namespace dsl
