#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <termios.h>
#include <sys/select.h>
#include "coprocessador.h"
#include "bitmap.h"

#define IMG_WIDTH 160
#define IMG_HEIGHT 120
#define IMG_SIZE (IMG_WIDTH * IMG_HEIGHT)

/* Parâmetros visuais do cursor e retângulo */
#define CURSOR_SIZE 5
#define CURSOR_COLOR 255      /* Branco */
#define CURSOR_BORDER_COLOR 0 /* Preto para contraste */
#define RECT_THICKNESS 2
#define RECT_COLOR 255 /* Branco */
#define CORNER_SIZE 8

/* Estrutura para região de zoom */
typedef struct
{
    int x1, y1;           /* Primeiro canto */
    int x2, y2;           /* Segundo canto */
    int ativo;            /* Se a janela está ativa */
    int pontos_definidos; /* Quantos pontos foram definidos (0, 1 ou 2) */
} JanelaZoom;

/* Algoritmos disponíveis */
typedef enum
{
    ALG_VIZINHO_PROXIMO = 0,
    ALG_REPLICACAO = 1,
    ALG_MEDIA = 2
} TipoAlgoritmo;

/* Estado global da aplicação */
typedef struct
{
    unsigned char *imagem_original;
    unsigned char *imagem_atual;
    JanelaZoom janela;
    TipoAlgoritmo algoritmo;
    float nivel_zoom; /* 1.0 = original, 2.0 = 2x, 0.5 = 0.5x */
    int mouse_x, mouse_y;
} EstadoApp;

/* ========================================================================
   FUNÇÕES DE TERMINAL E ENTRADA
   ======================================================================== */

struct termios original_term;

void configurar_terminal_nao_canonico()
{
    struct termios new_term;
    tcgetattr(STDIN_FILENO, &original_term);
    new_term = original_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
}

void restaurar_terminal()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
}

int tecla_disponivel()
{
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

/* ========================================================================
   FUNÇÕES DE DESENHO - CURSOR E RETÂNGULO NO BUFFER
   ======================================================================== */

/* Desenha linha horizontal no buffer */
void desenhar_linha_horizontal(unsigned char *buffer, int x1, int x2, int y,
                               int largura, int altura, unsigned char cor)
{
    int x;
    if (y < 0 || y >= altura)
        return;

    int start = (x1 < 0) ? 0 : x1;
    int end = (x2 >= largura) ? largura - 1 : x2;

    for (x = start; x <= end; x++)
    {
        buffer[y * largura + x] = cor;
    }
}

/* Desenha linha vertical no buffer */
void desenhar_linha_vertical(unsigned char *buffer, int x, int y1, int y2,
                             int largura, int altura, unsigned char cor)
{
    int y;
    if (x < 0 || x >= largura)
        return;

    int start = (y1 < 0) ? 0 : y1;
    int end = (y2 >= altura) ? altura - 1 : y2;

    for (y = start; y <= end; y++)
    {
        buffer[y * largura + x] = cor;
    }
}

/* Desenha cursor em forma de cruz com borda para visibilidade */
void desenhar_cursor(unsigned char *buffer, int mouse_x, int mouse_y,
                     int largura, int altura)
{

    /* Cruz com borda preta para ser visível em qualquer fundo */

    /* Linha horizontal (com borda) */
    desenhar_linha_horizontal(buffer, mouse_x - CURSOR_SIZE, mouse_x + CURSOR_SIZE,
                              mouse_y - 1, largura, altura, CURSOR_BORDER_COLOR);
    desenhar_linha_horizontal(buffer, mouse_x - CURSOR_SIZE, mouse_x + CURSOR_SIZE,
                              mouse_y, largura, altura, CURSOR_COLOR);
    desenhar_linha_horizontal(buffer, mouse_x - CURSOR_SIZE, mouse_x + CURSOR_SIZE,
                              mouse_y + 1, largura, altura, CURSOR_BORDER_COLOR);

    /* Linha vertical (com borda) */
    desenhar_linha_vertical(buffer, mouse_x - 1, mouse_y - CURSOR_SIZE,
                            mouse_y + CURSOR_SIZE, largura, altura, CURSOR_BORDER_COLOR);
    desenhar_linha_vertical(buffer, mouse_x, mouse_y - CURSOR_SIZE,
                            mouse_y + CURSOR_SIZE, largura, altura, CURSOR_COLOR);
    desenhar_linha_vertical(buffer, mouse_x + 1, mouse_y - CURSOR_SIZE,
                            mouse_y + CURSOR_SIZE, largura, altura, CURSOR_BORDER_COLOR);
}

/* Desenha retângulo de seleção */
void desenhar_retangulo(unsigned char *buffer, int x1, int y1, int x2, int y2,
                        int largura, int altura)
{
    int i;

    /* Normalizar coordenadas (garantir x1 < x2 e y1 < y2) */
    if (x1 > x2)
    {
        int temp = x1;
        x1 = x2;
        x2 = temp;
    }
    if (y1 > y2)
    {
        int temp = y1;
        y1 = y2;
        y2 = temp;
    }

    /* Desenhar bordas superior e inferior */
    for (i = 0; i < RECT_THICKNESS; i++)
    {
        desenhar_linha_horizontal(buffer, x1, x2, y1 + i,
                                  largura, altura, RECT_COLOR);
        desenhar_linha_horizontal(buffer, x1, x2, y2 - i,
                                  largura, altura, RECT_COLOR);
    }

    /* Desenhar bordas esquerda e direita */
    for (i = 0; i < RECT_THICKNESS; i++)
    {
        desenhar_linha_vertical(buffer, x1 + i, y1, y2,
                                largura, altura, RECT_COLOR);
        desenhar_linha_vertical(buffer, x2 - i, y1, y2,
                                largura, altura, RECT_COLOR);
    }
}

/* Desenha cantos animados quando apenas 1 ponto foi definido */
void desenhar_cantos_animados(unsigned char *buffer, int x, int y,
                              int largura, int altura, int fase)
{
    /* Animar variando o tamanho com base na fase */
    int tamanho = CORNER_SIZE + (fase % 4);

    /* Canto em L invertido */
    desenhar_linha_horizontal(buffer, x - 1, x + tamanho, y - 1,
                              largura, altura, CURSOR_BORDER_COLOR);
    desenhar_linha_horizontal(buffer, x, x + tamanho, y,
                              largura, altura, CURSOR_COLOR);
    desenhar_linha_horizontal(buffer, x - 1, x + tamanho, y + 1,
                              largura, altura, CURSOR_BORDER_COLOR);

    desenhar_linha_vertical(buffer, x - 1, y - 1, y + tamanho,
                            largura, altura, CURSOR_BORDER_COLOR);
    desenhar_linha_vertical(buffer, x, y, y + tamanho,
                            largura, altura, CURSOR_COLOR);
    desenhar_linha_vertical(buffer, x + 1, y - 1, y + tamanho,
                            largura, altura, CURSOR_BORDER_COLOR);

    /* Ponto central */
    if (x >= 0 && x < largura && y >= 0 && y < altura)
    {
        buffer[y * largura + x] = CURSOR_COLOR;
    }
}

/* ========================================================================
   FUNÇÕES DE PROCESSAMENTO DE REGIÃO
   ======================================================================== */

void normalizar_janela(JanelaZoom *janela)
{
    int temp;
    if (janela->x1 > janela->x2)
    {
        temp = janela->x1;
        janela->x1 = janela->x2;
        janela->x2 = temp;
    }
    if (janela->y1 > janela->y2)
    {
        temp = janela->y1;
        janela->y1 = janela->y2;
        janela->y2 = temp;
    }
}

void extrair_regiao(unsigned char *imagem_completa, unsigned char *regiao,
                    int x1, int y1, int x2, int y2)
{
    int largura_regiao = x2 - x1;
    int altura_regiao = y2 - y1;
    int y, x;

    for (y = 0; y < altura_regiao; y++)
    {
        for (x = 0; x < largura_regiao; x++)
        {
            int src_x = x1 + x;
            int src_y = y1 + y;
            if (src_x >= 0 && src_x < IMG_WIDTH && src_y >= 0 && src_y < IMG_HEIGHT)
            {
                regiao[y * largura_regiao + x] =
                    imagem_completa[src_y * IMG_WIDTH + src_x];
            }
        }
    }
}

void sobrepor_regiao(unsigned char *imagem_base, unsigned char *regiao,
                     int x1, int y1, int largura, int altura)
{
    int y, x;

    for (y = 0; y < altura && (y1 + y) < IMG_HEIGHT; y++)
    {
        for (x = 0; x < largura && (x1 + x) < IMG_WIDTH; x++)
        {
            int dst_idx = (y1 + y) * IMG_WIDTH + (x1 + x);
            int src_idx = y * largura + x;
            if (dst_idx < IMG_SIZE)
            {
                imagem_base[dst_idx] = regiao[src_idx];
            }
        }
    }
}

/* ========================================================================
   VALIDAÇÃO DE ALGORITMO E ZOOM
   ======================================================================== */

int algoritmo_zoom_compativel(TipoAlgoritmo algoritmo, float zoom)
{
    /* Algoritmo Média só funciona com redução (0.5x e 0.25x) */
    if (algoritmo == ALG_MEDIA && zoom > 1.0f)
    {
        return 0;
    }

    /* Algoritmo Replicação só funciona com ampliação (2x e 4x) */
    if (algoritmo == ALG_REPLICACAO && zoom < 1.0f)
    {
        return 0;
    }

    return 1;
}

void ajustar_zoom_para_algoritmo(EstadoApp *estado)
{
    /* Se algoritmo MÉDIA foi selecionado e zoom > 1.0, ajusta para 0.5x */
    if (estado->algoritmo == ALG_MEDIA && estado->nivel_zoom > 1.0f)
    {
        printf("\n  Algoritmo Média não suporta ampliação (2x/4x)\n");
        printf("   Ajustando zoom para 0.5x...\n");
        estado->nivel_zoom = 0.5f;
    }

    /* Se algoritmo REPLICAÇÃO foi selecionado e zoom < 1.0, ajusta para 2x */
    if (estado->algoritmo == ALG_REPLICACAO && estado->nivel_zoom < 1.0f)
    {
        printf("\n  Algoritmo Replicação não suporta redução (0.5x/0.25x)\n");
        printf("   Ajustando zoom para 2x...\n");
        estado->nivel_zoom = 2.0f;
    }
}

/* ========================================================================
   PROCESSAMENTO COM ALGORITMO + OVERLAY VISUAL
   ======================================================================== */

void processar_com_algoritmo(EstadoApp *estado)
{
    static int frame_counter = 0;
    unsigned char *buffer_temporario = NULL;
    unsigned char *regiao_extraida = NULL;
    unsigned char *regiao_processada = NULL;

    printf("\n[PROCESSAMENTO] Aplicando zoom %.2fx ", estado->nivel_zoom);

    /* Copiar imagem original para buffer de trabalho */
    memcpy(estado->imagem_atual, estado->imagem_original, IMG_SIZE);

    /* ====================================================================
       PROCESSAR APENAS A REGIÃO SELECIONADA (SE HOUVER)
       ==================================================================== */

    if (estado->janela.ativo && estado->janela.pontos_definidos == 2 &&
        estado->nivel_zoom != 1.0f)
    {

        normalizar_janela(&estado->janela);

        int largura_janela = estado->janela.x2 - estado->janela.x1;
        int altura_janela = estado->janela.y2 - estado->janela.y1;
        int tamanho_regiao = largura_janela * altura_janela;

        printf("na região (%d,%d) até (%d,%d) [%dx%d]\n",
               estado->janela.x1, estado->janela.y1,
               estado->janela.x2, estado->janela.y2,
               largura_janela, altura_janela);

        /* Alocar buffers temporários */
        regiao_extraida = (unsigned char *)malloc(tamanho_regiao);
        buffer_temporario = (unsigned char *)malloc(IMG_SIZE);

        if (!regiao_extraida || !buffer_temporario)
        {
            printf("ERRO: Falha ao alocar memória temporária\n");
            goto cleanup;
        }

        /* 1. Extrair apenas a região selecionada */
        extrair_regiao(estado->imagem_original, regiao_extraida,
                       estado->janela.x1, estado->janela.y1,
                       estado->janela.x2, estado->janela.y2);

        /* 2. Criar imagem 160x120 com a região no centro (resto preto) */
        memset(buffer_temporario, 0, IMG_SIZE);

        /* Calcular posição para centralizar a região */
        int offset_x = (IMG_WIDTH - largura_janela) / 2;
        int offset_y = (IMG_HEIGHT - altura_janela) / 2;

        /* Copiar região para o centro do buffer temporário */
        sobrepor_regiao(buffer_temporario, regiao_extraida,
                        offset_x, offset_y, largura_janela, altura_janela);

        /* 3. Enviar buffer temporário para FPGA e processar */
        carregar_imagem(buffer_temporario, IMG_SIZE);

        /* Aplicar algoritmo com validação */
        if (estado->nivel_zoom == 2.0f)
        {
            if (estado->algoritmo == ALG_MEDIA)
            {
                printf("AVISO: Média não suporta 2X, usando Vizinho Próximo\n");
                api_vizinho_2x();
            }
            else if (estado->algoritmo == ALG_REPLICACAO)
            {
                printf("Algoritmo: Replicação 2X (região)\n");
                api_replicacao_2x();
            }
            else
            {
                printf("Algoritmo: Vizinho Próximo 2X (região)\n");
                api_vizinho_2x();
            }
        }
        else if (estado->nivel_zoom == 4.0f)
        {
            if (estado->algoritmo == ALG_MEDIA)
            {
                printf("AVISO: Média não suporta 4X, usando Vizinho Próximo\n");
                api_vizinho_4x();
            }
            else if (estado->algoritmo == ALG_REPLICACAO)
            {
                printf("Algoritmo: Replicação 4X (região)\n");
                api_replicacao_4x();
            }
            else
            {
                printf("Algoritmo: Vizinho Próximo 4X (região)\n");
                api_vizinho_4x();
            }
        }
        else if (estado->nivel_zoom == 0.5f)
        {
            if (estado->algoritmo == ALG_REPLICACAO)
            {
                printf("AVISO: Replicação não suporta 0.5X, usando Vizinho Próximo\n");
                api_vizinho_0_5x();
            }
            else if (estado->algoritmo == ALG_MEDIA)
            {
                printf("Algoritmo: Média 0.5X (região)\n");
                api_media_0_5x();
            }
            else
            {
                printf("Algoritmo: Vizinho Próximo 0.5X (região)\n");
                api_vizinho_0_5x();
            }
        }
        else if (estado->nivel_zoom == 0.25f)
        {
            if (estado->algoritmo == ALG_REPLICACAO)
            {
                printf("AVISO: Replicação não suporta 0.25X, usando Vizinho Próximo\n");
                api_vizinho_0_25x();
            }
            else if (estado->algoritmo == ALG_MEDIA)
            {
                printf("Algoritmo: Média 0.25X (região)\n");
                api_media_0_25x();
            }
            else
            {
                printf("Algoritmo: Vizinho Próximo 0.25X (região)\n");
                api_vizinho_0_25x();
            }
        }

    /* Liberar buffers temporários */
    cleanup:
        if (regiao_extraida)
            free(regiao_extraida);
        if (buffer_temporario)
            free(buffer_temporario);

        /* Desenhar overlays na imagem original (para feedback visual) */
        desenhar_retangulo(estado->imagem_atual,
                           estado->janela.x1, estado->janela.y1,
                           estado->janela.x2, estado->janela.y2,
                           IMG_WIDTH, IMG_HEIGHT);
    }
    else
    {
        /* SEM JANELA SELECIONADA ou ZOOM 1X - processar imagem completa */

        if (estado->janela.pontos_definidos == 1)
        {
            /* Desenhar apenas o primeiro canto com animação */
            desenhar_cantos_animados(estado->imagem_atual,
                                     estado->janela.x1, estado->janela.y1,
                                     IMG_WIDTH, IMG_HEIGHT, frame_counter++);
            printf("(aguardando segundo ponto)\n");
        }
        else
        {
            printf("na imagem completa\n");
        }

        /* Desenhar cursor */
        desenhar_cursor(estado->imagem_atual,
                        estado->mouse_x, estado->mouse_y,
                        IMG_WIDTH, IMG_HEIGHT);

        /* Carregar e processar imagem completa */
        carregar_imagem(estado->imagem_atual, IMG_SIZE);

        if (estado->nivel_zoom == 1.0f)
        {
            printf("Algoritmo: Bypass (1X)\n");
            api_bypass();
        }
        else if (estado->nivel_zoom == 2.0f)
        {
            if (estado->algoritmo == ALG_MEDIA)
            {
                printf("AVISO: Média não suporta 2X, usando Vizinho Próximo\n");
                api_vizinho_2x();
            }
            else if (estado->algoritmo == ALG_REPLICACAO)
            {
                printf("Algoritmo: Replicação 2X\n");
                api_replicacao_2x();
            }
            else
            {
                printf("Algoritmo: Vizinho Próximo 2X\n");
                api_vizinho_2x();
            }
        }
        else if (estado->nivel_zoom == 4.0f)
        {
            if (estado->algoritmo == ALG_MEDIA)
            {
                printf("AVISO: Média não suporta 4X, usando Vizinho Próximo\n");
                api_vizinho_4x();
            }
            else if (estado->algoritmo == ALG_REPLICACAO)
            {
                printf("Algoritmo: Replicação 4X\n");
                api_replicacao_4x();
            }
            else
            {
                printf("Algoritmo: Vizinho Próximo 4X\n");
                api_vizinho_4x();
            }
        }
        else if (estado->nivel_zoom == 0.5f)
        {
            if (estado->algoritmo == ALG_REPLICACAO)
            {
                printf("AVISO: Replicação não suporta 0.5X, usando Vizinho Próximo\n");
                api_vizinho_0_5x();
            }
            else if (estado->algoritmo == ALG_MEDIA)
            {
                printf("Algoritmo: Média 0.5X\n");
                api_media_0_5x();
            }
            else
            {
                printf("Algoritmo: Vizinho Próximo 0.5X\n");
                api_vizinho_0_5x();
            }
        }
        else if (estado->nivel_zoom == 0.25f)
        {
            if (estado->algoritmo == ALG_REPLICACAO)
            {
                printf("AVISO: Replicação não suporta 0.25X, usando Vizinho Próximo\n");
                api_vizinho_0_25x();
            }
            else if (estado->algoritmo == ALG_MEDIA)
            {
                printf("Algoritmo: Média 0.25X\n");
                api_media_0_25x();
            }
            else
            {
                printf("Algoritmo: Vizinho Próximo 0.25X\n");
                api_vizinho_0_25x();
            }
        }
    }

    printf("[OK] Processamento concluído!\n");
}

int carregar_nova_imagem(EstadoApp *estado)
{
    char caminho[256];

    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║           CARREGAR NOVA IMAGEM BMP                    ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\nDigite o caminho do arquivo BMP: ");
    fflush(stdout);

    /* Restaurar terminal para ler linha */
    restaurar_terminal();

    if (fgets(caminho, sizeof(caminho), stdin) == NULL)
    {
        configurar_terminal_nao_canonico();
        return 0;
    }

    /* Remover newline */
    caminho[strcspn(caminho, "\n")] = 0;

    /* Configurar terminal não-canônico novamente */
    configurar_terminal_nao_canonico();

    /* Validar entrada */
    if (strlen(caminho) == 0)
    {
        printf(" Operação cancelada\n");
        return 0;
    }

    printf("\n Carregando: %s\n", caminho);

    /* Tentar carregar nova imagem em buffer temporário */
    unsigned char *temp_buffer = (unsigned char *)malloc(IMG_SIZE);
    if (!temp_buffer)
    {
        printf(" ERRO: Falha ao alocar memória temporária\n");
        return 0;
    }

    if (carregar_bitmap(caminho, temp_buffer, IMG_WIDTH, IMG_HEIGHT) != 0)
    {
        printf(" ERRO: Falha ao carregar bitmap\n");
        printf("   Verifique se o arquivo existe e é um BMP válido (160x120, 8-bit)\n");
        free(temp_buffer);
        return 0;
    }

    /* Sucesso! Substituir imagem atual */
    memcpy(estado->imagem_original, temp_buffer, IMG_SIZE);
    memcpy(estado->imagem_atual, temp_buffer, IMG_SIZE);
    free(temp_buffer);

    /* Resetar estado */
    estado->janela.pontos_definidos = 0;
    estado->janela.ativo = 0;
    estado->nivel_zoom = 1.0f;
    estado->algoritmo = ALG_VIZINHO_PROXIMO;

    printf(" Nova imagem carregada com sucesso!\n");
    printf(" Estado resetado (Zoom 1x, Algoritmo Vizinho Próximo)\n");

    /* Atualizar display */
    carregar_imagem(estado->imagem_atual, IMG_SIZE);
    api_bypass();

    return 1;
}

/* ========================================================================
   INTERFACE DO USUÁRIO
   ======================================================================== */

void mostrar_interface(EstadoApp *estado)
{
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║     SISTEMA DE PROCESSAMENTO DE IMAGENS - ETAPA 3      ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    printf("\nPosição do Mouse: (%d, %d)\n", estado->mouse_x, estado->mouse_y);
    printf("Zoom Atual: %.2fx\n", estado->nivel_zoom);

    printf("Algoritmo Selecionado: ");
    switch (estado->algoritmo)
    {
    case ALG_VIZINHO_PROXIMO:
        printf("Vizinho Próximo (Suporta: todos os zooms)\n");
        break;
    case ALG_REPLICACAO:
        printf("Replicação (Suporta: 2x e 4x apenas)\n");
        break;
    case ALG_MEDIA:
        printf("Média (Suporta: 0.5x e 0.25x apenas)\n");
        break;
    }

    printf("\nJanela de Zoom:\n");
    if (estado->janela.pontos_definidos == 0)
    {
        printf("   └─ Nenhum ponto definido. Clique para marcar o primeiro canto.\n");
    }
    else if (estado->janela.pontos_definidos == 1)
    {
        printf("   └─ Primeiro canto: (%d, %d)\n", estado->janela.x1, estado->janela.y1);
        printf("   └─ Clique para marcar o segundo canto.\n");
    }
    else
    {
        printf("   └─ Região: (%d,%d) até (%d,%d)\n",
               estado->janela.x1, estado->janela.y1,
               estado->janela.x2, estado->janela.y2);
        printf("   └─ Status: %s\n", estado->janela.ativo ? "ATIVA " : "Inativa");
    }

    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║ CONTROLES                                              ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf("║ [Clique Esquerdo]  → Definir cantos da janela          ║\n");
    printf("║ [+]                → Zoom In                           ║\n");
    printf("║ [-]                → Zoom Out                          ║\n");
    printf("║ [1]                → Algoritmo: Vizinho Próximo        ║\n");
    printf("║ [2]                → Algoritmo: Replicação (2x/4x)     ║\n");
    printf("║ [3]                → Algoritmo: Média (0.5x/0.25x)     ║\n");
    printf("║ [L]                → Carregar nova imagem BMP          ║\n");
    printf("║ [R]                → Resetar janela                    ║\n");
    printf("║ [Q]                → Sair                              ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    /* Validar compatibilidade e avisar */
    if (!algoritmo_zoom_compativel(estado->algoritmo, estado->nivel_zoom))
    {
        printf("\n  ATENÇÃO: Combinação atual de algoritmo e zoom incompatível!\n");
        if (estado->algoritmo == ALG_MEDIA && estado->nivel_zoom > 1.0f)
        {
            printf("   Média só funciona com redução (0.5x ou 0.25x)\n");
        }
        if (estado->algoritmo == ALG_REPLICACAO && estado->nivel_zoom < 1.0f)
        {
            printf("   Replicação só funciona com ampliação (2x ou 4x)\n");
        }
    }
}

int validar_mudanca_zoom(TipoAlgoritmo algoritmo, float zoom_atual, int direcao)
{
    float novo_zoom = (direcao > 0) ? zoom_atual * 2.0f : zoom_atual / 2.0f;

    /* Algoritmo Média só funciona com redução (0.5x e 0.25x) */
    if (algoritmo == ALG_MEDIA && novo_zoom > 1.0f)
    {
        return 0; /* Não permite */
    }

    /* Algoritmo Replicação só funciona com ampliação (2x e 4x) */
    if (algoritmo == ALG_REPLICACAO && novo_zoom < 1.0f)
    {
        return 0; /* Não permite */
    }

    return 1; /* Permite */
}

/* ========================================================================
   FUNÇÃO PRINCIPAL
   ======================================================================== */

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <arquivo.bmp>\n", argv[0]);
        return 1;
    }

    /* Inicializar estado */
    EstadoApp estado = {0};
    estado.nivel_zoom = 1.0f;
    estado.algoritmo = ALG_VIZINHO_PROXIMO;

    /* Alocar buffers */
    estado.imagem_original = (unsigned char *)malloc(IMG_SIZE);
    estado.imagem_atual = (unsigned char *)malloc(IMG_SIZE);

    if (!estado.imagem_original || !estado.imagem_atual)
    {
        fprintf(stderr, "ERRO: Falha ao alocar memória\n");
        return 1;
    }

    /* ====================================================================
       CARREGAR BITMAP
       ==================================================================== */
    printf("Carregando arquivo bitmap: %s\n", argv[1]);

    if (carregar_bitmap(argv[1], estado.imagem_original, IMG_WIDTH, IMG_HEIGHT) != 0)
    {
        fprintf(stderr, "ERRO: Falha ao carregar bitmap\n");
        free(estado.imagem_original);
        free(estado.imagem_atual);
        return 1;
    }

    memcpy(estado.imagem_atual, estado.imagem_original, IMG_SIZE);
    printf(" Bitmap carregado com sucesso!\n");

    /* ====================================================================
       INICIALIZAR COPROCESSADOR
       ==================================================================== */
    printf("\n Inicializando coprocessador...\n");
    iniciar_coprocessador();
    printf(" Coprocessador inicializado!\n");

    /* Carregar imagem inicial */
    carregar_imagem(estado.imagem_atual, IMG_SIZE);
    api_bypass();

    /* ====================================================================
       ABRIR DISPOSITIVO DE MOUSE
       ==================================================================== */
    int mouse_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (mouse_fd < 0)
    {
        /* Tenta outros dispositivos */
        mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
        if (mouse_fd < 0)
        {
            mouse_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
            if (mouse_fd < 0)
            {
                fprintf(stderr, "AVISO: Não foi possível abrir mouse. Controle apenas por teclado.\n");
                fprintf(stderr, "       Tente: sudo chmod 666 /dev/input/event*\n");
            }
        }
    }

    /* Configurar terminal */
    configurar_terminal_nao_canonico();

    /* ====================================================================
       LOOP PRINCIPAL
       ==================================================================== */
    struct input_event ev;
    int executando = 1;
    int mouse_moved = 0;
    int last_mouse_x = -1;
    int last_mouse_y = -1;
    int update_counter = 0;

    mostrar_interface(&estado);

    while (executando)
    {
        /* Processar eventos do mouse */
        if (mouse_fd >= 0)
        {
            while (read(mouse_fd, &ev, sizeof(ev)) > 0)
            {
                if (ev.type == EV_REL)
                {
                    if (ev.code == REL_X)
                    {
                        estado.mouse_x += ev.value;
                        if (estado.mouse_x < 0)
                            estado.mouse_x = 0;
                        if (estado.mouse_x >= IMG_WIDTH)
                            estado.mouse_x = IMG_WIDTH - 1;
                        mouse_moved = 1;
                    }
                    else if (ev.code == REL_Y)
                    {
                        estado.mouse_y += ev.value;
                        if (estado.mouse_y < 0)
                            estado.mouse_y = 0;
                        if (estado.mouse_y >= IMG_HEIGHT)
                            estado.mouse_y = IMG_HEIGHT - 1;
                        mouse_moved = 1;
                    }

                    /* Atualizar display no terminal */
                    printf("\r Mouse: (%d, %d)    ", estado.mouse_x, estado.mouse_y);
                    fflush(stdout);
                }
                else if (ev.type == EV_KEY && ev.code == BTN_LEFT && ev.value == 1)
                {
                    /* Clique do botão esquerdo - APENAS EM MODO BYPASS (1X) */

                    if (estado.nivel_zoom != 1.0f)
                    {
                        printf("\n  Seleção de janela disponível apenas em modo 1x (bypass)\n");
                        printf("   Pressione [-] para voltar ao zoom 1x\n");
                        continue;
                    }

                    if (estado.janela.pontos_definidos == 0)
                    {
                        estado.janela.x1 = estado.mouse_x;
                        estado.janela.y1 = estado.mouse_y;
                        estado.janela.pontos_definidos = 1;
                        printf("\n Primeiro canto definido: (%d, %d)\n",
                               estado.janela.x1, estado.janela.y1);
                        mouse_moved = 1;
                    }
                    else if (estado.janela.pontos_definidos == 1)
                    {
                        estado.janela.x2 = estado.mouse_x;
                        estado.janela.y2 = estado.mouse_y;
                        estado.janela.pontos_definidos = 2;
                        estado.janela.ativo = 1;
                        normalizar_janela(&estado.janela);
                        printf("\n Segundo canto definido: (%d, %d)\n",
                               estado.janela.x2, estado.janela.y2);
                        printf(" Janela ativada!\n");
                        mostrar_interface(&estado);
                        mouse_moved = 1;
                    }
                }
            }
        }

        /* ATUALIZAR VGA QUANDO MOUSE SE MOVER - APENAS EM BYPASS (1X) */
        if (mouse_moved &&
            (estado.mouse_x != last_mouse_x || estado.mouse_y != last_mouse_y))
        {

            /* Só atualiza cursor em tempo real se estiver em modo bypass (1x) */
            if (estado.nivel_zoom == 1.0f)
            {
                processar_com_algoritmo(&estado);
            }

            last_mouse_x = estado.mouse_x;
            last_mouse_y = estado.mouse_y;
            mouse_moved = 0;
        }

        /* Atualização periódica para animação do primeiro canto - APENAS EM 1X */
        if (estado.janela.pontos_definidos == 1 && estado.nivel_zoom == 1.0f)
        {
            if (update_counter++ % 50 == 0)
            { /* A cada ~500ms */
                processar_com_algoritmo(&estado);
            }
        }

        /* Processar teclas */
        if (tecla_disponivel())
        {
            char tecla = getchar();

            switch (tecla)
            {
            case '+':
            case '=':
                /* Zoom in */
                if (estado.nivel_zoom < 4.0f)
                {
                    /* Validar se o algoritmo suporta o próximo nível de zoom */
                    if (validar_mudanca_zoom(estado.algoritmo, estado.nivel_zoom, 1))
                    {
                        estado.nivel_zoom *= 2.0f;
                        processar_com_algoritmo(&estado);
                        mostrar_interface(&estado);
                    }
                    else
                    {
                        printf("\n  Algoritmo %s não suporta zoom %.2fx\n",
                               estado.algoritmo == ALG_MEDIA ? "Média" : "Replicação",
                               estado.nivel_zoom * 2.0f);
                        printf("   Use [1] para Vizinho Próximo (suporta todos os zooms)\n");
                    }
                }
                else
                {
                    printf("\n  Zoom máximo atingido (4x)\n");
                }
                break;

            case '-':
            case '_':
                /* Zoom out */
                if (estado.nivel_zoom > 0.25f)
                {
                    /* Validar se o algoritmo suporta o próximo nível de zoom */
                    if (validar_mudanca_zoom(estado.algoritmo, estado.nivel_zoom, -1))
                    {
                        estado.nivel_zoom /= 2.0f;
                        processar_com_algoritmo(&estado);
                        mostrar_interface(&estado);
                    }
                    else
                    {
                        printf("\n  Algoritmo %s não suporta zoom %.2fx\n",
                               estado.algoritmo == ALG_REPLICACAO ? "Replicação" : "Média",
                               estado.nivel_zoom / 2.0f);
                        printf("   Use [1] para Vizinho Próximo (suporta todos os zooms)\n");
                    }
                }
                else
                {
                    printf("\n  Zoom mínimo atingido (0.25x)\n");
                }
                break;

            case '1':
                estado.algoritmo = ALG_VIZINHO_PROXIMO;
                printf("\n Algoritmo alterado: Vizinho Próximo\n");
                processar_com_algoritmo(&estado);
                mostrar_interface(&estado);
                break;

            case '2':
                estado.algoritmo = ALG_REPLICACAO;
                printf("\n Algoritmo alterado: Replicação\n");

                /* Se incompatível, volta para 1x */
                if (!algoritmo_zoom_compativel(estado.algoritmo, estado.nivel_zoom))
                {
                    printf("  Replicação não suporta zoom %.2fx\n", estado.nivel_zoom);
                    printf("   Voltando para zoom 1x...\n");
                    estado.nivel_zoom = 1.0f;
                }

                processar_com_algoritmo(&estado);
                mostrar_interface(&estado);
                break;

            case '3':
                estado.algoritmo = ALG_MEDIA;
                printf("\n Algoritmo alterado: Média\n");

                /* Se incompatível, volta para 1x */
                if (!algoritmo_zoom_compativel(estado.algoritmo, estado.nivel_zoom))
                {
                    printf("  Média não suporta zoom %.2fx\n", estado.nivel_zoom);
                    printf("   Voltando para zoom 1x...\n");
                    estado.nivel_zoom = 1.0f;
                }

                processar_com_algoritmo(&estado);
                mostrar_interface(&estado);
                break;

            case 'l':
            case 'L':
                /* Carregar nova imagem */
                if (carregar_nova_imagem(&estado))
                {
                    mostrar_interface(&estado);
                }
                else
                {
                    /* Se falhou ou cancelou, continua com a imagem atual */
                    printf("\n  Continuando com a imagem atual\n");
                    mostrar_interface(&estado);
                }
                break;

            case 'r':
            case 'R':
                /* Resetar janela */
                estado.janela.pontos_definidos = 0;
                estado.janela.ativo = 0;
                estado.nivel_zoom = 1.0f;
                printf("\n Janela resetada\n");
                processar_com_algoritmo(&estado);
                mostrar_interface(&estado);
                break;

            case 'q':
            case 'Q':
                executando = 0;
                break;
            }
        }

        usleep(10000); /* 10ms delay */
    }

    /* ====================================================================
       LIMPEZA E FINALIZAÇÃO
       ==================================================================== */
    printf("\n\n Encerrando sistema...\n");

    restaurar_terminal();

    if (mouse_fd >= 0)
    {
        close(mouse_fd);
    }

    limpar_imagem();
    encerrar_coprocessador();

    free(estado.imagem_original);
    free(estado.imagem_atual);

    printf(" Sistema encerrado com sucesso!\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║          Obrigado por usar o sistema!                ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    return 0;
}