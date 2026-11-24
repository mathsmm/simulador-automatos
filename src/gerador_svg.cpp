#include "gerador_svg.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <queue>
#include <sstream>
#include <utility>

namespace dsl {

namespace {

// ------------------------------------------------------------ geometria
struct P {
    double x, y;
};
P operator+(P a, P b) { return {a.x + b.x, a.y + b.y}; }
P operator-(P a, P b) { return {a.x - b.x, a.y - b.y}; }
P operator*(P a, double k) { return {a.x * k, a.y * k}; }
double norma(P a) { return std::sqrt(a.x * a.x + a.y * a.y); }
P unitario(P a) {
    double n = norma(a);
    return n > 1e-9 ? a * (1.0 / n) : P{1, 0};
}

constexpr double R = 24.0;        // raio do estado
constexpr double DX = 170.0;      // distância entre camadas (colunas)
constexpr double DY = 110.0;      // distância entre estados da mesma camada
constexpr double MARGEM = 90.0;   // margem lateral do diagrama
constexpr double TOPO = 120.0;    // espaço do título + laços do topo
constexpr double LINHA_RODAPE = 20.0;

std::string n1(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", v);
    return b;
}
std::string pt(P p) { return n1(p.x) + "," + n1(p.y); }

std::string xml(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            case '\'': r += "&apos;"; break;
            default: r += c;
        }
    }
    return r;
}

size_t caracteres(const std::string& s) {  // conta pontos de código UTF-8
    size_t n = 0;
    for (unsigned char c : s)
        if (c < 0x80 || c >= 0xC0) ++n;
    return n;
}

// ------------------------------------------------------------ layout
// Camadas por busca em largura a partir do estado inicial: cada estado fica na
// coluna igual à sua distância (em transições) até o inicial. Estados
// inalcançáveis vão para uma coluna extra à direita.
std::vector<std::vector<int>> calcularCamadas(const Automato& a, std::vector<int>& camada) {
    const int n = static_cast<int>(a.estados.size());
    camada.assign(n, -1);
    std::vector<std::vector<int>> colunas;

    if (a.inicial >= 0) {
        std::queue<int> fila;
        camada[a.inicial] = 0;
        fila.push(a.inicial);
        while (!fila.empty()) {
            int u = fila.front();
            fila.pop();
            if (static_cast<int>(colunas.size()) <= camada[u]) colunas.resize(camada[u] + 1);
            colunas[camada[u]].push_back(u);
            for (const auto& t : a.transicoes) {
                if (t.origem == u && camada[t.destino] < 0) {
                    camada[t.destino] = camada[u] + 1;
                    fila.push(t.destino);
                }
            }
        }
    }

    std::vector<int> soltos;
    for (int i = 0; i < n; ++i)
        if (camada[i] < 0) soltos.push_back(i);
    if (!soltos.empty()) {
        for (int s : soltos) camada[s] = static_cast<int>(colunas.size());
        colunas.push_back(soltos);
    }
    return colunas;
}

struct LinhaRodape {
    std::string classe;
    std::string texto;
};

}  // namespace

std::string gerarSVG(const Automato& a, const std::vector<ResultadoTeste>& resultados,
                     const std::string& arquivoFonte) {
    // 1. layout em camadas
    std::vector<int> camada;
    std::vector<std::vector<int>> colunas = calcularCamadas(a, camada);

    // 2. agrupa transições com mesma origem/destino em uma única aresta
    std::map<std::pair<int, int>, std::vector<std::string>> rotulos;
    std::vector<std::pair<int, int>> ordem;
    for (const auto& t : a.transicoes) {
        auto chave = std::make_pair(t.origem, t.destino);
        auto& lista = rotulos[chave];
        if (lista.empty()) ordem.push_back(chave);
        lista.push_back(descreverSimbolo(t.simbolo));
    }

    // 3. rodapé: resumo + resultados dos testes
    std::vector<LinhaRodape> rodape;
    rodape.push_back({"info", "Σ = " + a.descreverAlfabeto() + "  ·  " + a.descreverTipo() +
                                  "  ·  " + std::to_string(a.estados.size()) + " estados  ·  " +
                                  std::to_string(a.transicoes.size()) + " transições"});
    if (!resultados.empty()) {
        rodape.push_back({"info forte", "Testes:"});
        for (const auto& r : resultados) {
            std::string texto = (r.aceita ? "✓ \"" : "✗ \"") + r.palavra + "\" " +
                                (r.aceita ? "aceita   " : "rejeitada") + "  " +
                                a.descreverTrilha(r);
            rodape.push_back({r.aceita ? "info ok" : "info falha", texto});
        }
    }

    // 4. dimensões
    size_t maxLinhas = 1;
    for (const auto& c : colunas) maxLinhas = std::max(maxLinhas, c.size());
    const double nColunas = colunas.empty() ? 1.0 : static_cast<double>(colunas.size());
    const double larguraConteudo = 2 * MARGEM + (nColunas - 1) * DX;

    double larguraTexto = 48.0 + caracteres(a.nome) * 11.5;
    for (const auto& l : rodape)
        larguraTexto = std::max(larguraTexto, 48.0 + caracteres(l.texto) * 7.6);

    const double largura = std::max({larguraConteudo, larguraTexto, 420.0});
    const double deslocX = (largura - larguraConteudo) / 2.0;
    const double alturaArea = static_cast<double>(maxLinhas) * DY;
    const double yRodape = TOPO + alturaArea + 30.0;
    const double altura = yRodape + rodape.size() * LINHA_RODAPE + 20.0;

    std::vector<P> pos(a.estados.size());
    for (size_t c = 0; c < colunas.size(); ++c) {
        const double m = static_cast<double>(colunas[c].size());
        for (size_t i = 0; i < colunas[c].size(); ++i) {
            pos[colunas[c][i]] = {deslocX + MARGEM + c * DX,
                                  TOPO + (alturaArea - m * DY) / 2.0 + DY * (i + 0.5)};
        }
    }

    // 5. emissão do SVG
    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<!-- Gerado por automatoc a partir de " << xml(arquivoFonte) << " -->\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << n1(largura) << "\" height=\""
        << n1(altura) << "\" viewBox=\"0 0 " << n1(largura) << ' ' << n1(altura) << "\">\n"
        << "  <title>" << xml(a.nome) << "</title>\n"
        << "  <defs>\n"
        << "    <marker id=\"seta\" viewBox=\"0 0 10 10\" refX=\"10\" refY=\"5\" markerWidth=\"10\""
           " markerHeight=\"10\" markerUnits=\"userSpaceOnUse\" orient=\"auto\">\n"
        << "      <path d=\"M0,0 L10,5 L0,10 L3,5 z\" fill=\"#334155\"/>\n"
        << "    </marker>\n"
        << "    <style>\n"
        << "      .titulo { font: 600 20px 'Segoe UI', Helvetica, Arial, sans-serif; fill: #0f172a; }\n"
        << "      .sub { font: 12px 'Segoe UI', Helvetica, Arial, sans-serif; fill: #64748b; }\n"
        << "      .estado { fill: #ffffff; stroke: #1e293b; stroke-width: 2; }\n"
        << "      .estado.inicial { fill: #e0f2fe; }\n"
        << "      .anel { fill: none; stroke: #1e293b; stroke-width: 1.5; }\n"
        << "      .nome { font: 600 14px Consolas, 'Courier New', monospace; fill: #0f172a;"
           " text-anchor: middle; dominant-baseline: central; }\n"
        << "      .aresta { fill: none; stroke: #334155; stroke-width: 1.6; }\n"
        << "      .rotulo { font: 600 13px Consolas, 'Courier New', monospace; fill: #b45309;"
           " text-anchor: middle; dominant-baseline: central; paint-order: stroke;"
           " stroke: #ffffff; stroke-width: 4px; stroke-linejoin: round; }\n"
        << "      .info { font: 12.5px Consolas, 'Courier New', monospace; fill: #334155; }\n"
        << "      .forte { font-weight: 600; }\n"
        << "      .ok { fill: #15803d; }\n"
        << "      .falha { fill: #b91c1c; }\n"
        << "    </style>\n"
        << "  </defs>\n"
        << "  <rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n"
        << "  <text class=\"titulo\" x=\"24\" y=\"36\">" << xml(a.nome) << "</text>\n"
        << "  <text class=\"sub\" x=\"24\" y=\"56\">autômato finito · gerado por automatoc</text>\n";

    // 5a. arestas (desenhadas antes dos estados, que ficam por cima)
    std::ostringstream textos;
    svg << "  <g class=\"transicoes\">\n";
    for (const auto& chave : ordem) {
        const int o = chave.first, d = chave.second;
        std::string rotulo;
        for (const auto& s : rotulos[chave]) rotulo += (rotulo.empty() ? "" : ", ") + s;
        const double larguraRotulo = caracteres(rotulo) * 7.8;

        P ancora;
        if (o == d) {
            // laço: curva cúbica acima do estado
            P c = pos[o];
            P ini = {c.x - R * 0.34, c.y - R * 0.94};
            P fim = {c.x + R * 0.34, c.y - R * 0.94};
            P c1 = {c.x - 28, c.y - R - 48};
            P c2 = {c.x + 28, c.y - R - 48};
            svg << "    <path class=\"aresta\" d=\"M" << pt(ini) << " C" << pt(c1) << ' ' << pt(c2)
                << ' ' << pt(fim) << "\" marker-end=\"url(#seta)\"/>\n";
            ancora = {c.x, c.y - R - 46};
        } else {
            P pu = pos[o], pv = pos[d];
            P dir = unitario(pv - pu);
            P normal = {dir.y, -dir.x};  // esquerda da direção (para cima, se dir = →)
            const bool reverso = rotulos.count({d, o}) > 0;
            const int salto = std::abs(camada[o] - camada[d]);
            const bool curva = reverso || salto != 1;
            const double offRotulo = 12.0 + std::fabs(normal.x) * larguraRotulo / 2.0;

            if (!curva) {
                P ini = pu + dir * R, fim = pv - dir * R;
                svg << "    <path class=\"aresta\" d=\"M" << pt(ini) << " L" << pt(fim)
                    << "\" marker-end=\"url(#seta)\"/>\n";
                ancora = (ini + fim) * 0.5 + normal * offRotulo;
            } else {
                const double dist = norma(pv - pu);
                double desloc = reverso ? 44.0 : 0.0;
                if (salto != 1) desloc = std::max(desloc, 30.0 + 0.18 * dist);
                P ctrl = (pu + pv) * 0.5 + normal * desloc;
                P ini = pu + unitario(ctrl - pu) * R;
                P fim = pv + unitario(ctrl - pv) * R;
                svg << "    <path class=\"aresta\" d=\"M" << pt(ini) << " Q" << pt(ctrl) << ' '
                    << pt(fim) << "\" marker-end=\"url(#seta)\"/>\n";
                P meio = ini * 0.25 + ctrl * 0.5 + fim * 0.25;  // ponto da curva em t = 0,5
                ancora = meio + normal * offRotulo;
            }
        }
        textos << "    <text class=\"rotulo\" x=\"" << n1(ancora.x) << "\" y=\"" << n1(ancora.y)
               << "\">" << xml(rotulo) << "</text>\n";
    }
    svg << "  </g>\n";

    // 5b. seta de entrada do estado inicial
    if (a.inicial >= 0) {
        P c = pos[a.inicial];
        svg << "  <path class=\"aresta\" d=\"M" << pt({c.x - R - 46, c.y}) << " L"
            << pt({c.x - R, c.y}) << "\" marker-end=\"url(#seta)\"/>\n";
    }

    // 5c. estados
    svg << "  <g class=\"estados\">\n";
    for (size_t i = 0; i < a.estados.size(); ++i) {
        const Estado& e = a.estados[i];
        P c = pos[i];
        svg << "    <g>\n      <title>" << xml(e.nome)
            << (e.inicial ? " (inicial)" : "") << (e.aceitacao ? " (aceitação)" : "")
            << "</title>\n";
        svg << "      <circle class=\"estado" << (e.inicial ? " inicial" : "") << "\" cx=\""
            << n1(c.x) << "\" cy=\"" << n1(c.y) << "\" r=\"" << n1(R) << "\"/>\n";
        if (e.aceitacao)
            svg << "      <circle class=\"anel\" cx=\"" << n1(c.x) << "\" cy=\"" << n1(c.y)
                << "\" r=\"" << n1(R - 5) << "\"/>\n";
        svg << "      <text class=\"nome\" x=\"" << n1(c.x) << "\" y=\"" << n1(c.y) << "\">"
            << xml(e.nome) << "</text>\n    </g>\n";
    }
    svg << "  </g>\n";

    // 5d. rótulos por cima de tudo
    svg << "  <g class=\"rotulos\">\n" << textos.str() << "  </g>\n";

    // 5e. rodapé
    for (size_t i = 0; i < rodape.size(); ++i)
        svg << "  <text class=\"" << rodape[i].classe << "\" x=\"24\" y=\""
            << n1(yRodape + i * LINHA_RODAPE) << "\" xml:space=\"preserve\">"
            << xml(rodape[i].texto) << "</text>\n";

    svg << "</svg>\n";
    return svg.str();
}

}  // namespace dsl
