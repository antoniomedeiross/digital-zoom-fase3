# Makefile para o Sistema de Processamento de Imagens - Etapa 3

# Compilador e flags
CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = 

# Arquivos fonte
SOURCES = main.c bitmap.c coprocessador.s
OBJECTS = main.o bitmap.o coprocessador.o

# Nome do executável
TARGET = exec

# Imagem padrão
DEFAULT_IMG = img/agata.bmp

# Regra padrão (compila tudo)
all: $(TARGET)

# Regra para criar o executável
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
	@echo "✓ Compilação concluída com sucesso!"
	@echo "  Execute com: sudo ./$(TARGET) $(DEFAULT_IMG)"

# Regra para compilar arquivos .c em .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para compilar arquivo assembly .s em .o
%.o: %.s
	$(CC) -c $< -o $@

# Executa o programa com sudo (necessário para acesso ao mouse)
run: $(TARGET)
	sudo ./$(TARGET) $(DEFAULT_IMG)

# Limpa arquivos compilados
clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ Arquivos compilados removidos"

# Recompila tudo do zero
rebuild: clean all

# Mostra informações de ajuda
help:
	@echo "════════════════════════════════════════════════════════"
	@echo "  Sistema de Processamento de Imagens - Makefile"
	@echo "════════════════════════════════════════════════════════"
	@echo ""
	@echo "Comandos disponíveis:"
	@echo "  make          - Compila o projeto"
	@echo "  make run      - Compila e executa com imagem padrão"
	@echo "  make clean    - Remove arquivos compilados"
	@echo "  make rebuild  - Recompila tudo do zero"
	@echo "  make help     - Mostra esta ajuda"
	@echo ""
	@echo "Uso manual:"
	@echo "  sudo ./exec <arquivo.bmp>"
	@echo ""

# Indica que estas regras não são arquivos
.PHONY: all run clean rebuild help