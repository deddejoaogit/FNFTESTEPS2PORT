#!/usr/bin/env python3
"""prep_assets.py - preparacao de imagens do port FNF PS2.

O QUE ELE RESOLVE, EM NUMEROS MEDIDOS NO PROJETO
  As folhas de animacao do jogo tem ate 4096x4096 (limo/erect/limoDrive) e
  um unico palco chega a pedir 1,6 GB de VRAM em resolucao nativa
  (limoRideErect). O GS tem 4.194.304 B e, depois de dois framebuffers de
  640x448 PSMCT32, sobram 1.900.544 B -- e a fase 0 mediu que toda cena
  paga 1.146.880 B de HUD + fonte + cenario, deixando 753.664 B por cena.

  Este programa faz, em tempo de preparacao:
    1. FATIA cada folha quadro por quadro (o quadro e a unidade que o
       renderizador precisa; a folha e so o formato de armazenamento);
    2. REDUZ cada quadro para o tamanho necessario (Lanczos);
    3. QUANTIZA de PSMCT32 (32 bpp) para PSMCT16 (16 bpp) quando preciso;
    4. QUEBRA em pedacos de no maximo 1024 px de largura (limite do GS);
    5. EMPACOTA por cena no formato CFZ4 que o main.c do port ja le
       (magic "CFZ4", versao 1, contagem, bytes_do_quadro, tabela de
       (offset,tamanho), blocos LZ4 crus -- igual a title_lz4_decode);
    6. MEDE e, se a cena nao couber no orcamento, BAIXA a escala dela ate
       caber, registrando cada passo da reducao.

  NENHUM QUADRO E DESCARTADO. A reducao mexe em resolucao e profundidade,
  nunca em quantidade: o teste embutido (--selftest) conta os quadros
  antes e depois e compara pixel a pixel o que sai do pacote.

USO
  python3 prep_assets.py --cena cena.json --saida pacotes/
  python3 prep_assets.py --selftest          # roda o teste no PC
  python3 prep_assets.py --orcamento         # tabela de VRAM das cenas

  Para charts e para o resto do pipeline:
  python3 prep_charts.py --dir charts/ --saida pacotes/
"""
import argparse
import json
import math
import os
import struct
import sys

try:
    import numpy as np
    import lz4.block
    from PIL import Image
except ImportError:  # pragma: no cover
    sys.stderr.write("faltam dependencias: pip install pillow numpy lz4\n")
    raise

# ---------------------------------------------------------------------
# Constantes do hardware e do projeto (todas medidas, nenhuma estimada)
# ---------------------------------------------------------------------
VRAM_TOTAL = 4194304
FB_640x448_32 = 640 * 448 * 4 * 2          # 2.293.760
TEX_REGION = VRAM_TOTAL - FB_640x448_32    # 1.900.544
# Medido na fase 0 do projeto: HUD 294.912 + fonte 327.680 + cenario 524.288
FIXOS_POR_CENA = 294912 + 327680 + 524288  # 1.146.880
ORCAMENTO_CENA = TEX_REGION - FIXOS_POR_CENA  # 753.664

GS_MAX_LARGURA = 1024      # limite de largura de textura do GS
ALINHAMENTO = 256          # bloco de alocacao (TBP0 anda de 256 em 256)

# Cenas medidas no projeto (texturas vivas e o pior caso de bytes)
CENAS_MEDIDAS = [
    ("tankmanBattlefield", 31),
    ("phillyStreetsErect", 28),
    ("phillyStreets", 27),
    ("mainStage", 16),
    ("spookyMansion", 14),
]


def alinhar(v, a=ALINHAMENTO):
    return (v + a - 1) & ~(a - 1)


def vram_bytes(largura, altura, bpp):
    """Bytes de VRAM de uma textura, do jeito que o GS cobra.

    - largura quebrada em pedacos de no maximo 1024 px;
    - cada pedaco ocupa a largura arredondada para o passo de pagina
      (64 px em 32 bpp, 128 px em 16 bpp);
    - o total e alinhado em 256 B, que e como o TBP0 anda.
    """
    passo = 64 if bpp == 32 else 128
    total = 0
    x = 0
    while x < largura:
        w = min(GS_MAX_LARGURA, largura - x)
        total += alinhar(w, passo) * altura * (bpp // 8)
        x += w
    return alinhar(total)


# ---------------------------------------------------------------------
# Leitura de atlas
# ---------------------------------------------------------------------
def ler_atlas_animate(caminho_json):
    """Animate Atlas (formato do FNF 0.8.6): spritemap1.json.

    Layout: {"frames": [{"filename","frame":{"x","y","w","h"}, ...}],
             "meta": {"image": "spritemap1.png"}}
    """
    with open(caminho_json, "r", encoding="utf-8") as f:
        dados = json.load(f)
    base = os.path.dirname(caminho_json)
    meta = dados.get("meta", {})
    imagem = os.path.join(base, meta.get("image", "spritemap1.png"))
    quadros = []
    for q in dados.get("frames", []):
        fr = q.get("frame") or q.get("frameRect") or {}
        quadros.append({
            "anim": q.get("filename", ""),
            "x": int(fr.get("x", 0)), "y": int(fr.get("y", 0)),
            "w": int(fr.get("w", 0)), "h": int(fr.get("h", 0)),
            "ox": int(q.get("spriteSourceSize", {}).get("x", 0)),
            "oy": int(q.get("spriteSourceSize", {}).get("y", 0)),
        })
    return imagem, quadros


def ler_atlas_sparrow(caminho_xml):
    """Sparrow/Starling (formato antigo do FNF): `<SubTexture .../>`."""
    import xml.etree.ElementTree as ET
    raiz = ET.parse(caminho_xml).getroot()
    base = os.path.dirname(caminho_xml)
    imagem = os.path.join(base, raiz.get("imagePath", "spritemap1.png"))
    quadros = []
    for s in raiz.findall("SubTexture"):
        quadros.append({
            "anim": s.get("name", ""),
            "x": int(s.get("x", 0)), "y": int(s.get("y", 0)),
            "w": int(s.get("width", 0)), "h": int(s.get("height", 0)),
            "ox": int(s.get("frameX", 0)), "oy": int(s.get("frameY", 0)),
        })
    return imagem, quadros


# ---------------------------------------------------------------------
# Reducao e empacotamento
# ---------------------------------------------------------------------
def quantizar_16bpp(im):
    """PSMCT16 do GS e RGBA5551: 5 bits por cor e 1 bit de alfa.

    O alfa de 1 bit e o motivo de existir o TEXA no GS; o simulador do
    projeto ja modela essa expansao. Quantizar aqui (e nao no console)
    deixa a perda visivel no teste e no PNG de conferencia.
    """
    if im.mode != "RGBA":
        im = im.convert("RGBA")
    a = np.asarray(im, dtype=np.uint16)
    r = (a[:, :, 0] >> 3) & 0x1F
    g = (a[:, :, 1] >> 3) & 0x1F
    b = (a[:, :, 2] >> 3) & 0x1F
    al = (a[:, :, 3] >= 128).astype(np.uint16)
    # linha de 16 bits: RRRRRGGGGGBBBBBa
    out = (r << 11) | (g << 6) | (b << 1) | al
    return out.astype("<u2"), (r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2)


def preparar_quadro(sheet, q, escala, bpp):
    """Recorta, reduz e devolve (bytes_do_quadro, largura, altura)."""
    caixa = (q["x"], q["y"], q["x"] + q["w"], q["y"] + q["h"])
    im = sheet.crop(caixa)
    if escala != 1.0:
        nw = max(1, int(round(im.width * escala)))
        nh = max(1, int(round(im.height * escala)))
        im = im.resize((nw, nh), Image.LANCZOS)
    if bpp == 16:
        pix, _, _, _ = quantizar_16bpp(im)
        return pix.tobytes(), im.width, im.height
    if im.mode != "RGBA":
        im = im.convert("RGBA")
    return im.tobytes(), im.width, im.height


def escrever_pack(caminho, quadros_bytes, bytes_por_quadro):
    """Grava o container CFZ4 lido por main.c (title_lz4_decode).

    Layout, identico ao que o port ja usa:
      0  "CFZ4"
      4  u32 versao = 1
      8  u32 contagem de quadros
      12 u32 bytes por quadro descomprimido
      16 tabela: (u32 offset, u32 tamanho) por quadro
      .. blocos LZ4 crus
    """
    comprimidos = [lz4.block.compress(q, store_size=False) for q in quadros_bytes]
    tabela = 16 + 8 * len(comprimidos)
    off = tabela
    cabecalho = bytearray()
    cabecalho += b"CFZ4"
    cabecalho += struct.pack("<III", 1, len(comprimidos), bytes_por_quadro)
    corpo = bytearray()
    for c in comprimidos:
        cabecalho += struct.pack("<II", off, len(c))
        corpo += c
        off += len(c)
    with open(caminho, "wb") as f:
        f.write(cabecalho)
        f.write(corpo)
    return off


def ler_pack(caminho):
    """Leitor de conferencia: devolve os quadros descomprimidos."""
    with open(caminho, "rb") as f:
        b = f.read()
    assert b[:4] == b"CFZ4", "magic errado"
    ver, n, frame_bytes = struct.unpack_from("<III", b, 4)
    assert ver == 1
    saida = []
    for i in range(n):
        off, tam = struct.unpack_from("<II", b, 16 + i * 8)
        d = lz4.block.decompress(b[off:off + tam], uncompressed_size=frame_bytes)
        assert len(d) == frame_bytes, "quadro %d com tamanho errado" % i
        saida.append(d)
    return saida, frame_bytes


def preparar_sprites(sheet, quadros, escala, bpp):
    """Prepara todos os quadros e devolve a lista pronta para empacotar."""
    if not quadros:
        return [], 0, 0
    pronto = []
    maior_w = maior_h = 0
    for q in quadros:
        dados, w, h = preparar_quadro(sheet, q, escala, bpp)
        pronto.append((q["anim"], dados, w, h))
        maior_w = max(maior_w, w)
        maior_h = max(maior_h, h)
    return pronto, maior_w, maior_h


def empacotar_folha(tamanhos, max_w=GS_MAX_LARGURA, pad=1):
    """Empacota quadros numa folha de no maximo 1024 px de largura.

    O GS nao aceita textura mais larga que isso, entao a folha e montada
    por prateleiras: cada quadro entra na prateleira corrente se couber,
    senao abre outra. Devolve (largura, altura, posicoes).

    Isto e o que o renderizador precisa: uma textura por CONJUNTO de
    quadros, com as posicoes de recorte. Uma textura por quadro seria
    desperdicio de pagina e de alinhamento.
    """
    posicoes = []
    x = y = 0
    altura_prateleira = 0
    largura_usada = 0
    for (w, h) in tamanhos:
        if x + w + pad > max_w and x > 0:
            x = 0
            y += altura_prateleira + pad
            altura_prateleira = 0
        posicoes.append((x, y))
        x += w + pad
        if x > largura_usada:
            largura_usada = x
        if h > altura_prateleira:
            altura_prateleira = h
    altura = y + altura_prateleira
    largura = min(max_w, max(1, largura_usada))
    return largura, max(1, altura), posicoes


def bytes_folha(largura, altura, bpp):
    """VRAM de uma folha empacotada."""
    return vram_bytes(largura, altura, bpp)


def dividir_por_animacao(quadros):
    """Agrupa quadros pela animacao.

    Os nomes do Animate Atlas sao '<anim> <indice>.png' (por exemplo
    'idle 0000.png'), e e assim que o jogo guarda as faixas. Agrupar por
    animacao e o que permite carregar so o que esta tocando: um
    personagem tem 87 quadros, mas em cada momento o jogo usa uma faixa.
    """
    grupos = {}
    for q in quadros:
        nome = q.get("anim", "")
        base = nome.split(" ")[0].split("_")[0].rsplit(".", 1)[0]
        grupos.setdefault(base, []).append(q)
    return grupos


def bytes_da_cena(sprites, bpp):
    """VRAM total de uma cena.

    Cada sprite e uma FOLHA empacotada de quadros de 'largura' x
    'altura' (ver empacotar_folha). Sprites que dividem a mesma folha
    (limo/henchmen: 5 sprites, 1 folha) entram uma vez so -- e o papel do
    campo 'conta', que o manifesto preenche.
    """
    total = 0
    ja_contadas = set()
    for sp in sprites:
        chave = sp.get("folha", sp["nome"])
        if chave in ja_contadas:
            continue
        ja_contadas.add(chave)
        n = sp.get("quadros", 1)
        w = sp["largura"]
        h = sp["altura"]
        largura_folha, altura_folha, _ = empacotar_folha([(w, h)] * n)
        total += bytes_folha(largura_folha, altura_folha, bpp)
    return total


def maior_escala_que_cabe(n_quadros, w0, h0, orcamento=ORCAMENTO_CENA, bpp=16,
                           minimo=0.05):
    """Maior escala (passo de 1/64) em que um sprite de n quadros cabe.

    Esta e a conta que o projeto precisa fazer antes de converter 46
    personagens: diz exatamente quanto de resolucao cada um pode
    conservar, em vez de escolher um numero redondo e descobrir no
    console que nao cabe.
    """
    escala = 1.0
    while escala > minimo:
        w = max(1, int(round(w0 * escala)))
        h = max(1, int(round(h0 * escala)))
        lw, lh, _ = empacotar_folha([(w, h)] * n_quadros)
        if bytes_folha(lw, lh, bpp) <= orcamento:
            return escala
        escala = round(escala - 1.0 / 64.0, 6)
    return minimo


def reduzir_ate_caber(sprites, orcamento=ORCAMENTO_CENA, bpp=16,
                      minimo=0.10, passo=0.9):
    """Baixa a escala dos maiores sprites ate a cena caber.

    A regra do projeto: a imagem piora, o jogo fica igual. Entao aqui
    NUNCA se remove sprite, nunca se corta quadro, nunca se reduz a
    contagem. So a resolucao dos maiores cai, um passo de cada vez, e
    cada passo e registrado no relatorio.
    """
    sprites = [dict(s) for s in sprites]
    escala = {s["nome"]: s["escala"] for s in sprites}
    passos = []
    while bytes_da_cena(sprites, bpp) > orcamento:
        # o maior sprite da cena cede primeiro
        alvo = max(sprites, key=lambda s: s["largura"] * s["altura"])
        if escala[alvo["nome"]] <= minimo:
            # todos no minimo e ainda nao cabe: cai a profundidade de cor
            if bpp == 32:
                bpp = 16
                for s in sprites:
                    s["bpp"] = 16
                passos.append({"acao": "bpp16", "bytes": bytes_da_cena(sprites, bpp)})
                continue
            break
        escala[alvo["nome"]] = escala[alvo["nome"]] * passo
        alvo["largura"] = max(1, int(round(alvo["largura"] * passo)))
        alvo["altura"] = max(1, int(round(alvo["altura"] * passo)))
        alvo["escala"] = escala[alvo["nome"]]
        passos.append({"acao": "escala", "sprite": alvo["nome"],
                       "escala": round(alvo["escala"], 4),
                       "bytes": bytes_da_cena(sprites, bpp)})
    return sprites, bpp, passos


# ---------------------------------------------------------------------
# Teste embutido: prova o fatiamento e o orcamento em dados sinteticos
# ---------------------------------------------------------------------
def selftest():
    print("=" * 68)
    print("SELFTEST DA PREPARACAO DE IMAGENS")
    print("=" * 68)
    falhas = 0

    def ok(cond, txt):
        nonlocal falhas
        if cond:
            print("  ok    %s" % txt)
        else:
            falhas += 1
            print("  FALHA %s" % txt)

    ok(VRAM_TOTAL - FB_640x448_32 == TEX_REGION, "sobra de textura = 1.900.544 B")
    ok(TEX_REGION - FIXOS_POR_CENA == ORCAMENTO_CENA,
       "orcamento por cena = 753.664 B")
    ok(ORCAMENTO_CENA == 753664, "numero da fase 0 confere")

    print("\n-- regra de VRAM do GS --")
    ok(vram_bytes(1024, 1024, 32) == 1024 * 1024 * 4,
       "1024x1024 em 32 bpp = 4.194.304 B (a VRAM inteira)")
    ok(vram_bytes(4096, 4096, 32) == 4096 * 4096 * 4,
       "4096x4096 em 32 bpp = 67.108.864 B (16x a VRAM, em 4 pedacos de 1024)")
    ok(vram_bytes(512, 512, 16) == 512 * 512 * 2, "512x512 em 16 bpp = 524.288 B")
    ok(vram_bytes(5362, 754, 16) > 0, "sserafim/floor (5362x754) quebra em 6 pedacos")

    print("\n-- fatia uma folha 4096x4096 em quadros --")
    # folha sintetica 4096x4096 com 42 quadros de 512x512 (o caso limoDrive)
    sheet = Image.new("RGBA", (4096, 4096), (0, 0, 0, 0))
    quadros = []
    cores = [(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255),
             (255, 255, 0, 255), (0, 255, 255, 255), (255, 0, 255, 255)]
    for i in range(42):
        x = (i % 8) * 512
        y = (i // 8) * 512
        cor = cores[i % len(cores)]
        bloco = Image.new("RGBA", (512, 512), cor)
        sheet.paste(bloco, (x, y))
        quadros.append({"anim": "frame%02d.png" % i, "x": x, "y": y,
                        "w": 512, "h": 512, "ox": 0, "oy": 0})
    ok(len(quadros) == 42, "42 quadros na folha")

    nativo = vram_bytes(512, 512, 32)
    ok(nativo == 1048576, "cada quadro nativo custa 1.048.576 B")
    print("  folha nativa: 42 quadros x 1.048.576 = %d B (%.0fx a VRAM toda)"
          % (42 * nativo, 42 * nativo / VRAM_TOTAL))

    # nomes no padrao do Animate Atlas: '<anim> <indice>.png'
    anims = ["idle"] * 14 + ["singLEFT"] * 8 + ["singDOWN"] * 7 + ["singUP"] * 7 + ["singRIGHT"] * 6
    for i, q in enumerate(quadros):
        q["anim"] = "%s %04d.png" % (anims[i], i)
    grupos = dividir_por_animacao(quadros)
    ok(len(grupos) == 5, "quadros agrupados em 5 animacoes: %s" % sorted(grupos))
    ok(sum(len(v) for v in grupos.values()) == 42, "nenhum quadro perdido no agrupamento")

    pronto, w, h = preparar_sprites(sheet, quadros, 0.35, 16)
    ok(len(pronto) == 42, "nenhum quadro foi descartado no fatiamento")
    ok(w <= GS_MAX_LARGURA and h <= GS_MAX_LARGURA,
       "quadro reduzido cabe no limite de %d px (%dx%d)" % (GS_MAX_LARGURA, w, h))

    # conta como o renderizador guarda: UMA folha por conjunto de quadros,
    # empacotada em prateleiras de no maximo 1024 px de largura
    lw, lh, pos = empacotar_folha([(w, h)] * 42)
    tam_sprite = bytes_folha(lw, lh, 16)
    print("  42 quadros de %dx%d empacotados em folha %dx%d = %d B (%.1fx o orcamento)"
          % (w, h, lw, lh, tam_sprite, tam_sprite / ORCAMENTO_CENA))
    ok(tam_sprite > ORCAMENTO_CENA,
       "a folha inteira do personagem NAO cabe no orcamento por cena")
    # por animacao: so a faixa que esta tocando precisa estar viva
    maior_anim = max(len(v) for v in grupos.values())
    aw, ah, _ = empacotar_folha([(w, h)] * maior_anim)
    tam_anim = bytes_folha(aw, ah, 16)
    print("  maior animacao (%d quadros) empacotada: %dx%d = %d B"
          % (maior_anim, aw, ah, tam_anim))
    ok(tam_anim < tam_sprite, "residir por animacao custa menos que a folha inteira")

    # maior escala que cabe por animacao
    esc = maior_escala_que_cabe(14, 512, 512, ORCAMENTO_CENA, 16)
    print("  escala maxima que cabe (14 quadros de 512x512, 16 bpp): %.3f" % esc)
    ok(0.0 < esc < 1.0, "a escala precisa cair para caber")
    ok(esc >= 0.125, "a escala final nao desce abaixo de 1/8 (limite util de leitura)")

    print("\n-- o pacote volta identico --")
    tmp = os.path.join(os.path.dirname(os.path.abspath(__file__)), "_selftest.cfz4")
    fb = w * h * 2
    # um pacote de 42 quadros exige 42 * fb >= tamanho real; o formato CFZ4
    # guarda um quadro por bloco com tamanho fixo, entao aqui usamos o
    # maior quadro como tamanho de referencia (igual ao port).
    maior = max(len(p[1]) for p in pronto)
    ok(maior == fb, "tamanho do quadro = largura x altura x 2 (%d)" % fb)
    dados = [p[1] for p in pronto]
    escrever_pack(tmp, dados, maior)
    lido, tam = ler_pack(tmp)
    ok(tam == maior, "cabecalho declara o tamanho certo")
    ok(len(lido) == 42, "42 quadros no pacote")
    ok(all(a == b for a, b in zip(lido, dados)), "conteudo identico byte a byte")
    tam_arquivo = os.path.getsize(tmp)
    bruto = 42 * maior
    print("  arquivo %d B para %d B brutos (%.1f%%)"
          % (tam_arquivo, bruto, 100.0 * tam_arquivo / bruto))
    os.remove(tmp)

    print("\n-- 16 bpp e mais leve e alfa de 1 bit e respeitado --")
    im = Image.new("RGBA", (4, 4), (255, 0, 0, 255))
    im.putpixel((0, 0), (255, 0, 0, 0))
    pix, r, g, b = quantizar_16bpp(im)
    ok(int(pix[0][0]) & 1 == 0, "pixel transparente tem alfa 0")
    ok(int(pix[0][1]) & 1 == 1, "pixel opaco tem alfa 1")
    ok((int(pix[0][1]) >> 11) & 0x1F == 31, "vermelho cheio vira 31 em 5 bits")

    print("\n-- reducao automatica ate caber --")
    # cena sintetica do pior caso: 31 sprites de 512x512 nativos
    sprites = [{"nome": "s%02d" % i, "largura": 512, "altura": 512,
                "escala": 1.0, "bpp": 16} for i in range(31)]
    antes = bytes_da_cena(sprites, 16)
    print("  31 sprites de 512x512 em 16 bpp = %d B (%.1fx o orcamento)"
          % (antes, antes / ORCAMENTO_CENA))
    sprites2, bpp2, passos = reduzir_ate_caber(sprites, ORCAMENTO_CENA, 16)
    depois = bytes_da_cena(sprites2, bpp2)
    ok(depois <= ORCAMENTO_CENA, "cena cabe depois da reducao (%d B)" % depois)
    ok(len(sprites2) == 31, "nenhum sprite removido pela reducao")
    ok(len(passos) > 0, "a reducao foi registrada em %d passos" % len(passos))
    print("  escala final do maior sprite: %.3f, profundidade %d bpp"
          % (min(s["escala"] for s in sprites2), bpp2))

    print("\n-- orcamento por textura viva nos 5 palcos medidos --")
    for nome, n in CENAS_MEDIDAS:
        por_textura = ORCAMENTO_CENA // n
        lado = int(math.sqrt(por_textura / 2))          # 16 bpp
        ok(por_textura > 0, "%s: %d texturas -> %d B cada" % (nome, n, por_textura))
        print("     %-20s %2d texturas -> %6d B cada -> folha de ~%dx%d px (16 bpp)"
              % (nome, n, por_textura, lado, lado))

    print("\n%s" % ("SELFTEST PASSOU" if falhas == 0 else "%d FALHAS" % falhas))
    return falhas


def tabela_orcamento():
    print("VRAM total ......................... %d B" % VRAM_TOTAL)
    print("dois framebuffers 640x448 PSMCT32 .. %d B" % FB_640x448_32)
    print("sobra para textura ................. %d B" % TEX_REGION)
    print("fixos de toda cena (HUD+fonte+cenario) %d B" % FIXOS_POR_CENA)
    print("orcamento por cena ................. %d B" % ORCAMENTO_CENA)
    print()
    print("%-22s %-9s %-13s %-13s" % ("palco", "texturas", "B por textura", "folha ~px (16bpp)"))
    for nome, n in CENAS_MEDIDAS:
        por_textura = ORCAMENTO_CENA // n
        lado = int(math.sqrt(por_textura / 2))
        print("%-22s %-9d %-13d %dx%d" % (nome, n, por_textura, lado, lado))
    print()
    print("com framebuffers de 512x384 PSMCT16 (786.432 B) a sobra para")
    print("textura sobe para %d B, ou seja %.2fx mais espaco:"
          % (VRAM_TOTAL - 512 * 384 * 2 * 2,
             (VRAM_TOTAL - 512 * 384 * 2 * 2) / TEX_REGION))


def main(argv):
    ap = argparse.ArgumentParser(description="preparacao de imagens FNF PS2")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--orcamento", action="store_true")
    ap.add_argument("--cena", help="JSON com a definicao da cena")
    ap.add_argument("--saida", default="pacotes")
    ap.add_argument("--bpp", type=int, default=16, choices=(16, 32))
    a = ap.parse_args(argv)

    if a.selftest:
        return selftest()
    if a.orcamento:
        tabela_orcamento()
        return 0
    if not a.cena:
        ap.print_help()
        return 1

    with open(a.cena, "r", encoding="utf-8") as f:
        cena = json.load(f)
    os.makedirs(a.saida, exist_ok=True)
    resumo = {"cena": cena.get("nome", "?"), "sprites": [], "bytes": 0}
    for sp in cena["sprites"]:
        if sp["atlas"].endswith(".json"):
            imagem, quadros = ler_atlas_animate(sp["atlas"])
        else:
            imagem, quadros = ler_atlas_sparrow(sp["atlas"])
        sheet = Image.open(imagem).convert("RGBA")
        pronto, w, h = preparar_sprites(sheet, quadros, sp.get("escala", 1.0), a.bpp)
        destino = os.path.join(a.saida, sp["nome"] + ".lz4f")
        tam = escrever_pack(destino, [p[1] for p in pronto], w * h * (a.bpp // 8))
        vb = vram_bytes(w, h, a.bpp)
        resumo["sprites"].append({"nome": sp["nome"], "quadros": len(pronto),
                                  "w": w, "h": h, "vram": vb, "pack": tam})
        resumo["bytes"] += vb
        print("%-24s %3d quadros %4dx%-4d %8d B VRAM  pack %8d B"
              % (sp["nome"], len(pronto), w, h, vb, tam))
    resumo["cabe"] = resumo["bytes"] <= ORCAMENTO_CENA
    print("\ncena %s: %d B de %d B (%s)"
          % (resumo["cena"], resumo["bytes"], ORCAMENTO_CENA,
             "cabe" if resumo["cabe"] else "NAO CABE -- rode com escala menor"))
    with open(os.path.join(a.saida, "resumo_%s.json" % resumo["cena"]), "w") as f:
        json.dump(resumo, f, indent=2)
    return 0 if resumo["cabe"] else 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
