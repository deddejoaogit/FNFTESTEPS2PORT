import importlib.util, struct, sys
spec = importlib.util.spec_from_file_location("g", "tools/gen_harness_assets.py")
g = importlib.util.module_from_spec(spec); spec.loader.exec_module(g)

def dec_menu(src, dst_bytes):
    si=di=0
    while si < len(src) and di < dst_bytes:
        c=src[si]; si+=1
        if c<128:
            n=(c+1)*4
            if si+n>len(src) or di+n>dst_bytes: return -1
            si+=n; di+=n
        elif c>128:
            k=257-c
            if si+4>len(src) or di+k*4>dst_bytes: return -2
            di+=k*4; si+=4
        else: return -3
    if si!=len(src) or di!=dst_bytes: return -4
    return 0

def dec_alpha(src, pixels):
    si=di=0
    while si<len(src) and di<pixels:
        c=src[si]; si+=1
        if c<128:
            n=c+1
            if si+n>len(src) or di+n>pixels: return -1
            si+=n; di+=n
        elif c>128:
            if si>=len(src): return -2
            k=257-c; si+=1
            if di+k>pixels: return -3
            di+=k
        else: return -4
    return 0 if (si==len(src) and di==pixels) else -5

def dec_lz4(src, dst_size):
    ip=0; iend=len(src); op=0
    dst=bytearray(dst_size)
    while ip<iend:
        token=src[ip]; ip+=1
        lit=token>>4
        if lit==15:
            while True:
                v=src[ip]; ip+=1; lit+=v
                if v!=255: break
        if iend-ip<lit or dst_size-op<lit: return -2
        dst[op:op+lit]=src[ip:ip+lit]; ip+=lit; op+=lit
        if ip==iend: break
        if iend-ip<2: return -3
        off=src[ip]|(src[ip+1]<<8); ip+=2
        if not off or off>op: return -4
        match=token&15
        if match==15:
            while True:
                v=src[ip]; ip+=1; match+=v
                if v!=255: break
        match+=4
        if dst_size-op<match: return -6
        while match: dst[op]=dst[op-off]; op+=1; match-=1
    return 0 if op==dst_size else -7

ok=True
for name,d,s in [("menu atlas",g.MENU_CACHE,g.MENU_RLE),
                 ("font",384*192*4,g.FONT_RLE),
                 ("story ui",512*256*4,g.STORY_RLE),
                 ("story titles",306_180,g.STORY_TITLES_RLE)]:
    b=g.make_rle(d,s); r=dec_menu(b,d)
    print(f"  RLE menu    {name:14s} arquivo={len(b):>9,} decoded={d:>9,} rc={r}")
    ok &= (r==0 and len(b)==s)

b=g.make_alpha_rle(g.SFX_ALPHA_PIXELS, g.SFX_ALPHA_LEN); r=dec_alpha(b,g.SFX_ALPHA_PIXELS)
print(f"  RLE alfa    arquivo={len(b):>9,} pixels={g.SFX_ALPHA_PIXELS:>9,} rc={r}")
ok &= (r==0 and len(b)==g.SFX_ALPHA_LEN)

for name,count,fb in [("bf_frames",43,258*254*4),("gf_frames",87,320*307*2)]:
    pk=g.make_cfz4(count,fb)
    magic=pk[:4]; ver,cnt,fbytes=struct.unpack("<III",pk[4:16])
    good = (magic==b"CFZ4" and ver==1 and cnt==count and fbytes==fb)
    bad=0
    for i in range(cnt):
        off,size=struct.unpack("<II",pk[16+i*8:24+i*8])
        if off>len(pk) or size>len(pk)-off: bad+=1; continue
        if dec_lz4(pk[off:off+size], fbytes)!=0: bad+=1
    print(f"  CFZ4 {name:10s} arquivo={len(pk):>9,} quadros={cnt} blocos_ruins={bad}")
    ok &= good and bad==0 and len(pk) < 8*1024*1024

# NOTE SPLASH: a regiao que o jogo copia para dentro do atlas do HUD. O teste
# do PC (test/test_splash.c) prova que ela cabe; aqui prova-se que o ARQUIVO
# que o harness entrega tem exatamente esse tamanho e tem alfa de verdade (um
# bloco todo transparente passaria no tamanho e nao desenharia nada).
sw, sh = g.splash_regiao()
sp = g.make_splash(sw, sh)
op = sum(1 for i in range(3, len(sp), 4) if sp[i] > 64)
zero = sum(1 for i in range(3, len(sp), 4) if sp[i] == 0)
print(f"  SPLASH      {sw}x{sh} arquivo={len(sp):>9,} opacos={op:>6} vazios={zero:>6}")
ok &= (len(sp) == sw * sh * 4 and op > sw * sh // 4 and zero > 0)

# TELA DE RESULTADO: um CFZ4 por folha, todos os quadros decodificando no
# tamanho do plano, e o gradiente do fundo com o tamanho certo.
for _id, _w, _h, _nq, _fps, _tex in g.resultado_folhas()[0]:
    pk = g.make_resultado_pack(_w, _h, _nq)
    magic = pk[:4]
    ver, cnt, fbytes = struct.unpack("<III", pk[4:16])
    bad = 0
    for i in range(cnt):
        off, size = struct.unpack("<II", pk[16 + i * 8:24 + i * 8])
        if off > len(pk) or size > len(pk) - off:
            bad += 1; continue
        if dec_lz4(pk[off:off + size], fbytes) != 0:
            bad += 1
    print(f"  RESULTADO   {_id:9s} {_w}x{_h} arquivo={len(pk):>9,} quadros={cnt} "
          f"blocos_ruins={bad}")
    ok &= (magic == b"CFZ4" and ver == 1 and cnt == _nq
           and fbytes == _w * _h * 4 and bad == 0)

# As imagens do rank: uma por rank e eixo, com alfa de verdade (o desenho e'
# justamente a faixa opaca rolando).
nrank = 0
for _a, _w, _h in g.resultado_ranks():
    im = g.make_rank_img(_w, _h, _a.endswith("_vert.rgba"))
    op = sum(1 for i in range(3, len(im), 4) if im[i] > 64)
    print(f"  RESULTADO   {_a:22s} {_w}x{_h} arquivo={len(im):>9,} opacos={op:>6}")
    ok &= (len(im) == _w * _h * 4 and op > 0)
    nrank += 1
ok &= (nrank >= 10)

# O atlas do PLACAR: 52 quadros, o tamanho do plano, e cada um pintado na sua
# celula (so' o quadro desenhado tem pixel).
_rs = g.resultado_score()
if _rs:
    _a, _w, _h, _fr = _rs
    _at = g.make_score_atlas(_w, _h, _fr)
    _pintados = sum(1 for i in range(3, len(_at), 4) if _at[i])
    _cel = sum(f["aw"] * f["ah"] for f in _fr)
    print(f"  RESULTADO   {_a:22s} {_w}x{_h} arquivo={len(_at):>9,} "
          f"quadros={len(_fr)} pintados={_pintados} (celulas={_cel})")
    ok &= (len(_at) == _w * _h * 4 and len(_fr) == 52 and _pintados == _cel
           and all(0 <= f["ax"] and f["ax"] + f["aw"] <= _w and
                   0 <= f["ay"] and f["ay"] + f["ah"] <= _h for f in _fr))
else:
    ok = False

# O atlas dos CONTADORES DE JULGAMENTO: dez glifos, cada um no seu retangulo.
_rt = g.resultado_tallies()
if _rt:
    _a, _w, _h, _fr = _rt
    _at = g.make_tally_atlas(_w, _h, _fr)
    _pintados = sum(1 for i in range(3, len(_at), 4) if _at[i])
    _cel = sum(f["aw"] * f["ah"] for f in _fr)
    print(f"  RESULTADO   {_a:22s} {_w}x{_h} arquivo={len(_at):>9,} "
          f"glifos={len(_fr)} pintados={_pintados} (celulas={_cel})")
    ok &= (len(_at) == _w * _h * 4 and len(_fr) == 10 and _pintados == _cel
           and all(0 <= f["ax"] and f["ax"] + f["aw"] <= _w and
                   0 <= f["ay"] and f["ay"] + f["ah"] <= _h for f in _fr))
else:
    ok = False

# O PERCENTUAL DE CONCLUSAO: tres atlas de digitos e dois textos.
_rc = g.resultado_clear()
if _rc:
    for _k, _a in _rc["atlas"].items():
        _at = g.make_cp_atlas(_a["w"], _a["h"], _a["frames"])
        _pintados = sum(1 for i in range(3, len(_at), 4) if _at[i])
        _cel = sum(f["aw"] * f["ah"] for f in _a["frames"])
        print(f"  RESULTADO   clear_{_k:6s} {_a['w']}x{_a['h']} "
              f"arquivo={len(_at):>9,} glifos={len(_a['frames'])} "
              f"pintados={_pintados} (celulas={_cel}) fator={_a['fator']}")
        ok &= (len(_at) == _a["w"] * _a["h"] * 4 and len(_a["frames"]) == 10
               and _pintados == _cel and _a["fator"] > 0)
    for _k, _t in _rc["texto"].items():
        _im = g.make_cp_img(_t["w"], _t["h"], 200)
        print(f"  RESULTADO   clear_{_k:6s} {_t['w']}x{_t['h']} "
              f"arquivo={len(_im):>9,}")
        ok &= (len(_im) == _t["w"] * _t["h"] * 4)
else:
    ok = False

# A IMAGEM DA DIFICULDADE (barra do topo): 640x360 sem arte, e cada uma com o
# tamanho EXATO que o gerador prometeu -- um byte a mais ou a menos e o
# `read_exact_path` falha no console.
_rtopo = g.resultado_topo()
ok &= (len(_rtopo) == 5)
_hashes = set()
for _i, (_a, _w, _h, _n) in enumerate(_rtopo):
    _im = g.make_dif_img(_w, _h, _i + 1)
    _op = sum(1 for _k in range(3, len(_im), 4) if _im[_k] > 64)
    _hashes.add(bytes(_im[:4096]))
    print(f"  RESULTADO   {_a:22s} {_w}x{_h} arquivo={len(_im):>9,} "
          f"opacos={_op:>6} ({_n})")
    ok &= (len(_im) == _w * _h * 4 and 0 < _op < _w * _h)
# As cinco tem de ser DIFERENTES: o teste prova que o arquivo da dificuldade
# jogada foi aberto, e nao um qualquer.
ok &= (len(_hashes) == 5)

# O NOME DA MUSICA: um bitmap por titulo, com o tamanho EXATO do plano, e todos
# diferentes entre si (o teste prova que o arquivo da musica jogada abriu).
_rn = g.resultado_nomes()
ok &= (len(_rn) == 27)
_hashes_nome = set()
for _i, (_a, _w, _h, _id) in enumerate(_rn):
    _im = g.make_nome_img(_w, _h, _i + 1)
    _op = sum(1 for _k in range(3, len(_im), 4) if _im[_k] > 64)
    _hashes_nome.add(bytes(_im[:2048]))
    print(f"  RESULTADO   {_a:30s} {_w}x{_h} arquivo={len(_im):>7,} "
          f"opacos={_op:>5} ({_id})")
    ok &= (len(_im) == _w * _h * 4 and 0 < _op < _w * _h)
ok &= (len(_hashes_nome) == 27)

bw, bh = g.resultado_folhas()[1:]
bgd = g.make_resultado_bg(bw, bh)
print(f"  RESULTADO   gradiente {bw}x{bh} arquivo={len(bgd):>9,}")
ok &= (len(bgd) == bw * bh * 4)

print()
print("TODOS EXATOS" if ok else "FALHOU")
