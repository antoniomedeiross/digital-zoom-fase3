#!/usr/bin/env python3
"""
Script para criar imagens BMP de teste em escala de cinza (160x120)
Para uso com o Sistema de Processamento de Imagens
"""

import struct
import os
import numpy as np

def criar_bmp_grayscale(nome_arquivo, largura, altura, dados):
    """
    Cria um arquivo BMP em escala de cinza
    
    Args:
        nome_arquivo: Nome do arquivo de saída
        largura: Largura da imagem
        altura: Altura da imagem
        dados: Array numpy com os pixels (valores 0-255)
    """
    # Calcular padding (linhas devem ser múltiplas de 4 bytes)
    padding = (4 - (largura % 4)) % 4
    tamanho_linha = largura + padding
    tamanho_imagem = tamanho_linha * altura
    tamanho_paleta = 256 * 4  # 256 cores * 4 bytes (BGRA)
    
    # Header BMP (14 bytes)
    bmp_header = struct.pack('<HI2HI',
        0x4D42,  # Assinatura "BM"
        14 + 40 + tamanho_paleta + tamanho_imagem,  # Tamanho do arquivo
        0, 0,    # Reservado
        14 + 40 + tamanho_paleta  # Offset para dados da imagem
    )
    
    # Info Header (40 bytes)
    info_header = struct.pack('<IiiHHIIiiII',
        40,              # Tamanho do header
        largura,         # Largura
        altura,          # Altura (positivo = de baixo para cima)
        1,               # Planos
        8,               # Bits por pixel
        0,               # Compressão (0 = sem compressão)
        tamanho_imagem,  # Tamanho da imagem
        2835,            # Resolução X (pixels/metro)
        2835,            # Resolução Y (pixels/metro)
        256,             # Cores usadas
        256              # Cores importantes
    )
    
    # Criar arquivo
    with open(nome_arquivo, 'wb') as f:
        # Escrever headers
        f.write(bmp_header)
        f.write(info_header)
        
        # Escrever paleta de cores (escala de cinza)
        for i in range(256):
            f.write(struct.pack('BBBB', i, i, i, 0))  # B, G, R, reservado
        
        # Escrever dados (de baixo para cima, como BMP exige)
        padding_bytes = bytes([0] * padding)
        for y in range(altura - 1, -1, -1):
            linha = dados[y, :].astype(np.uint8).tobytes()
            f.write(linha)
            if padding > 0:
                f.write(padding_bytes)
    
    print(f"✓ Criado: {nome_arquivo} ({largura}x{altura})")


def gerar_gradiente_horizontal(largura, altura):
    """Gradiente da esquerda (preto) para direita (branco)"""
    imagem = np.zeros((altura, largura), dtype=np.uint8)
    for x in range(largura):
        valor = int((x / largura) * 255)
        imagem[:, x] = valor
    return imagem


def gerar_gradiente_vertical(largura, altura):
    """Gradiente de cima (preto) para baixo (branco)"""
    imagem = np.zeros((altura, largura), dtype=np.uint8)
    for y in range(altura):
        valor = int((y / altura) * 255)
        imagem[y, :] = valor
    return imagem


def gerar_tabuleiro(largura, altura, tamanho_quadrado=20):
    """Padrão de tabuleiro xadrez"""
    imagem = np.zeros((altura, largura), dtype=np.uint8)
    for y in range(altura):
        for x in range(largura):
            if ((x // tamanho_quadrado) + (y // tamanho_quadrado)) % 2 == 0:
                imagem[y, x] = 255
    return imagem


def gerar_circulos(largura, altura):
    """Círculos concêntricos"""
    imagem = np.zeros((altura, largura), dtype=np.uint8)
    centro_x, centro_y = largura // 2, altura // 2
    
    for y in range(altura):
        for x in range(largura):
            dist = np.sqrt((x - centro_x)**2 + (y - centro_y)**2)
            valor = int((np.sin(dist / 10) * 127 + 128)) % 256
            imagem[y, x] = valor
    return imagem


def gerar_teste_resolucao(largura, altura):
    """Padrão para testar resolução e aliasing"""
    imagem = np.zeros((altura, largura), dtype=np.uint8)
    
    # Linhas horizontais de diferentes espessuras
    for y in range(0, altura, 4):
        espessura = (y // 20) % 3 + 1
        for dy in range(espessura):
            if y + dy < altura:
                imagem[y + dy, :] = 255
    
    # Linhas verticais
    for x in range(0, largura, 4):
        espessura = (x // 20) % 3 + 1
        for dx in range(espessura):
            if x + dx < largura:
                imagem[:, x + dx] = 128
    
    return imagem


def gerar_foto_simulada(largura, altura):
    """Simula uma foto com diferentes regiões de contraste"""
    imagem = np.zeros((altura, largura), dtype=np.uint8)
    
    # Céu (gradiente suave no topo)
    for y in range(altura // 3):
        valor = 200 - int((y / (altura // 3)) * 50)
        imagem[y, :] = valor
    
    # Montanha (triângulos)
    for x in range(largura):
        altura_montanha = int(altura * 0.5 + np.sin(x / 20) * altura * 0.2)
        for y in range(altura // 3, altura):
            if y > altura_montanha:
                # Solo escuro
                imagem[y, x] = 60 + int(np.random.rand() * 30)
            else:
                # Montanha média
                imagem[y, x] = 100 + int(np.random.rand() * 40)
    
    return imagem


def main():
    """Cria conjunto de imagens de teste"""
    
    # Criar diretório se não existir
    os.makedirs('test_images', exist_ok=True)
    
    largura, altura = 160, 120
    
    print("🎨 Gerando imagens de teste...")
    print(f"   Dimensões: {largura}x{altura} pixels\n")
    
    # Gerar diferentes padrões
    criar_bmp_grayscale('test_images/gradiente_horizontal.bmp', 
                        largura, altura, 
                        gerar_gradiente_horizontal(largura, altura))
    
    criar_bmp_grayscale('test_images/gradiente_vertical.bmp',
                        largura, altura,
                        gerar_gradiente_vertical(largura, altura))
    
    criar_bmp_grayscale('test_images/tabuleiro.bmp',
                        largura, altura,
                        gerar_tabuleiro(largura, altura))
    
    criar_bmp_grayscale('test_images/circulos.bmp',
                        largura, altura,
                        gerar_circulos(largura, altura))
    
    criar_bmp_grayscale('test_images/teste_resolucao.bmp',
                        largura, altura,
                        gerar_teste_resolucao(largura, altura))
    
    criar_bmp_grayscale('test_images/foto_simulada.bmp',
                        largura, altura,
                        gerar_foto_simulada(largura, altura))
    
    # Imagem uniforme para teste
    criar_bmp_grayscale('test_images/cinza_medio.bmp',
                        largura, altura,
                        np.full((altura, largura), 128, dtype=np.uint8))
    
    print("\n✅ Todas as imagens de teste foram criadas!")
    print("\nUso:")
    print("  ./processador_imagens test_images/gradiente_horizontal.bmp")


if __name__ == '__main__':
    main()