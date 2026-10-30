"""LZ4 em bloco (o formato que o jogo decodifica), em Python.

POR QUE EXISTE
O jogo NAO le' PNG: os pacotes guardam quadros ja' em pixels, comprimidos com o
bloco LZ4. O decodificador esta' em C (src/engine/lz4_bloco.c). Este arquivo e'
o lado que COMPRIME -- o empacotador precisa dele -- e o lado que permite
conferir o decodificador do C contra uma implementacao independente, com
vetores reais (tools/gen_lz4_vetores.py -> test/lz4_vetores.h).

O formato (LZ4 block, sem frame): uma sequencia de sequencias. Cada uma comeca
com um token: nibble alto = tamanho dos literais, nibble baixo = tamanho do
match MENOS 4. Se um tamanho vale 15, seguem bytes de extensao somados ate' um
que nao seja 255. Depois dos literais vem o offset do match (2 bytes, little
endian) e o match e' copiado byte a byte (pode se sobrepor, e' assim que dados
periodicos comprimem). A ultima sequencia tem so' literais e nenhum offset.
"""

import struct

MIN_MATCH = 4
MAX_OFFSET = 65535
LAST_LITERALS = 5          # o formato exige os ultimos 5 bytes como literais
MFLIMIT = 12               # nao se procura match nos ultimos 12 bytes


def _put_len(value):
    """(nibble, extensao). O nibble vai dentro do token, a extensao DEPOIS dele --
    por isso as duas coisas vem separadas, e nao escritas direto na saida."""
    if value < 15:
        return value, b""
    rem = value - 15
    ext = bytearray()
    while rem >= 255:
        ext.append(255)
        rem -= 255
    ext.append(rem)
    return 15, bytes(ext)


def compress(data):
    """LZ4 bloco, guloso, com tabela de hash simples.

    Nao tenta ser o melhor compressor: tenta ser o MESMO formato e nunca gerar
    fluxo invalido. Os quadros do jogo sao grandes e a taxa sai perto do que um
    LZ4 normal daria (o conteudo e' pixel de desenho, com muita repeticao
    local).
    """
    n = len(data)
    out = bytearray()
    if n == 0:
        return bytes(out)
    table = {}
    anchor = 0
    i = 0
    limit = n - MFLIMIT if n > MFLIMIT else 0
    while i < limit:
        chave = bytes(data[i:i + 4])
        cand = table.get(chave)
        table[chave] = i
        if cand is None or i - cand > MAX_OFFSET:
            i += 1
            continue
        # estende o match
        mlen = 4
        while i + mlen < n - LAST_LITERALS and data[cand + mlen] == data[i + mlen]:
            mlen += 1
        lit = i - anchor
        token_lit, lit_ext = _put_len(lit)
        token_m, m_ext = _put_len(mlen - MIN_MATCH)
        out.append((token_lit << 4) | token_m)
        out.extend(lit_ext)
        out.extend(data[anchor:i])
        out.extend(struct.pack("<H", i - cand))
        out.extend(m_ext)
        i += mlen
        anchor = i
    lit = n - anchor
    token_lit, lit_ext = _put_len(lit)
    out.append((token_lit << 4) | 0)
    out.extend(lit_ext)
    out.extend(data[anchor:])
    return bytes(out)


def decompress(src, dst_size):
    """Decodificador de referencia. Levanta ValueError em fluxo invalido."""
    ip = 0
    op = bytearray()
    n = len(src)
    while ip < n:
        token = src[ip]
        ip += 1
        lit = token >> 4
        if lit == 15:
            while True:
                if ip >= n:
                    raise ValueError("truncado nos literais")
                v = src[ip]
                ip += 1
                lit += v
                if v != 255:
                    break
        if ip + lit > n:
            raise ValueError("literais alem do fim")
        op.extend(src[ip:ip + lit])
        ip += lit
        if ip == n:
            break
        if ip + 2 > n:
            raise ValueError("offset truncado")
        off = src[ip] | (src[ip + 1] << 8)
        ip += 2
        if off == 0 or off > len(op):
            raise ValueError("offset invalido: %d (saida %d)" % (off, len(op)))
        match = token & 15
        if match == 15:
            while True:
                if ip >= n:
                    raise ValueError("truncado no match")
                v = src[ip]
                ip += 1
                match += v
                if v != 255:
                    break
        match += MIN_MATCH
        for _ in range(match):
            op.append(op[-off])
    if len(op) != dst_size:
        raise ValueError("saida tem %d bytes, esperado %d" % (len(op), dst_size))
    return bytes(op)


def selfcheck():
    """Compressao e decodificacao da mesma implementacao, em casos de borda."""
    casos = [b"", b"a", b"ab" * 3, b"abc" * 100, bytes(300), b"x" + bytes(200) + b"x",
             bytes(range(256)) * 3, b"abcdefgh" * 40 + b"zz", bytes(4096) + b"ta"]
    import random
    rnd = random.Random(1234)
    casos.append(bytes(rnd.randrange(256) for _ in range(2048)))
    casos.append((b"FNF" * 7 + bytes(37)) * 11)
    for i, c in enumerate(casos):
        enc = compress(c)
        dec = decompress(enc, len(c))
        if dec != c:
            raise SystemExit("lz4 selfcheck: caso %d falhou (%d bytes)" % (i, len(c)))
        if len(enc) > len(c) + len(c) // 255 + 16 + len(c) // 8:
            raise SystemExit("lz4 selfcheck: caso %d comprimiu para MAIOR que o "
                             "esperado (%d -> %d)" % (i, len(c), len(enc)))
    return len(casos)


if __name__ == "__main__":
    print("lz4 selfcheck: %d casos" % selfcheck())
