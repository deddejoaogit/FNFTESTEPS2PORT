#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Extrai o MAXIMO de diagnostico de uma pasta do jogo (0.2.176 v6).

Corre em QUALQUER lado com Python 3 (PC, Mac, Linux):  python3 extrair_diagnostico.py PASTA
onde PASTA = a pasta FNFPS2 do pendrive (ou qualquer pasta com os FNF_*.txt).

Imprime um relatorio compacto (para colar no chat). Nao precisa de descrever
nada: o relatorio diz onde o jogo parou, se o produtor de audio vivia, se o
audsrv recebia, se o vigia escreveu, e onde esta' a janela cega.

v6 (0.2.176): (a) le o bloco # VALORES (os numeros pintados na TV, em texto);
(b) converte as capturas (FNF_SHOT/FNF_SCREEN) e a MINIATURA do FNF_TRAVA em
ficheiros PNG -- mande os PNGs no chat e eu vejo o ecra EXACTO, pixel a pixel.
"""
import sys, os, re, glob, zlib, struct

def achar(pasta, padrao):
    v = sorted(glob.glob(os.path.join(pasta, padrao)))
    return v

def ler(fn):
    try:
        with open(fn, 'rb') as f:
            return f.read().decode('latin-1', 'replace')
    except OSError:
        return ''

def png_gravar(path, w, h, rgba):
    """PNG RGBA8 sem dependencias (zlib/struct da biblioteca standard)."""
    def bloco(tag, dados):
        return (struct.pack('>I', len(dados)) + tag + dados +
                struct.pack('>I', zlib.crc32(tag + dados) & 0xffffffff))
    linhas = b''.join(b'\x00' + rgba[y*w*4:(y+1)*w*4] for y in range(h))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(bloco(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)))
        f.write(bloco(b'IDAT', zlib.compress(linhas, 9)))
        f.write(bloco(b'IEND', b''))

def converter_captura(fn):
    """FNF_SHOT (320x224 raw) / FNF_SCREEN (640x448 raw) -> PNG. Devolve (w,h,png) ou None."""
    n = os.path.getsize(fn)
    try:
        with open(fn, 'rb') as f:
            dados = f.read()
    except OSError:
        return None
    w = h = 0
    if n == 320*224*4:
        w, h = 320, 224
    elif n == 640*448*4:
        w, h = 640, 448
    else:
        return None
    png = fn.replace('.rgba.txt', '.png').replace('.txt', '.png')
    png_gravar(png, w, h, dados)
    return (w, h, png)

def extrair_miniatura(fn_trava):
    """Procura '# MINIATURA WxH f=N rgba32' em FNF_TRAVA e grava o PNG."""
    try:
        with open(fn_trava, 'rb') as f:
            dados = f.read()
    except OSError:
        return None
    m = re.search(br'# MINIATURA (\d+)x(\d+) f=(\d+) rgba32\n', dados)
    if not m:
        return None
    w, h, fr = int(m.group(1)), int(m.group(2)), int(m.group(3))
    ini = m.end()
    fim = ini + w*h*4
    if fim > len(dados):
        return None
    png = os.path.join(os.path.dirname(fn_trava),
                       'FNF_MINIATURA_f%06d.png' % fr)
    png_gravar(png, w, h, dados[ini:fim])
    return (w, h, fr, png)

def principal():
    pasta = sys.argv[1] if len(sys.argv) > 1 else '.'
    if not os.path.isdir(pasta):
        print('pasta invalida:', pasta); return 2
    L = []
    A = L.append
    A('==== EXTRATOR FNF PS2 ====')

    # 0. Pasta certa? (erro comum: correr na pasta das descarregamentos, onde
    # nao ha' logs nenhuns -- o relatorio sai vazio e parece que o jogo falhou.)
    if not (achar(pasta, 'FNF_SESSAO*.txt') or achar(pasta, 'BOOT*.txt')
            or achar(pasta, 'FNF_TRAVA*.txt') or achar(pasta, 'FNF_LOG*.txt')):
        A('')
        A('!!!! ESTA PASTA NAO TEM LOGS DO JOGO !!!!')
        A('O extractor tem de correr sobre a pasta FNFPS2 DO PENDRIVE (a que tem')
        A('FNF_LOG*.txt, FNF_TRAVA.txt, BOOT.txt dentro). Exemplo no Windows:')
        A('    python extrair_diagnostico.py E:\\FNFPS2')
        A('(onde E: e\' a letra do pendrive. Tambem funciona escrever')
        A(' `python extrair_diagnostico.py ` e ARRASTAR a pasta FNFPS2 para a')
        A(' janela do PowerShell, depois Enter.)')
        A('')
        A('Procurando pastas com logs perto de %s ...' % os.path.abspath(pasta))
        base = os.path.dirname(os.path.abspath(pasta))
        candidatos = []
        for raiz in (os.path.abspath(pasta), base):
            try:
                nomes = [raiz] + [os.path.join(raiz, n) for n in os.listdir(raiz)]
            except OSError:
                continue
            for n in nomes:
                if not os.path.isdir(n):
                    continue
                if (achar(n, 'FNF_LOG*.txt') or achar(n, 'FNF_TRAVA*.txt')
                        or achar(n, 'BOOT*.txt')):
                    candidatos.append(n)
        if candidatos:
            for c in sorted(set(candidatos))[:5]:
                A('  ENCONTRADO: %s' % c)
            A('Corre: python extrair_diagnostico.py "%s"' % sorted(set(candidatos))[0])
        else:
            A('  nada encontrado aqui. Interessa e\' a pasta FNFPS2 do PENDRIVE.')
        print('\n'.join(L))
        return 2

    # 1. Sessao
    sess = ler(achar(pasta, 'FNF_SESSAO*.txt')[-1]) if achar(pasta, 'FNF_SESSAO*.txt') else ''
    build = re.search(r'build=([^\s]+)', sess)
    A('build: %s' % (build.group(1) if build else 'DESCONHECIDA'))
    boot = ler(achar(pasta, 'BOOT*.txt')[-1]).strip() if achar(pasta, 'BOOT*.txt') else ''
    A('boot ultimo passo: %s' % (boot.replace('\n', ' | ')[:90] or 'nada'))

    # 2. Vigia
    trava = '\n'.join(ler(f) for f in achar(pasta, 'FNF_TRAVA*.txt'))
    vivo = 'VIGIA_VIVO' in trava
    marcas = [l for l in trava.splitlines() if l.startswith('# MARCADOR')]
    A('vigia: %s | marcador de paragem: %s | despejos: %d' %
      ('vivo' if vivo else 'MORTO (nunca escreveu)', 'SIM' if marcas else 'NAO', len(marcas)))
    if marcas:
        A('  ultima marca: %s' % marcas[-1].strip())

    # 3. Q-lines (todos os logs)
    qs = []
    logs = achar(pasta, 'FNF_LOG*.txt') + achar(pasta, 'FNF_TRAVA*.txt') + achar(pasta, 'FNF_SAVESTATE*.txt')
    for fn in logs:
        for l in ler(fn).splitlines():
            if l.startswith('Q,f='):
                d = dict(kv.split('=', 1) for kv in l[2:].split(',') if '=' in kv)
                qs.append(d)
    if qs:
        fs = [int(q.get('f', 0)) for q in qs]
        A('q-lines: %d | quadros cobertos: %d..%d | despejos de log: %d' %
          (len(qs), min(fs), max(fs), len(logs)))
        # estados
        cont = {}
        for q in qs:
            st = q.get('st', '?')
            cont[st] = cont.get(st, 0) + 1
        A('estados (amostrados): %s' % ', '.join('st=%s x%d' % kv for kv in sorted(cont.items())))
        # audio: primeiro/ultimo
        def num(q, k):
            try: return int(q.get(k, 0))
            except ValueError: return 0
        p0, p1 = qs[0], qs[-1]
        A('audio primeiro: q=%d r=%d un=%d so=%d lr=%d' %
          (num(p0, 'q'), num(p0, 'r'), num(p0, 'un'), num(p0, 'so'), num(p0, 'lr')))
        A('audio ultimo:   q=%d r=%d un=%d so=%d lr=%d' %
          (num(p1, 'q'), num(p1, 'r'), num(p1, 'un'), num(p1, 'so'), num(p1, 'lr')))
        A('  so= submissoes audsrv | lr= bytes lidos pelo produtor | un= underruns')
        lentos = [q for q in qs if num(q, 't') > 25000]
        if lentos:
            pior = max(lentos, key=lambda q: num(q, 't'))
            A('quadros lentos (>25ms): %d | pior: f=%s t=%s us' %
              (len(lentos), pior.get('f'), pior.get('t')))
        pads = set(q.get('pad', '') for q in qs)
        A('pad observado: %s' % ', '.join(sorted(pads)))
    else:
        A('q-lines: NENHUM (o log nao cobre nada!)')

    # 4. Anomalias
    for fn in achar(pasta, 'FNF_ANOM*.txt'):
        l = ler(fn).splitlines()
        if l:
            A('anomalia: %s' % l[0].strip()[:100])

    # 5. Eventos P e E (linha do tempo das marcas)
    evs = []
    for fn in logs:
        for l in ler(fn).splitlines():
            if l.startswith('P,') or l.startswith('E,'):
                evs.append(l)
    if evs:
        A('ultimas marcas:')
        for l in evs[-12:]:
            A('  %s' % l[:88])

    # 6. VALORES (a TV em texto) + Capturas -> PNG
    for fn in achar(pasta, 'FNF_TRAVA*.txt'):
        for l in ler(fn).splitlines():
            if l.startswith('# VALORES'):
                A('VALORES (TV): %s' % l[10:])
                A('  (por ordem: f=quadro st=estado beat=laco q=fila audsrv')
                A('   r=anel un=underruns lr=bytes lidos so=submissoes)')
    for fn in achar(pasta, 'FNF_SCREEN*.txt') + achar(pasta, 'FNF_SHOT*.txt'):
        sz = os.path.getsize(fn)
        if sz == 0:
            A('captura: %s (0 bytes)  <- VAZIA (a captura antiga; ver FNF_SHOT)' % os.path.basename(fn))
            continue
        r = converter_captura(fn)
        if r:
            A('captura: %s (%dx%d) -> %s  << ENVIE ESTE PNG' %
              (os.path.basename(fn), r[0], r[1], os.path.basename(r[2])))
        else:
            A('captura: %s (%d bytes, formato desconhecido)' % (os.path.basename(fn), sz))
    for fn in achar(pasta, 'FNF_TRAVA*.txt'):
        m = extrair_miniatura(fn)
        if m:
            A('MINIATURA do TRAVA (f=%d, %dx%d) -> %s  << ENVIE ESTE PNG' %
              (m[2], m[0], m[1], os.path.basename(m[3])))
    for fn in achar(pasta, 'FNF_SAVESTATE*.txt'):
        t = ler(fn)
        if t.strip():
            A('SAVESTATE: %s (%d bytes)' % (os.path.basename(fn), os.path.getsize(fn)))
            for l in t.splitlines()[:5]:
                if l.startswith('#'):
                    A('  %s' % l[:100])

    # 7. Conclusao automatica
    A('---- CONCLUSAO ----')
    if not vivo:
        A('* O vigia NUNCA escreveu: ou morreu cedo ou o USB ja estava morto no arranque.')
    if marcas:
        A('* O vigia VIU a paragem e escreveu o marcador: a morte esta no ficheiro FNF_TRAVA.')
    if qs:
        tem_campos = 'lr' in qs[-1] or 'so' in qs[-1]
        dlr = num(qs[-1], 'lr') - num(qs[0], 'lr')
        dso = num(qs[-1], 'so') - num(qs[0], 'so')
        if not tem_campos:
            A('* Log antigo (sem campos so/lr): nao da para julgar o audio por aqui.')
        if tem_campos and dlr == 0:
            A('* O produtor de audio NAO leu nada durante toda a janela: o audio nunca viveu.')
        if tem_campos and dso == 0 and num(qs[-1], 'un') >= 1:
            A('* Nenhuma submissao ao audsrv na janela: o consumidor esta morto.')
        if tem_campos and dlr > 0 and dso > 0:
            A('* Audio VIVO na janela: produtor leu %.1f KB, consumidor submeteu %d x ao audsrv.' %
              (dlr / 1024.0, dso))

    # 7b. Padrao de freeze: o freeze vem a seguir a um despejo?
    if qs:
        f_ult = num(qs[-1], 'f')
        # cada linha Q cobre um quadro; o despejo escreve as linhas NOVAS. Se as
        # linhas terminam em f_ult, o proximo despejo seria ~300 quadros depois.
        # O freeze com o USB ja morto deixa tudo parado em f_ult..f_ult+300.
        A('---- PADRAO DE FREEZE ----')
        A('* Ultimo quadro registado: f=%d (~%.1f s a 60 fps).' % (f_ult, f_ult / 60.0))
        A('* O freeze esta ENTRE f=%d e f=%d (janela do proximo despejo).' % (f_ult, f_ult + 300))
        A('* Se as linhas param logo depois de um despejo (f=%d %s), o despejo' %
          (f_ult, '≈ fim de ciclo 300: PADRAO CONFIRMADO' if (f_ult + 2) % 300 <= 12 or f_ult % 300 >= 288 else 'nao coincide com o ciclo 300'))
        A('*  (escrita grande no USB) + a leitura em fluxo do audio = o usbhdfsd trava.')
        A('* O numero branco pintado na TV diz o quadro exacto do freeze: FOTOGRAFE A TV.')
    # 7c. E/S morta com o jogo VIVO (assinatura da 0.2.174+): o produtor de
    # audio para (lr deixa de crescer) mas os quadros continuam (f avanca).
    if qs and ('lr' in qs[-1] or 'so' in qs[-1]):
        f_lr, last_lr = None, -1
        for l in qs:
            v = num(l, 'lr')
            if v > last_lr:
                last_lr = v
                f_lr = num(l, 'f')
        f_last = num(qs[-1], 'f')
        if f_lr is not None and f_last - f_lr > 300:
            A('* PRODUTOR PAROU em f=%d e o jogo CONTINOU ate f=%d (+%.1f s):' %
              (f_lr, f_last, (f_last - f_lr) / 60.0))
            A('*  a E/S morreu mas o quadro VIVEU (0.2.174+). Foto da faixa')
            A('*  vermelha na TV = os contadores no momento da morte.')
    # 7d. Assinatura da 0.2.175: os logs param (a escrita morreu) mas o JOGO
    # continuou vivo -- as marcas P,FRAME/BOOT.txt avancam alem do ultimo
    # q-line. E' o resultado ESPERADO quando o usbhdfsd morre sem congelar.
    if qs:
        f_ult2 = num(qs[-1], 'f')
        f_marca = f_ult2
        for l in evs:
            if l.startswith('P,FRAME,'):
                try:
                    f_marca = max(f_marca, int(l.split(',')[2]))
                except (IndexError, ValueError):
                    pass
        if f_marca > f_ult2 + 120:
            A('* ESCRITA MORTA, JOGO VIVO: as marcas chegam a f=%d mas os logs'
              % f_marca)
            A('*  param em f=%d (+%.1f s de jogo depois dos logs).' % (f_ult2, (f_marca-f_ult2)/60.0))
            A('*  A E/S morreu mas o quadro NAO congelou (0.2.175+). Foto da faixa = estado.')
    A("* Janela cega: o que falta esta ENTRE o ultimo q-line e o fim (ver quadros cobertos).")
    print('\n'.join(L))
    return 0

if __name__ == '__main__':
    sys.exit(principal())
