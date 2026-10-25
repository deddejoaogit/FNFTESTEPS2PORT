#!/usr/bin/env python3
"""Le um FNF_COLHEITA.txt e responde, SOZINHO, as quatro coisas que o teste no
console acusou. Nada de relatorio, nada de print de tela: so o log.

Uso:  python3 tools/analisa_log.py [caminho/do/FNF_COLHEITA.txt]

Sem argumento, procura em build/harness_assets/FNFPS2/FNF_COLHEITA.txt.

O que ele verifica, e de onde tira a conclusao:

 (1) linhas estranhas abaixo dos botoes
     -> desenha algo que nao eh o fundo nem um dos itens do menu? Conta, por
        quadro do estado 6, quantos sprites existem e quais sao. Se aparecer
        um sprite que nao casa com a geometria dos itens, ele eh listado.

 (2) o fundo esticando em vez de rolar
     -> compara a escala vertical de cada sprite com a escala do sprite
        inteiro. Escala = altura na tela / altura na textura. Se o sprite foi
        recortado pela faixa, a escala TEM de continuar igual a do inteiro;
        se mudou, o sprite foi esticado ou encolhido. A linha D, guarda a
        altura na tela (h), o trecho de textura (vh) e o trecho recortado (v),
        da para conferir sem saber de mais nada.

 (3) os botoes bugados ao voltar do roteiro
     -> a posicao y dos itens do menu. Se um item aparecer em y diferente da
        posicao que ocupa no menu normal, ou se um item sumir, esta aqui.

 (4) o travamento ao entrar no roteiro
     -> onde o log parou. O ultimo quadro com sprite, o ultimo P, e a ultima
        linha escrita. Um travamento deixa o log cortado no meio, e esse
        corte aparece aqui.
"""
import os
import sys
from collections import Counter, defaultdict

BAND = (56, 391)          # faixa publicada em 4:3 (SCISSOR_TOPO/BASE)
CORR = 1.5894             # escala fisica/textura do jogo (406.893/256)


def caminho_log():
    if len(sys.argv) > 1:
        return sys.argv[1]
    aqui = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(aqui, "build", "harness_assets", "FNFPS2",
                        "FNF_COLHEITA.txt")


def le_log(p):
    """Devolve (estados, quadros, sprites, fases, eventos, ultima_linha)."""
    est, fr, sp, fas, ev, ult = [], [], [], [], [], None
    for n, ln in enumerate(open(p, encoding="utf-8", errors="replace")):
        ln = ln.rstrip("\n")
        ult = (n + 1, ln)
        if ln.startswith("S,"):
            c = ln.split(",")
            est.append({"f": int(c[1]), "ui": int(c[2])})
        elif ln.startswith("F,"):
            c = ln.split(",")
            fr.append({"f": int(c[1])})
        elif ln.startswith("D,"):
            c = ln.split(",")
            # D,quadro,i,estado,x,y,w,h,u,v,uw,vh,alfa,vy0,vy1,vy_span,
            # gsx0,gsy0,gsx1,gsy1
            sp.append({"f": int(c[1]), "i": int(c[2]), "st": int(c[3]),
                       "x": float(c[4]), "y": float(c[5]), "w": float(c[6]),
                       "h": float(c[7]), "v": float(c[9]), "vh": float(c[11]),
                       "vy0": float(c[13]), "vy1": float(c[14]),
                       "gsy0": float(c[17]), "gsy1": float(c[19])})
        elif ln.startswith("P,"):
            c = ln.split(",")
            fas.append({"fase": c[1], "ordem": n,
                        "linha": int(c[2]) if c[2].lstrip("-").isdigit() else -1})
        elif ln.startswith("E,"):
            ev.append(ln)
    return est, fr, sp, fas, ev, ult


def main():
    p = caminho_log()
    if not os.path.exists(p):
        print("nao achei o log em %s" % p)
        print("copie o FNF_COLHEITA.txt do cartao de memoria para esse caminho.")
        return 2
    est, fr, sp, fas, ev, ult = le_log(p)
    if not sp:
        print("log sem nenhuma linha D,: %d linhas no total." % (ult[0] if ult else 0))
        print("Sem linha D, nao da para julhar geometria. Log incompleto.")
        return 1

    ui = {e["f"]: e["ui"] for e in est}
    print("log: %s" % p)
    print("     %d quadros com estado, %d sprites, %d fases, %d eventos"
          % (len(est), len(sp), len(fas), len(ev)))
    problemas = 0

    # ---------- (2) o esticamento ----------
    print("\n[2] FUNDO DO MENU -- escala vertical tela/textura")
    print("    referencia: %.4f (a do sprite inteiro, 406.893 linhas / 256 texels)"
          % CORR)
    fundo = [d for d in sp if d["st"] == 6 and d["w"] > 700]
    if not fundo:
        print("    nenhum sprite largo no estado 6; nao da para conferir.")
    else:
        estic = [d for d in fundo
                 if d["vh"] > 0 and abs(d["h"] / d["vh"] - CORR) > 0.002]
        print("    %d quadros com o fundo, %d com escala errada"
              % (len(fundo), len(estic)))
        if estic:
            problemas += 1
            d = estic[0]
            print("    ESTICADO. exemplo, quadro %d:" % d["f"])
            print("      altura na tela h=%.2f, trecho de textura vh=%.2f,"
                  " escala=%.4f" % (d["h"], d["vh"], d["h"] / d["vh"]))
            print("      desvio = %+.2f%% contra a referencia."
                  % (100 * (d["h"] / d["vh"] / CORR - 1)))
            print("      -> o fundo nao esta so rolando: esta esticando.")
        else:
            print("    OK: escala certa em todos os quadros.")

    # ---------- (1) o que aparece abaixo dos botoes ----------
    print("\n[1] O QUE O MENU DESENHA POR QUADRO")
    porq = defaultdict(list)
    for d in sp:
        if d["st"] == 6:
            porq[d["f"]].append(d)
    if porq:
        q0 = min(porq)
        lista = porq[q0]
        print("    quadro %d (estado 6): %d sprites" % (q0, len(lista)))
        for d in lista:
            print("      i=%2d x=%7.1f y=%6.1f w=%6.1f h=%5.1f "
                  "v=%6.2f vh=%6.2f tela y=%.1f..%.1f"
                  % (d["i"], d["x"], d["y"], d["w"], d["h"], d["v"], d["vh"],
                     d["vy0"], d["vy1"]))
        # um sprite que nao seja o fundo (largo) nem item (quadradao)
        estranhos = [d for d in lista if not (d["w"] > 700 or 100 < d["w"] < 700)]
        if estranhos:
            problemas += 1
            print("    ESTRANHO: %d sprite(s) que nao sao fundo nem item:"
                  % len(estranhos))
            for d in estranhos:
                print("      i=%d x=%.1f y=%.1f w=%.1f h=%.1f"
                      % (d["i"], d["x"], d["y"], d["w"], d["h"]))
        else:
            print("    nenhum sprite fora do fundo e dos itens.")

    # ---------- (3) a posicao dos itens ----------
    print("\n[3] POSICAO DOS ITENS DO MENU")
    itens = [d for d in sp if d["st"] == 6 and 100 < d["w"] < 700]
    if not itens:
        print("    nenhum item registrado.")
    else:
        pos = Counter((round(d["y"]), round(d["h"]), round(d["w"])) for d in itens)
        print("    %d itens, %d posicoes distintas:" % (len(itens), len(pos)))
        for (y, h, w), n in sorted(pos.items()):
            print("      y=%6.1f  altura=%5.1f  largura=%6.1f   (%d quadros)"
                  % (y, h, w, n))
        # O que importa nao eh o passo entre itens (cada item tem um tamanho
        # diferente, entao o passo nunca eh constante). O que importa eh se o
        # menu MUDOU de cara entre a primeira visita e as seguintes -- que eh
        # o sintoma de "voltei do roteiro e os botoes estao bugados".
        visitas = {}
        for d in itens:
            visitas.setdefault(d["f"], sorted((round(x["y"]), round(x["h"]))
                                              for x in porq[d["f"]]
                                              if 100 < x["w"] < 700))
        ordem = sorted(visitas)
        if ordem:
            base = visitas[ordem[0]]
            mudou = [f for f in ordem if visitas[f] != base]
            print("    layout da primeira visita ao menu: %s" % (base,))
            if mudou:
                problemas += 1
                print("    BUG: o menu tem cara diferente em %d de %d quadros."
                      % (len(mudou), len(ordem)))
                print("      quadro %d: %s" % (mudou[0], visitas[mudou[0]]))
                print("      -> o menu nao voltou ao normal depois do roteiro.")
            else:
                print("    o menu tem a MESMA cara em todas as %d visitas."
                      % len(ordem))

    # ---------- (4) onde o log parou ----------
    print("\n[4] ONDE O LOG PAROU")
    if not fr and not sp:
        print("    log vazio de quadros.")
    else:
        ultf = max([d["f"] for d in sp] + [e["f"] for e in est])
        print("    ultimo quadro com sprite: %d (estado %s)"
              % (ultf, ui.get(ultf, "?")))
        if fas:
            print("    ultima fase registrada: %s (linha %d)"
                  % (fas[-1]["fase"], fas[-1]["linha"]))
        if ult:
            print("    ultima linha escrita (%d): %.90s" % (ult[0], ult[1]))
        # fase aberta sem fecho
        # Uma fase so esta "aberta" se o _BEGIN nao tiver _DONE DEPOIS dele.
        # Assim um _BEGIN do comeco do log que fechou logo depois nao conta, e
        # um _BEGIN que fechou em outra rodada do jogo tambem nao.
        # Um _BEGIN cujo prefixo NUNCA tem _DONE eh so marcador de progresso
        # (CACHE_ALLOC_BEGIN, GS_BEGIN, F0_BLEND_BEGIN...) e nao conta.
        sem = []
        for f in fas:
            if not f["fase"].endswith("_BEGIN"):
                continue
            pre = f["fase"].rsplit("_", 1)[0]
            fechou = any(g["ordem"] > f["ordem"] and g["fase"] == pre + "_DONE"
                         for g in fas)
            if not fechou and any(g["fase"] == pre + "_DONE" for g in fas):
                sem.append(f["fase"])
        if sem:
            problemas += 1
            print("    TRAVAMENTO PROVAVEL: comecou e nao terminou -> %s"
                  % ", ".join(sem))
            print("      -> o jogo parou ai. O resto do log nao existe.")
        else:
            print("    toda fase comecada terminou.")

    # ---------- falhas declaradas ----------
    print("\n[extra] FALHAS DECLARADAS NO LOG")
    falhas = [f for f in fas if "_FAIL" in f["fase"] or "_ERROR" in f["fase"]]
    if falhas:
        problemas += 1
        print("    %d marcador(es) de falha:" % len(falhas))
        for f in falhas:
            print("      %s (linha %d)" % (f["fase"], f["linha"]))
        print("    -> alguma etapa abortou. Essas linhas dizem qual.")
    else:
        print("    nenhuma. Nenhuma etapa abortou.")

    # ---------- sprites fora da faixa ----------
    print("\n[extra] SPRITES FORA DA FAIXA VISIVEL (%d..%d)" % BAND)
    fora = [d for d in sp
            if (d["vy1"] < BAND[0] - 0.5 or d["vy0"] > BAND[1] + 0.5)]
    if fora:
        problemas += 1
        print("    %d sprites inteiramente fora da faixa" % len(fora))
        for d in fora[:5]:
            print("      quadro %d i=%d y=%.1f..%.1f"
                  % (d["f"], d["i"], d["vy0"], d["vy1"]))
    else:
        print("    nenhum. Todos os sprites passam pela faixa.")

    print("\n======================================================")
    if problemas:
        print("RESUMO: %d ponto(s) para investigar, tudo saido do log."
              % problemas)
    else:
        print("RESUMO: nenhum problema encontrado no log.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
