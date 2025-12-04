// ========================================================================
// bitmap.h - Header para manipulação de arquivos BMP
// ========================================================================

#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>

// Estruturas BMP (packed para corresponder ao formato do arquivo)
#pragma pack(push, 1)

typedef struct {
    uint16_t tipo;          // "BM" = 0x4D42
    uint32_t tamanho;       // Tamanho do arquivo
    uint16_t reservado1;
    uint16_t reservado2;
    uint32_t offset;        // Offset para os dados da imagem
} BMPHeader;

typedef struct {
    uint32_t tamanho;       // Tamanho deste header (40 bytes)
    int32_t  largura;
    int32_t  altura;
    uint16_t planos;        // Sempre 1
    uint16_t bits_por_pixel;
    uint32_t compressao;
    uint32_t tamanho_imagem;
    int32_t  resolucao_x;
    int32_t  resolucao_y;
    uint32_t cores_usadas;
    uint32_t cores_importantes;
} BMPInfoHeader;

#pragma pack(pop)

/**
 * Carrega um arquivo BMP em escala de cinza
 * 
 * @param nome_arquivo: Caminho do arquivo BMP
 * @param buffer: Buffer de saída (deve estar alocado)
 * @param largura_esperada: Largura esperada da imagem
 * @param altura_esperada: Altura esperada da imagem
 * @return 0 em sucesso, -1 em erro
 */
int carregar_bitmap(const char *nome_arquivo, unsigned char *buffer, 
                    int largura_esperada, int altura_esperada);

/**
 * Salva buffer em arquivo BMP (escala de cinza)
 * 
 * @param nome_arquivo: Caminho do arquivo de saída
 * @param buffer: Buffer com os dados da imagem
 * @param largura: Largura da imagem
 * @param altura: Altura da imagem
 * @return 0 em sucesso, -1 em erro
 */
int salvar_bitmap(const char *nome_arquivo, unsigned char *buffer,
                  int largura, int altura);

#endif // BITMAP_H
