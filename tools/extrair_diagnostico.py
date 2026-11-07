#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Extrai o MAXIMO de diagnostico de uma pasta do jogo (0.2.180 v8).

Corre em QUALQUER lado com Python 3 (PC, Mac, Linux):  python3 extrair_diagnostico.py PASTA
onde PASTA = a pasta FNFPS2 do pendrive (ou qualquer pasta com os FNF_*).

v8 (0.2.180) le o NOVO layout de 5 txt + 2 bin com NOMES FIXOS:
    FNF_BOOT.TXT     slot 0 = ultima marca de fase; slot 4096 = anel de marcas
    FNF_LOG.TXT      anel de blocos "despejo %u" (Q-lines, eventos, anomalias)
    FNF_TRAVA.TXT    0=VIGIA_VIVO 512=MARCADOR/VALORES 4096=corpo do vigia
                     65536=CONGELADO 131072=traco AV
    FNF_TELA.TXT     0=RETRATO 8192=SESSAO 16384=FIM 24576=TV 32768=ESTADO
    FNF_ERRO.TXT     0=falha fatal 8192=aviso
    FNF_SHOT.rgba    captura SELECT+R3 (320x224 RGBA, sobrescrita)
    FNF_SAVESTATE.bin save state (texto, sobrescrito)
    FNF_LOGVIVO.bin  anel vivo (0.2.179, mantido)
E continua a ler o layout ANTIGO (FNF_LOG00..31, FNF_ANOM_*, BOOT.txt,
FNF_TRAVA-*, FNF_SCREEN_*), por isso serve para sessoes antigas tambem.
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

def ler_bytes(fn):
    try:
        with open(fn, 'rb') as f:
            return f.read()
    except OSError:
        return b''

def ler_slot(fn, off, tam=1 << 20):
    """Slot de um ficheiro pre-alocado (0.2.180): bytes a partir de `off`
    ate' ao primeiro NUL (cada escrita termina com NUL)."""
    try:
        with open(fn, 'rb') as f:
            f.seek(off)
            d = f.read(tam)
    except OSError:
        return ''
    i = d.find(b'\0')
    if i >= 0:
        d = d[:i]
    return d.decode('latin-1', 'replace')

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
    """FNF_SHOT.rgba (320x224) / FNF_SCREEN (640x448) -> PNG. Devolve (w,h,png) ou None."""
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
    png = fn.replace('.rgba.txt', '.png').replace('.rgba', '.png').replace('.txt', '.png')
    png_gravar(png, w, h, dados)
    return (w, h, png)

def reconstruir_log_anel(fn):
    """FNF_LOG.TXT (0.2.180): anel circular de blocos. Cada bloco comeca com
    '# FNF PS2 <build> | boot <8hex> | log v2 | despejo <n> | ... | t=<ms> ms'
    ou '# ANOMALIA: ... | boot <8hex> | ... | t=<ms> ms'. Os blocos reescrevem-se
    uns aos outros; a reconstrucao: agrupa por boot, fica com o boot mais
    recente (id maior) e ordena por t (ms desde o boot). Devolve o texto."""
    dados = ler_bytes(fn)
    if not dados:
        return '', {}
    marcas = []
    for m in re.finditer(br'# FNF PS2 [^\n]{0,200}?\| boot ([0-9a-fA-F]{8}) \| log v2 \| despejo (\d+)[^\n]*?\| t=(\d+) ms', dados):
        marcas.append((int(m.group(1), 16), int(m.group(3)), m.start(), 'D', int(m.group(2))))
    for m in re.finditer(br'# ANOMALIA: [^\n]{0,200}?\| boot ([0-9a-fA-F]{8})[^\n]*?\| t=(\d+) ms', dados):
        marcas.append((int(m.group(1), 16), int(m.group(2)), m.start(), 'A', -1))
    if not marcas:
        return '', {}
    boots = {}
    for b, t, off, kind, seq in marcas:
        boots.setdefault(b, []).append((t, off, kind, seq))
    bmax = max(boots)
    ests = {'blocos': len(boots[bmax]), 'boots': len(boots), 'despejos': 0, 'anomalias': 0}
    ordem = sorted(boots[bmax])
    fim = len(dados)
    partes = []
    # pontos de corte: cada bloco vai ate' ao proximo NUL ou ao proximo marcador
    limites = sorted(off for t, off, k, s in ordem) + [fim]
    for i, (t, off, kind, seq) in enumerate(ordem):
        if kind == 'D':
            ests['despejos'] += 1
        else:
            ests['anomalias'] += 1
        corte = limites[i + 1]
        b = dados[off:corte]
        nul = b.find(b'\0')
        if nul >= 0:
            b = b[:nul]
        partes.append(b.decode('latin-1', 'replace').rstrip())
    ests['boot'] = bmax
    return '\n'.join(partes), ests

def ler_logvivo(fn):
    """FNF_LOGVIVO.bin (0.2.179): anel circular de registos fixos de 256 bytes,
    cabecalho '[boot 8hex][seq 8hex]|'. Devolve a linha do tempo ordenada."""
    try:
        with open(fn, 'rb') as f:
            dados = f.read()
    except OSError:
        return []
    regs = []
    for i in range(0, len(dados), 256):
        rec = dados[i:i+256]
        if len(rec) < 256:
            break
        if rec[16:17] != b'|':
            continue
        try:
            boot = int(rec[0:8], 16)
            seq = int(rec[8:16], 16)
        except ValueError:
            continue
        payload = rec[17:].split(b'\0', 1)[0].decode('latin-1', 'replace')
        if payload:
            regs.append((boot, seq, payload))
    return regs

def extrair_miniatura(fn_trava):
    """Procura '# MINIATURA WxH f=N rgba32' em FNF_TRAVA e grava o PNG."""
    dados = ler_bytes(fn_trava)
    if not dados:
        return None
    m = re.search(br'# MINIATURA (\d+)x(\d+) f=(\d+) rgba32\n', dados)
    if not m:
        return None
    w, h, fr = int(m.group(1)), int(m.group(2)), int(m.group(3))
    ini = m.end()
    fim = ini + w*h*4
    if fim > len(dados):
        return None
    png = os.path.join(os.path.dirname(fn_trava) or '.',
                       'FNF_MINIATURA_f%06d.png' % fr)
    png_gravar(png, w, h, dados[ini:fim])
    return (w, h, fr, png)

def principal():
    pasta = sys.argv[1] if len(sys.argv) > 1 else '.'
    if not os.path.isdir(pasta):
        print('pasta invalida:', pasta); return 2
    L = []
    A = L.append
    A('==== EXTRATOR FNF PS2 v8 (0.2.180) ====')

    fn_boot = os.path.join(pasta, 'FNF_BOOT.TXT')
    fn_log = os.path.join(pasta, 'FNF_LOG.TXT')
    fn_trava = os.path.join(pasta, 'FNF_TRAVA.TXT')
    fn_tela = os.path.join(pasta, 'FNF_TELA.TXT')
    fn_erro = os.path.join(pasta, 'FNF_ERRO.TXT')
    fn_shot = os.path.join(pasta, 'FNF_SHOT.rgba')
    fn_state = os.path.join(pasta, 'FNF_SAVESTATE.bin')

    # 0. Pasta certa? (erro comum: correr na pasta das descarregamentos.)
    tem_novo = any(os.path.exists(p) for p in
                   (fn_boot, fn_log, fn_trava, fn_tela, fn_shot, fn_state))
    if not (tem_novo or achar(pasta, 'FNF_SESSAO*.txt') or achar(pasta, 'BOOT*.txt')
            or achar(pasta, 'FNF_TRAVA*.txt') or achar(pasta, 'FNF_LOG*.txt') or achar(pasta, 'FNF_LOGVIVO*')):
        A('')
        A('!!!! ESTA PASTA NAO TEM LOGS DO JOGO !!!!')
        A('O extractor tem de correr sobre a pasta FNFPS2 DO PENDRIVE (a que tem')
        A('FNF_LOG.TXT, FNF_TRAVA.TXT, FNF_BOOT.TXT dentro). Exemplo no Windows:')
        A('    python extrair_diagnostico.py E:\\FNFPS2')
        A('(onde E: e\' a letra do pendrive. Tambem funciona escrever')
        A(' `python extrair_diagnostico.py ` e ARRASTAR a pasta FNFPS2 para a')
        A(' janela do PowerShell, depois Enter.)')
        print('\n'.join(L))
        return 2

    # 1. Build + ultima marca de fase
    build = None
    ring_txt, ring_est = reconstruir_log_anel(fn_log) if os.path.exists(fn_log) else ('', {})
    if ring_est:
        m = re.search(r'# FNF PS2 (\S+)', ring_txt)
        if m:
            build = m.group(1)
    sess = ler_slot(fn_tela, 8192) if os.path.exists(fn_tela) else ''
    if not build:
        m = re.search(r'build=([^\s]+)', sess)
        if m:
            build = m.group(1)
    if not build and achar(pasta, 'FNF_SESSAO*.txt'):
        m = re.search(r'build=([^\s]+)', ler(achar(pasta, 'FNF_SESSAO*.txt')[-1]))
        if m:
            build = m.group(1)
    if not build:
        for fn in achar(pasta, 'FNF_TRAVA*.txt'):
            m = re.search(r'build=([^\s]+)', ler(fn))
            if m:
                build = m.group(1); break
    A('build: %s' % (build or 'DESCONHECIDA'))
    marca = ler_slot(fn_boot, 0).strip() if os.path.exists(fn_boot) else ''
    if not marca and achar(pasta, 'BOOT*.txt'):
        marca = ler(achar(pasta, 'BOOT*.txt')[-1]).strip()
    A('boot ultimo passo: %s' % (marca.replace('\n', ' | ')[:90] or 'nada'))

    # 2. Vigia (novo: slots do FNF_TRAVA.TXT; antigo: FNF_TRAVA*.txt)
    trava_txt = ''
    if os.path.exists(fn_trava):
        trava_txt = '\n'.join(x for x in (
            ler_slot(fn_trava, 0), ler_slot(fn_trava, 512),
            ler_slot(fn_trava, 4096), ler_slot(fn_trava, 65536)) if x)
    trava_legado = '\n'.join(ler(f) for f in achar(pasta, 'FNF_TRAVA*.txt'))
    trava = trava_txt + '\n' + trava_legado
    vivo = 'VIGIA_VIVO' in trava
    marcas = [l for l in trava.splitlines() if l.startswith('# MARCADOR')]
    A('vigia: %s | marcador de paragem: %s | despejos: %d' %
      ('vivo' if vivo else 'MORTO (nunca escreveu)', 'SIM' if marcas else 'NAO', len(marcas)))
    if marcas:
        A('  ultima marca: %s' % marcas[-1].strip())

    # 3. Q-lines (anel novo + logs antigos)
    textos = []
    if ring_txt:
        textos.append(ring_txt)
    for fn in achar(pasta, 'FNF_LOG*.txt'):
        if os.path.abspath(fn) == os.path.abspath(fn_log):
            continue  # o anel novo ja' foi reconstruido
        textos.append(ler(fn))
    textos.append(trava)
    for fn in achar(pasta, 'FNF_SAVESTATE*.txt') + ([fn_state] if os.path.exists(fn_state) else []):
        textos.append(ler(fn))
    qs = []
    for txt in textos:
        for l in txt.splitlines():
            if l.startswith('Q,f='):
                d = dict(kv.split('=', 1) for kv in l[2:].split(',') if '=' in kv)
                qs.append(d)
    evs = []
    for txt in textos:
        for l in txt.splitlines():
            if l.startswith('P,') or l.startswith('E,'):
                evs.append(l)
    def num(q, k):
        try: return int(q.get(k, 0))
        except ValueError: return 0
    if ring_est:
        A('anel FNF_LOG.TXT: %d blocos (%d despejos + %d anomalias) | boots vistos: %d' %
          (ring_est['blocos'], ring_est['despejos'], ring_est['anomalias'], ring_est['boots']))
    if qs:
        fs = [int(q.get('f', 0)) for q in qs]
        A('q-lines: %d | quadros cobertos: %d..%d' % (len(qs), min(fs), max(fs)))
        cont = {}
        for q in qs:
            st = q.get('st', '?')
            cont[st] = cont.get(st, 0) + 1
        A('estados (amostrados): %s' % ', '.join('st=%s x%d' % kv for kv in sorted(cont.items())))
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

    # 4. Anomalias (anel novo + antigos)
    anom_novo = [l for l in (ring_txt.splitlines() if ring_txt else [])
                 if l.startswith('# ANOMALIA')]
    for l in anom_novo:
        A('anomalia: %s' % l.strip()[:100])
    for fn in achar(pasta, 'FNF_ANOM*.txt'):
        l = ler(fn).splitlines()
        if l:
            A('anomalia: %s' % l[0].strip()[:100])

    # 5. Ultimas marcas (P/E)
    if evs:
        A('ultimas marcas:')
        for l in evs[-12:]:
            A('  %s' % l[:88])

    # 6. VALORES (a TV em texto) + Capturas -> PNG
    for l in trava.splitlines():
        if l.startswith('# VALORES'):
            A('VALORES (TV): %s' % l[10:])
            A('  (por ordem: f=quadro st=estado beat=laco q=fila audsrv')
            A('   r=anel un=underruns lr=bytes lidos so=submissoes)')
    caps = [fn for fn in (achar(pasta, 'FNF_SCREEN*.txt') + achar(pasta, 'FNF_SHOT*.txt'))
            if os.path.abspath(fn) != os.path.abspath(fn_shot)]
    if os.path.exists(fn_shot):
        caps.append(fn_shot)
    for fn in caps:
        sz = os.path.getsize(fn)
        if sz == 0:
            A('captura: %s (0 bytes)  <- VAZIA' % os.path.basename(fn))
            continue
        if os.path.basename(fn).startswith('FNF_SHOT') and sz == 286720 \
                and ler_bytes(fn)[:64] == b'\0' * 64:
            A('captura: %s (286720 bytes)  <- pre-alocada, SEM imagem (ninguem capturou ou falhou)' %
              os.path.basename(fn))
            continue
        r = converter_captura(fn)
        if r:
            A('captura: %s (%dx%d) -> %s  << ENVIE ESTE PNG' %
              (os.path.basename(fn), r[0], r[1], os.path.basename(r[2])))
        else:
            A('captura: %s (%d bytes, formato desconhecido)' % (os.path.basename(fn), sz))
    for fn in ([fn_trava] if os.path.exists(fn_trava) else []) + achar(pasta, 'FNF_TRAVA*.txt'):
        m = extrair_miniatura(fn)
        if m:
            A('MINIATURA do TRAVA (f=%d, %dx%d) -> %s  << ENVIE ESTE PNG' %
              (m[2], m[0], m[1], os.path.basename(m[3])))
    for fn in ([fn_state] if os.path.exists(fn_state) else []) + achar(pasta, 'FNF_SAVESTATE*.txt'):
        t = ler(fn)
        if t.strip():
            A('SAVESTATE: %s (%d bytes)' % (os.path.basename(fn), os.path.getsize(fn)))
            for l in t.splitlines()[:5]:
                if l.startswith('#'):
                    A('  %s' % l[:100])

    # 6b. LOG VIVO (0.2.179) -- a linha do tempo completa, sempre presente
    for fn in achar(pasta, 'FNF_LOGVIVO*'):
        regs = ler_logvivo(fn)
        if not regs:
            A('LOG VIVO: vazio ou ilegivel (%s)' % os.path.basename(fn))
            continue
        boots = {}
        for b, s, p in regs:
            boots.setdefault(b, []).append((s, p))
        bmax = max(boots, key=lambda b: len(boots[b]))
        linha = sorted(boots[bmax])
        A('---- LOG VIVO (%d registos da sessao mais recente) ----' % len(linha))
        fs = [int(m.group(1)) for s, p in linha
              for m in [re.search(r'f=(\d+)', p)] if m]
        if fs:
            A('  cobertura: f=%d..%d (~%.1f s de historia)' %
              (fs[0], fs[-1], (fs[-1] - fs[0]) / 60.0))
        evs2 = [p for s, p in linha if p.startswith(('EV=', 'BOOT'))]
        A('  eventos: %d (estados/accoes/erros/anomalias/capturas)' % (len(evs2) - 1))
        for p in evs2[:6]:
            A('    %s' % p[:96])
        if len(evs2) > 8:
            A('    ...')
        for p in evs2[-10:]:
            A('    %s' % p[:96])
        q2 = [p for s, p in linha if p.startswith('Q2 ')]
        if q2:
            A('  compactos de quadro: %d (2 Hz)' % len(q2))
            A('    primeiro: %s' % q2[0][:96])
            A('    ultimo:   %s' % q2[-1][:96])

    # 6c. FNF_TELA.TXT (0.2.180): retrato, sessao, fim, TV, estado
    if os.path.exists(fn_tela):
        for off, nome in ((0, 'RETRATO DO CONSOLE'), (8192, 'SESSAO'),
                          (16384, 'FIM'), (24576, 'VALORES DA TV'), (32768, 'ESTADO (SELECT+R1)')):
            t = ler_slot(fn_tela, off)
            if not t.strip():
                continue
            A('---- FNF_TELA / %s ----' % nome)
            linhas = t.splitlines()
            for l in linhas[:18]:
                A('  %s' % l[:100])
            if len(linhas) > 18:
                A('  ... (%d linhas)' % len(linhas))

    # 6d. FNF_ERRO.TXT (0.2.180)
    if os.path.exists(fn_erro):
        for off, nome in ((0, 'FALHA FATAL'), (8192, 'AVISO')):
            t = ler_slot(fn_erro, off)
            if t.strip():
                A('---- FNF_ERRO / %s ----' % nome)
                for l in t.splitlines()[:8]:
                    A('  %s' % l[:100])

    # 6e. Anel de marcas de fase (FNF_BOOT.TXT slot 4096)
    if os.path.exists(fn_boot):
        anel = ler_slot(fn_boot, 4096)
        if anel.strip():
            A('---- ANEL DE MARCAS (ultimas posicoes do laco) ----')
            for l in anel.splitlines()[-10:]:
                A('  %s' % l[:100])

    # 7. Conclusao automatica
    A('---- CONCLUSAO ----')
    if not vivo:
        A('* O vigia NUNCA escreveu: ou morreu cedo ou o USB ja estava morto no arranque.')
    if marcas:
        A('* O vigia VIU a paragem e escreveu o marcador: a morte esta no ficheiro FNF_TRAVA.')
    if marca and 'TUT_FONT_BEGIN' in marca and 'TUT_FONT_DONE' not in marca:
        A('* A ULTIMA MARCA E TUT_FONT_BEGIN: o freeze esta no intervalo TUT_FONT_BEGIN->DONE')
        A('*  (tutorial_vcr_restore + a escrita da marca seguinte). Ver CORRECOES #16.')
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

    # 7b. Padrao de freeze
    if qs:
        f_ult = num(qs[-1], 'f')
        A('---- PADRAO DE FREEZE ----')
        A('* Ultimo quadro registado: f=%d (~%.1f s a 60 fps).' % (f_ult, f_ult / 60.0))
        A('* O freeze esta ENTRE f=%d e f=%d (janela do proximo despejo).' % (f_ult, f_ult + 300))
        A('* O numero branco pintado na TV diz o quadro exacto do freeze: FOTOGRAFE A TV.')
    # 7c. E/S morta com o jogo VIVO
    if qs and ('lr' in qs[-1] or 'so' in qs[-1]):
        f_lr, last_lr = None, -1
        for l in qs:
            v = num(l, 'lr')
            if v > last_lr:
                last_lr = v
                f_lr = num(l, 'f')
        f_last = num(qs[-1], 'f')
        if f_lr is not None and f_last - f_lr > 300:
            A('* PRODUTOR PAROU em f=%d e o jogo CONTINOU ate f=%d (+%.1f s):' % (f_lr, f_last, (f_last - f_lr) / 60.0))
            A('*  a E/S morreu mas o quadro VIVEU (0.2.174+). Foto da faixa')
            A('*  vermelha na TV = os contadores no momento da morte.')
    # 7d. Escrita morta, jogo vivo
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
            A('* ESCRITA MORTA, JOGO VIVO: as marcas chegam a f=%d mas os logs' % f_marca)
            A('*  param em f=%d (+%.1f s de jogo depois dos logs).' % (f_ult2, (f_marca - f_ult2) / 60.0))
    A("* Janela cega: o que falta esta ENTRE o ultimo q-line e o fim (ver quadros cobertos).")
    print('\n'.join(L))
    return 0

if __name__ == '__main__':
    sys.exit(principal())
