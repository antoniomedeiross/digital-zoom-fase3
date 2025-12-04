// ========================================================================
// coprocessador.h
// Header da API do Coprocessador de Processamento de Imagens
// 
// Declara as funções Assembly disponíveis para controle do coprocessador
// implementado na FPGA
// ========================================================================

#ifndef COPROCESSADOR_H
#define COPROCESSADOR_H

#ifdef __cplusplus
extern "C" {
#endif

// ========================================================================
// FUNÇÕES DE INICIALIZAÇÃO E CONTROLE
// ========================================================================

/**
 * Inicializa o coprocessador
 * - Abre /dev/mem
 * - Mapeia a ponte Lightweight HPS-FPGA na memória virtual
 * 
 * DEVE ser chamada antes de qualquer outra função
 */
void iniciar_coprocessador(void);

/**
 * Encerra o coprocessador
 * - Libera o mapeamento de memória (munmap)
 * - Fecha /dev/mem
 * 
 * DEVE ser chamada ao finalizar o uso do coprocessador
 */
void encerrar_coprocessador(void);

// ========================================================================
// FUNÇÕES DE TRANSFERÊNCIA DE DADOS
// ========================================================================

/**
 * Carrega imagem da memória HPS para a memória da FPGA
 * 
 * @param buffer_hps: Ponteiro para buffer na memória HPS (imagem fonte)
 * @param tamanho: Tamanho da imagem em bytes (tipicamente 160x120 = 19200)
 * 
 * A imagem deve estar em escala de cinza (8 bits por pixel)
 */
void carregar_imagem(unsigned char *buffer_hps, int tamanho);

/**
 * Limpa (zera) toda a memória de imagem na FPGA
 * 
 * Útil para resetar o estado antes de carregar nova imagem
 */
void limpar_imagem(void);

// ========================================================================
// FUNÇÕES DA ISA - OPERAÇÕES DE PROCESSAMENTO
// ========================================================================

/**
 * Bypass (Sem processamento) - 1X
 * 
 * Copia a imagem original para o framebuffer sem aplicar zoom
 * Opcode: 0
 */
void api_bypass(void);

// ------------------------------------------------------------------------
// ALGORITMO: MÉDIA (Redução com suavização)
// ------------------------------------------------------------------------

/**
 * Redução por Média - 0.5X
 * 
 * Reduz a imagem para metade do tamanho calculando a média
 * de blocos 2x2 de pixels
 * Opcode: 11
 */
void api_media_0_5x(void);

/**
 * Redução por Média - 0.25X
 * 
 * Reduz a imagem para 1/4 do tamanho calculando a média
 * de blocos 4x4 de pixels
 * Opcode: 12
 */
void api_media_0_25x(void);

// ------------------------------------------------------------------------
// ALGORITMO: VIZINHO MAIS PRÓXIMO (Nearest Neighbor)
// ------------------------------------------------------------------------

/**
 * Ampliação por Vizinho Mais Próximo - 2X
 * 
 * Dobra o tamanho da imagem replicando o pixel mais próximo
 * Opcode: 17
 */
void api_vizinho_2x(void);

/**
 * Ampliação por Vizinho Mais Próximo - 4X
 * 
 * Quadruplica o tamanho da imagem replicando o pixel mais próximo
 * Opcode: 18
 */
void api_vizinho_4x(void);

/**
 * Redução por Vizinho Mais Próximo - 0.5X
 * 
 * Reduz a imagem para metade selecionando pixels alternados
 * Opcode: 27
 */
void api_vizinho_0_5x(void);

/**
 * Redução por Vizinho Mais Próximo - 0.25X
 * 
 * Reduz a imagem para 1/4 selecionando 1 pixel a cada 4
 * Opcode: 28
 */
void api_vizinho_0_25x(void);

// ------------------------------------------------------------------------
// ALGORITMO: REPLICAÇÃO (Ampliação com duplicação direta)
// ------------------------------------------------------------------------

/**
 * Ampliação por Replicação - 2X
 * 
 * Dobra o tamanho duplicando cada pixel em blocos 2x2
 * Opcode: 33
 */
void api_replicacao_2x(void);

/**
 * Ampliação por Replicação - 4X
 * 
 * Quadruplica o tamanho duplicando cada pixel em blocos 4x4
 * Opcode: 34
 */
void api_replicacao_4x(void);

// ========================================================================
// FUNÇÕES AUXILIARES (se necessário expor)
// ========================================================================

/**
 * Processa imagem com operação genérica
 * 
 * @param operacao: Código da operação (0-1023, 10 bits)
 * 
 * Função de baixo nível que permite enviar qualquer opcode
 * Prefira usar as funções específicas (api_*) ao invés desta
 */
void processar_imagem(int operacao);

#ifdef __cplusplus
}
#endif

#endif // COPROCESSADOR_H