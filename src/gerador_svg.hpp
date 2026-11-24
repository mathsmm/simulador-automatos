// Geração de código: traduz a representação intermediária (Automato) para a
// linguagem de destino, SVG, desenhando o diagrama de estados.
#pragma once

#include <string>
#include <vector>

#include "automato.hpp"

namespace dsl {

std::string gerarSVG(const Automato& automato,
                     const std::vector<ResultadoTeste>& resultados,
                     const std::string& arquivoFonte);

}  // namespace dsl
