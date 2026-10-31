"""CHP4: o pacote de UM personagem (quadros + tabela de animacoes).

POR QUE EXISTE
O palco ja' e' generico (CFZ4 por prop), o personagem nao: o jogo conhece dois
pacotes escritos no codigo. Cada um dos 46 personagens vira um arquivo
`assets/chars/<id>.chp4` com os quadros comprimidos em LZ4 e, junto, a tabela
que diz quantos quadros cada animacao tem, a que taxa e se volta ao comeco --
porque esse dado nao existe no `character.json` (vem do atlas, que so' o clone
da arte tem).

FORMATO (o mesmo que `src/engine/char_pack.h` le')
    0   "CHP4"
    4   u32 versao = 1
    8   u32 n_quadros
    12  u32 bytes_por_quadro
    16  u32 w
    20  u32 h
    24  u32 n_animacoes
    28  u32 psm (32 ou 16)
    32  u32 escala x10000 (0 nos pacotes v1, que nao guardavam a escala)
    36  n_quadros * (u32 offset, u32 tamanho)
    ... n_animacoes * 40: nome[24], u32 primeiro, quantos, fps*100, looped
    ... os blocos LZ4

Este arquivo e' o lado que ESCREVE (o empacotador) e o lado que LE' em Python,
para que um pacote gerado possa ser conferido sem ligar o jogo -- e para que a
geracao de teste (tools/gen_harness_assets.py) e o empacotador real usem o mesmo
escritor, em vez de duas cópias do formato.
"""

import struct

MAGIC = b"CHP4"
VERSAO = 2
CABECALHO = 36
NOME = 24        # o mais longo do original: "singRIGHTmiss-beautiful" (23 + NUL)
ENTRADA = 40     # nome[24] + 4 campos de 4 bytes
MAX_ANIMS = 64
ERR_TAMANHO = "bytes_por_quadro nao bate com w*h*(psm/8)"


def escrever(quadros, w, h, psm, anims, escala=1.0):
    """Monta o pacote.

    quadros  -- lista de bytes (um por quadro, todos do mesmo tamanho)
    anims    -- lista de dicts {nome, primeiro, quantos, fps100, looped}
    escala   -- pixels de canvas por pixel do original (0,5 = "a escala em que
                o original exibe", ver tools/ps2_display.py). O jogo precisa
                disso para desenhar o personagem no TAMANHO certo: o canvas e'
                a arte ja reduzida, e sem a escala ele nao sabe por quanto
                dividir. Vai no campo que era `reservado`, x10000.
    """
    if psm not in (16, 32):
        raise ValueError("psm tem de ser 16 ou 32")
    if not 0 < w <= 1024 or not 0 < h <= 1024:
        raise ValueError("canvas fora de 1..1024")
    if not quadros:
        raise ValueError("pacote sem quadro nenhum")
    if not 0 < len(anims) <= MAX_ANIMS:
        raise ValueError(f"numero de animacoes fora de 1..{MAX_ANIMS}")
    bpp = psm // 8
    frame_bytes = w * h * bpp
    for i, q in enumerate(quadros):
        if len(q) != frame_bytes:
            raise ValueError(f"quadro {i} tem {len(q)} bytes, esperado {frame_bytes}")

    tabela = CABECALHO + len(quadros) * 8
    anims_off = tabela
    corpo = anims_off + len(anims) * ENTRADA

    entradas = []
    blocos = bytearray()
    for q in quadros:
        comp = comprimir(q)
        entradas.append((corpo + len(blocos), len(comp)))
        blocos += comp

    out = bytearray()
    out += MAGIC
    if not (0.0 < escala <= 20.0):
        raise ValueError(f"escala {escala} fora de 0..20")
    out += struct.pack("<IIIIIII", VERSAO, len(quadros), frame_bytes, w, h, len(anims), psm)
    out += struct.pack("<I", int(round(escala * 10000.0)) & 0xFFFFFFFF)
    for off, tam in entradas:
        out += struct.pack("<II", off, tam)
    for a in anims:
        nome = a["nome"].encode("ascii")
        if len(nome) >= NOME:
            raise ValueError(f"nome de animacao longo demais: {a['nome']}")
        out += nome + b"\x00" * (NOME - len(nome))
        out += struct.pack("<IIII", a["primeiro"], a["quantos"], a["fps100"], a["looped"])
    out += blocos
    return bytes(out)


def montar_de_grupos(grupos, w, h, psm, escala, fps100=2400):
    """Pacote a partir dos quadros JA' separados por animacao.

    grupos -- [(nome, [bytes de cada quadro], looped)] na ordem em que as
              animacoes devem ficar. Cada grupo vira UMA entrada da tabela com
              `primeiro` acumulado e `quantos` = numero de quadros do grupo, entao
              os quadros de uma animacao sao SEMPRE contiguos -- e' o que o
              leitor do jogo assume (`cp_anim_i` aponta para um intervalo).

    Um quadro usado por duas animacoes aparece duas vezes no pacote. Custa
    espaco em disco (nao em VRAM: uma textura por vez) e evita a alternativa
    feia, que seria uma tabela de indices por animacao.

    Devolve (bytes, n_quadros, tabela_de_anims).
    """
    anims = []
    quadros = []
    for nome, lista, looped in grupos:
        if not lista:
            raise ValueError(f"animacao {nome} sem quadro")
        anims.append({"nome": nome, "primeiro": len(quadros), "quantos": len(lista),
                      "fps100": fps100, "looped": 1 if looped else 0})
        quadros.extend(lista)
    if len(quadros) > 4096:
        raise ValueError(f"{len(quadros)} quadros passam do limite de 4096")
    return escrever(quadros, w, h, psm, anims, escala=escala), len(quadros), anims


def ler(dados):
    """Devolve um dict com o cabecalho, a tabela de quadros e a de animacoes.

    Nao descomprime: para conferir o formato, o cabecalho basta; para conferir os
    pixels, use `quadro()`.
    """
    if len(dados) < CABECALHO:
        raise ValueError("cabecalho curto")
    if dados[:4] != MAGIC:
        raise ValueError("nao e' CHP4")
    versao, nq, fbytes, w, h, na, psm = struct.unpack_from("<IIIIIII", dados, 4)
    if versao not in (1, VERSAO):
        raise ValueError(f"versao {versao}")
    # v1 nao guardava a escala: 0 aqui quer dizer "nao sei", e quem le' tem de
    # dizer isso, nao inventar 0,5.
    escala = struct.unpack_from("<I", dados, 32)[0] / 10000.0 if versao >= 2 else 0.0
    if psm not in (16, 32):
        raise ValueError(f"psm {psm}")
    if fbytes != w * h * (psm // 8):
        raise ValueError(ERR_TAMANHO)
    tabela = CABECALHO + nq * 8
    anims_off = tabela
    if anims_off + na * ENTRADA > len(dados):
        raise ValueError("tabela fora do arquivo")
    quadros = []
    for i in range(nq):
        off, tam = struct.unpack_from("<II", dados, CABECALHO + i * 8)
        if tam == 0 or off + tam > len(dados):
            raise ValueError(f"bloco {i} fora do arquivo")
        quadros.append({"offset": off, "tamanho": tam})
    anims = []
    for i in range(na):
        e = anims_off + i * ENTRADA
        nome = dados[e:e + NOME].split(b"\x00")[0].decode("ascii", "replace")
        primeiro, quantos, fps100, looped = struct.unpack_from("<IIII", dados, e + NOME)
        anims.append({"nome": nome, "primeiro": primeiro, "quantos": quantos,
                      "fps100": fps100, "looped": looped})
    return {"n_quadros": nq, "bytes_por_quadro": fbytes, "w": w, "h": h,
            "psm": psm, "quadros": quadros, "anims": anims, "escala": escala,
            "versao": versao}


def conferir(cab):
    """A tabela tem de caber nos quadros que o pacote realmente tem."""
    n = cab["n_quadros"]
    for a in cab["anims"]:
        if a["quantos"] <= 0:
            raise ValueError(f"animacao {a['nome']} sem quadro")
        if a["primeiro"] + a["quantos"] > n:
            raise ValueError(f"animacao {a['nome']} passa do fim ({a['primeiro']}+{a['quantos']} > {n})")
    return True


def quadro(dados, cab, i):
    from lz4py import decompress
    q = cab["quadros"][i]
    return decompress(dados[q["offset"]:q["offset"] + q["tamanho"]], cab["bytes_por_quadro"])


def comprimir(dados):
    from lz4py import compress
    return compress(dados)
