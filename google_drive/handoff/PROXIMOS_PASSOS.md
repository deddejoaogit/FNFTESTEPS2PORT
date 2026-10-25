# PRÓXIMOS PASSOS — detalhado

Ordenado por prioridade. Cada item diz **o que já se sabe** (com fonte) e **o que
falta descobrir**. Nada aqui é palpite.

---

## P0 — Confirmar que a 0.2.111 funciona no console

**Por que é prioridade zero:** as builds 0.2.108, 0.2.109 e 0.2.110 não abriram.
A 0.2.111 corrige a causa raiz do log vazio (reabertura tardia do arquivo) e
adiciona marca por quadro. **Nenhuma dessas duas coisas foi confirmada em
hardware.** Até ser, nenhuma outra afirmação sobre o estado do projeto é segura.

**O que pedir ao usuário:** rodar a 0.2.111 e devolver dois arquivos:

- `mass:/FNFPS2/FNF_COLHEITA.txt` — se vier com conteúdo, o log funciona;
- `mass:/FNFPS2/BOOT.txt` — diz em qual quadro o jogo parou, se parou.

**Como interpretar:**

| `BOOT.txt` diz | Significa |
|---|---|
| `passo 30: F0_FRAMEBUFFER_SET` e depois `passo 31: FRAME,1,...` | boot completo, rodando |
| para num `passo N` de boot (META, PAD, CACHE, AUDIO, GS, TITLE) | travou no boot, no passo N |
| para num `passo N: FRAME,k,...` | travou no quadro k |
| arquivo não existe | o pendrive não montou — problema de install ou de `reset_iop()` |

---

## P1 — O menu desenha cada item duas vezes

**O que já se sabe, medido no log:**

No estado 6 (menu principal), um quadro tem **8 sprites**, não 4:

```
quadro 364 (estado 6): 8 sprites
  i= 1 x=  199.0 y= 181.0 w= 242.0 h= 57.0 ...
  i= 2 x=  224.0 y= 255.0 w= 193.0 h= 58.0 ...
  i= 3 x=  198.0 y= 332.0 w= 244.0 h= 52.0 ...
  i= 4 x=  128.0 y=   94.0 w= 384.0 h= 81.0 ...
  i= 1 x=  199.0 y= 181.0 w= 242.0 h= 57.0 ...   <- repetido
  i= 2 x=  224.0 y= 255.0 w= 193.0 h= 58.0 ...   <- repetido
  i= 3 x=  198.0 y= 332.0 w= 244.0 h= 52.0 ...   <- repetido
  i= 4 x=  128.0 y=   94.0 w= 384.0 h= 81.0 ...   <- repetido
```

A ordem é **1,2,3,4 / 1,2,3,4** — duas passagens completas, não duplicação por
item. E a geometria das duas cópias é **idêntica**.

**Por que importa:** dobra o alpha (os botões ficam com cor errada) e dobra as
chamadas de desenho do menu. É a melhor candidata a explicar a "linha estranha
abaixo dos botões" que o usuário reporta desde o início.

**O que já se sabe sobre o código:**

`src/main.c` tem **dois** blocos `if(ui_state==6)`:

- linha ~1582: desenha o **fundo** (parallax, `memcpy(g_up_bounce,...)`,
  `upload_raw`, `draw_clear`, `bind_texture`, `sprite_region_phys`).
- linha ~1654: desenha os **itens**
  (`for(order=0;order<5;order++){ mi=(order==4)?selection:(order>=selection?order+1:order); ... }`).

O laço de itens, por si só, **não duplica**: para `selection=0` gera
mi = 1,2,3,4,0 (cada um uma vez); para `selection=2` gera 0,1,3,4,2.

**O que falta descobrir:** onde vem a segunda passagem. Hipóteses a testar,
**nesta ordem**, e cada uma tem de ser provada antes de ser aceita:

1. O bloco de itens (linha ~1654) está dentro de um laço que corre duas vezes.
2. Há uma segunda chamada ao mesmo bloco em outro lugar do quadro.
3. O pacote é construído duas vezes por quadro (duas chamadas ao builder).

**Como investigar:** o log já dá a resposta. Contar sprites por quadro em cada
estado, e ver em qual estado a contagem é o dobro do esperado. Depois, colocar
um marcador no início e no fim do bloco de itens e ver quantas vezes cada um
aparece.

---

## P2 — Itens quebrados ao voltar do modo história

**O que já se sabe:** o usuário reporta desde a 0.2.105. O log registra `(y,h)`
de cada item do menu por quadro, então a comparação é mecânica.

**O que já se sabe sobre o código:** ao entrar no estado 6 vindo de uma
transição, há `if(ui_state==6){selection=g_menu_remembered; ...}` — a seleção é
restaurada, e um comentário no código diz que o original **snapa** a câmera para
o item lembrado em vez de varrer.

**O que falta:** comparar, no log, o layout do menu
(`[(y,h),...]` ordenado) na **primeira** visita ao menu com o das visitas
posteriores. `analisa_log.py` já faz essa comparação e acusa quando diferem.
Rodar num log que atravesse menu → story → menu e ver se acusa.

---

## P3 — Travamento ao entrar no modo história

**O que já se sabe:** o harness atravessa o jogo inteiro 12 vezes sem travar,
incluindo 5 ciclos de tutorial. Não é reproduzível localmente.

**O que falta:** um log do console travando. Com a marca por quadro dos
primeiros 300 quadros, o `BOOT.txt` vai dizer o quadro exato. Daí é isolar o
estado (`ui_state`) e a transição na linha `S,` anterior.

---

## P4 — Tutorial

**Sintomas reportados:** câmera travada no BF, nenhuma nota, nenhuma barra de
acerto, personagens não animando, música toca.

**O que já se sabe:** há código escrito para isso no repositório, não confirmado
em hardware. O estado 8 (tutorial) aparece 14.000 vezes no log do harness —
ou seja, é exercitado, mas os sintomas são visuais e o log não media animação
de personagem.

**O que falta:** confirmar em hardware, e se persistir, usar o log para medir.
O log tem `note_hits`, `note_misses`, `note_combo`, `note_score`,
`note_feedback` na linha `S,` — se as notas não aparecem, esses campos ficam em
zero e dizem se o chart carregou.

---

## P5 — Otimização (objetivo declarado do usuário)

O usuário pediu explicitamente **otimizar o máximo possível no console
original**. Frentes já identificadas:

- **O menu dobra as chamadas de desenho** (P1). Corrigir isso é otimização de
  graça.
- **O log escreve a cada quadro.** `telemetry_line()` já acumula, e o `fsync` é
  a cada 30 quadros. Se o custo ainda pesar, subir o intervalo.
- **VRAM:** `tools/vram_budget.py` existe para medir. Rodar e ver a margem.

---

## O que NÃO fazer

- **Não entregar build sem compilar limpo, rodar a suite e provar a correção.**
- **Não afirmar causa sem dado.** Três builds foram perdidas assim.
- **Não empilhar hipóteses.** Se uma build não funciona, a próxima **reverte** o
  suspeito antes de tentar outro.
- **Não confiar no harness para I/O, tempo ou sincronização.** Ele finge
  `write()` e `fsync()` como instantâneos.
- **Não remover o `LEIA-ME.txt` do pendrive** nem o marcador `BOOT.txt`.
