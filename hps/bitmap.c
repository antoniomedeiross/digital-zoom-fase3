
// ========================================================================
// bitmap.c - Implementação
// ========================================================================

#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int carregar_bitmap(const char *nome_arquivo, unsigned char *buffer,
                    int largura_esperada, int altura_esperada) {
    
    FILE *arquivo = fopen(nome_arquivo, "rb");
    if (!arquivo) {
        fprintf(stderr, "ERRO: Não foi possível abrir '%s'\n", nome_arquivo);
        return -1;
    }
    
    // Ler header principal
    BMPHeader header;
    if (fread(&header, sizeof(BMPHeader), 1, arquivo) != 1) {
        fprintf(stderr, "ERRO: Falha ao ler header BMP\n");
        fclose(arquivo);
        return -1;
    }
    
    // Verificar assinatura "BM"
    if (header.tipo != 0x4D42) {
        fprintf(stderr, "ERRO: Arquivo não é BMP válido (assinatura: 0x%X)\n", header.tipo);
        fclose(arquivo);
        return -1;
    }
    
    // Ler info header
    BMPInfoHeader info;
    if (fread(&info, sizeof(BMPInfoHeader), 1, arquivo) != 1) {
        fprintf(stderr, "ERRO: Falha ao ler info header BMP\n");
        fclose(arquivo);
        return -1;
    }
    
    printf("  ├─ Dimensões: %dx%d pixels\n", info.largura, abs(info.altura));
    printf("  ├─ Bits por pixel: %d\n", info.bits_por_pixel);
    printf("  └─ Compressão: %d\n", info.compressao);
    
    // Verificar dimensões
    if (info.largura != largura_esperada || abs(info.altura) != altura_esperada) {
        fprintf(stderr, "ERRO: Dimensões incorretas. Esperado %dx%d, encontrado %dx%d\n",
                largura_esperada, altura_esperada, info.largura, abs(info.altura));
        fclose(arquivo);
        return -1;
    }
    
    // Posicionar no início dos dados
    fseek(arquivo, header.offset, SEEK_SET);
    
    // BMPs são armazenados de baixo para cima, calcular padding
    int largura_bytes = largura_esperada;
    int padding = (4 - (largura_bytes % 4)) % 4;
    int altura_abs = abs(info.altura);
    int invertido = (info.altura > 0); // Se altura > 0, está de cabeça para baixo
    
    // Buffer temporário para linha com padding
    unsigned char *linha = (unsigned char *)malloc(largura_bytes + padding);
    if (!linha) {
        fprintf(stderr, "ERRO: Falha ao alocar buffer temporário\n");
        fclose(arquivo);
        return -1;
    }
    
    // Ler imagem
    if (info.bits_por_pixel == 8) {
        // Imagem em escala de cinza (8 bits)
        
        // Pular tabela de cores se existir
        if (info.cores_usadas > 0 || info.bits_por_pixel == 8) {
            int tamanho_paleta = (info.cores_usadas > 0) ? info.cores_usadas : 256;
            fseek(arquivo, sizeof(BMPHeader) + sizeof(BMPInfoHeader) + tamanho_paleta * 4, SEEK_SET);
        }
        
        int y;
        for (y = 0; y < altura_abs; y++) {
            // BMP armazena de baixo para cima
            int linha_destino = invertido ? (altura_abs - 1 - y) : y;
            
            if (fread(linha, 1, largura_bytes + padding, arquivo) != largura_bytes + padding) {
                fprintf(stderr, "ERRO: Falha ao ler linha %d\n", y);
                free(linha);
                fclose(arquivo);
                return -1;
            }
            
            memcpy(&buffer[linha_destino * largura_esperada], linha, largura_bytes);
        }
        
    } else if (info.bits_por_pixel == 24) {
        // Imagem RGB (24 bits) - converter para escala de cinza
        largura_bytes = largura_esperada * 3;
        padding = (4 - (largura_bytes % 4)) % 4;
        
        free(linha);
        linha = (unsigned char *)malloc(largura_bytes + padding);
        
        int y;
        for (y = 0; y < altura_abs; y++) {
            int linha_destino = invertido ? (altura_abs - 1 - y) : y;
            
            if (fread(linha, 1, largura_bytes + padding, arquivo) != largura_bytes + padding) {
                fprintf(stderr, "ERRO: Falha ao ler linha %d\n", y);
                free(linha);
                fclose(arquivo);
                return -1;
            }
            
            // Converter RGB para escala de cinza: Y = 0.299*R + 0.587*G + 0.114*B
            int x;
            for (x = 0; x < largura_esperada; x++) {
                int idx_rgb = x * 3;
                unsigned char b = linha[idx_rgb];
                unsigned char g = linha[idx_rgb + 1];
                unsigned char r = linha[idx_rgb + 2];
                
                // Conversão para escala de cinza
                unsigned char gray = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
                buffer[linha_destino * largura_esperada + x] = gray;
            }
        }
        
    } else {
        fprintf(stderr, "ERRO: Formato não suportado (%d bits por pixel)\n", 
                info.bits_por_pixel);
        free(linha);
        fclose(arquivo);
        return -1;
    }
    
    free(linha);
    fclose(arquivo);
    
    printf("  └─ Arquivo carregado com sucesso!\n");
    return 0;
}

int salvar_bitmap(const char *nome_arquivo, unsigned char *buffer,
                  int largura, int altura) {
    
    FILE *arquivo = fopen(nome_arquivo, "wb");
    if (!arquivo) {
        fprintf(stderr, "ERRO: Não foi possível criar '%s'\n", nome_arquivo);
        return -1;
    }
    
    // Calcular padding
    int largura_bytes = largura;
    int padding = (4 - (largura_bytes % 4)) % 4;
    int tamanho_linha = largura_bytes + padding;
    int tamanho_imagem = tamanho_linha * altura;
    int tamanho_paleta = 256 * 4; // 256 cores * 4 bytes (BGRA)
    
    // Preencher header
    BMPHeader header = {0};
    header.tipo = 0x4D42; // "BM"
    header.tamanho = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + 
                     tamanho_paleta + tamanho_imagem;
    header.offset = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + tamanho_paleta;
    
    // Preencher info header
    BMPInfoHeader info = {0};
    info.tamanho = sizeof(BMPInfoHeader);
    info.largura = largura;
    info.altura = altura; // Positivo = de baixo para cima
    info.planos = 1;
    info.bits_por_pixel = 8;
    info.compressao = 0; // Sem compressão
    info.tamanho_imagem = tamanho_imagem;
    info.cores_usadas = 256;
    info.cores_importantes = 256;
    
    // Escrever headers
    fwrite(&header, sizeof(BMPHeader), 1, arquivo);
    fwrite(&info, sizeof(BMPInfoHeader), 1, arquivo);
    
    // Escrever paleta de cores (escala de cinza)
    int i;
    for (i = 0; i < 256; i++) {
        unsigned char cor[4] = {i, i, i, 0}; // B, G, R, A
        fwrite(cor, 4, 1, arquivo);
    }
    
    // Escrever dados da imagem (de baixo para cima)
    unsigned char padding_bytes[3] = {0, 0, 0};
    int y;
    for (y = altura - 1; y >= 0; y--) {
        fwrite(&buffer[y * largura], 1, largura, arquivo);
        if (padding > 0) {
            fwrite(padding_bytes, 1, padding, arquivo);
        }
    }
    
    fclose(arquivo);
    printf("Arquivo '%s' salvo com sucesso!\n", nome_arquivo);
    return 0;
}