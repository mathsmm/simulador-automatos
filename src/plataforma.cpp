#include "plataforma.hpp"

#include <cstdlib>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <io.h>
#  include <windows.h>
#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#else
#  include <unistd.h>
#endif

namespace dsl {

void configurarConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    for (DWORD id : {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE}) {
        HANDLE h = GetStdHandle(id);
        DWORD modo = 0;
        if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &modo))
            SetConsoleMode(h, modo | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

bool terminalInterativo(std::FILE* fluxo) {
#ifdef _WIN32
    return _isatty(_fileno(fluxo)) != 0;
#else
    return isatty(fileno(fluxo)) != 0;
#endif
}

bool abrirNoVisualizador(const std::string& caminho) {
#ifdef _WIN32
    const std::string comando = "start \"\" \"" + caminho + "\"";
#elif defined(__APPLE__)
    const std::string comando = "open \"" + caminho + "\"";
#else
    const std::string comando = "xdg-open \"" + caminho + "\" >/dev/null 2>&1 &";
#endif
    return std::system(comando.c_str()) == 0;
}

}  // namespace dsl
