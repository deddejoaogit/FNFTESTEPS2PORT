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

print()
print("TODOS EXATOS" if ok else "FALHOU")
