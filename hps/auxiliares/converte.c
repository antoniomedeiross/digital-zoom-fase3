#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void converter_para_bmp() {
    char caminho[300];
    printf("Digite o caminho da imagem JPEG/PNG: ");
    scanf("%s", caminho);

    int largura, altura, canais;
    unsigned char *img = stbi_load(caminho, &largura, &altura, &canais, 3);
    if (!img) {
        printf("Erro ao carregar a imagem.\n");
        return;
    }

    // Dimensões de saída
    int newW = 160;
    int newH = 120;

    unsigned char out[newW * newH];

    // Redimensionar e converter para escala de cinza       
    for (int y = 0; y < newH; y++) {
        for (int x = 0; x < newW; x++) {
            int srcX = x * largura / newW;
            int srcY = y * altura / newH;

            int idx = (srcY * largura + srcX) * 3;  // RGB

            unsigned char r = img[idx];
            unsigned char g = img[idx + 1];
            unsigned char b = img[idx + 2];

            // conversão para cinza (luma)
            unsigned char gray = 0.299*r + 0.587*g + 0.114*b;

            out[y * newW + x] = gray;
        }
    }

    stbi_image_free(img);

    // gera arquivo bmp
    if (!stbi_write_bmp("saida.bmp", newW, newH, 1, out)) {
        printf("Erro ao salvar BMP.\n");
        return;
    }

    printf("Imagem gerada: saida.bmp\n");
}

int main() {
    converter_para_bmp();
    return 0;
}
