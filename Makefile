# Compila o automatoc em Linux, macOS ou MSYS2.  No Windows sem MSYS2, use build.ps1.
#
#   make            compila build/automatoc
#   make exemplos   compila e executa todos os exemplos (sem abrir os SVGs)
#   make testes     roda a suíte de testes
#   make clean

CXX      ?= g++
FLEX     ?= flex
BISON    ?= bison
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-function

BUILD  := build
TARGET := $(BUILD)/automatoc
SRCS   := $(wildcard src/*.cpp)
HDRS   := $(wildcard src/*.hpp)
GEN    := $(BUILD)/parser.cpp $(BUILD)/lexer.cpp

all: $(TARGET)

$(BUILD):
	mkdir -p $@

$(BUILD)/parser.cpp $(BUILD)/parser.hpp: src/parser.y | $(BUILD)
	$(BISON) -Wall --defines=$(BUILD)/parser.hpp -o $(BUILD)/parser.cpp $<

$(BUILD)/lexer.cpp: src/lexer.l $(BUILD)/parser.hpp | $(BUILD)
	$(FLEX) -o $@ $<

$(TARGET): $(SRCS) $(HDRS) $(GEN)
	$(CXX) $(CXXFLAGS) -I$(BUILD) -Isrc $(SRCS) $(GEN) -o $@

exemplos: $(TARGET)
	@for f in exemplos/*.aut; do ./$(TARGET) "$$f" --nao-abrir || exit 1; done

testes: $(TARGET)
	sh testes/executar.sh

clean:
	rm -rf $(BUILD)

.PHONY: all exemplos testes clean
