# Teste no console: o que fazer no PS2, o que o jogo grava e o que cada linha prova

Este documento existe porque a **regra 3** do `REGRAS.md` é dura e verdadeira:
o harness do PC prova que o jogo não estoura pacote, que o asset abre e que a
conta está certa — ele **não** prova I/O do `usbhdfsd`, tempo de leitura do
pendrive, sincronia de áudio com o GS nem o comportamento do `audsrv` no
hardware. Isso só o console diz. Aqui está o roteiro da primeira rodada, com a
linha de log que responde cada pergunta.

---

## 1. Antes de ligar: o pendrive

```
python3 tools/empacotar_stems.py --assets <clone funkin.assets>   # 27 stems, ~1,08 GB
python3 tools/empacotar_stems.py --conferir                       # tem de dar 27 de 27
sh tools/build_pendrive.sh                                        # monta build/pendrive
```

O `build_pendrive.sh` **não copia nada** se faltar pré-requisito: ele mede tudo
primeiro e imprime a lista inteira (assets de runtime, 46 `.chp4`, 21 palcos, 81
charts, 27 stems e o ELF) com o comando que resolve cada item. Se ele terminou,
o pendrive está completo.

Na raiz do pendrive tem de ficar:

```
FNF_PS2.ELF
FNFPS2/
  assets/
    <16 assets de runtime>.rgba/.pcm/.rle/.lz4f
    songs/<id>/stems.pcm            (27)
    songs/<id>/<dif>.chart          (81 charts, gravados em 4 layouts)
    chars/*.chp4                    (46)
    stages/<palco>/*.cfz4           (84 props)
    stages/sserafim/cut/*           (main/gf/bf .quads+.cfz4, end1/end2.cfz4)
```

## 2. O que o jogo grava sozinho

| Arquivo | Quando | Para que serve |
|---|---|---|
| `mass:/FNFPS2/FNF_COLHEITA.txt` | sempre, em fluxo | **o log**. Ver os formatos abaixo |
| `mass:/FNFPS2/FNF_SCREEN_%04u.rgba.txt` | SELECT+R3 | despejo cru 640×448, RGB 2 B por pixel — a tela sem filmar a TV |
| `mc0:/FNFPS2/SAVE.BIN` (ou `mass:`, com `FNF_CARD=1`) | fim de música / favoritar | save de 1 992 B com CRC |

Se o `FNF_COLHEITA.txt` **não** aparecer, o problema é antes do jogo: o ELF não
rodou (os `.irx` não subiram) ou o `FNFPS2/` não está na raiz. O próprio ELF
tenta `mass:/FNFPS2/FNF_COLHEITA.txt`, `mass:/FNF_COLHEITA.txt`, `host:` e
`mc0:/FNF_COLHEITA.txt`.

### Os cinco tipos de linha

| Prefixo | Formato | O que é |
|---|---|---|
| `#` | `# FNF PS2 TELEMETRIA CONTINUA build=<id>` / `# elf=<caminho>` / `# init ok caminho=<n> modulos_carregados=<n> falhas_modulo=<n> aberto_direto=<n>` | **o boot**. Se o `# init ok` está aqui, o log abriu e os módulos subiram |
| `F` | `F,<quadro>,<begin>,<draw_done>,<vsync_done>,<draw_ticks>,<wait_ticks>,<total>,<fps>.<centesimos>,<botoes>,<anim>,<quadro_anim>,<asset_frame>,<quads>,<qwords>,<estado>,<transicoes>,<audio_ouvido>,<audio_na_fila>,<anel>,<underruns>` | um por quadro: **fps, quadwords, underruns e estado** |
| `E` | `E,<ticks>,<nome>,<a>,<b>` | os eventos nomeados (as tabelas abaixo) |
| `P` | `P,<fase>,<linha>,<codigo>` | marcas de fase do boot (captura de tela, formatos) |
| `IO` | `IO,<op>,<caminho>,<resultado>,<detalhe>` | abertura/leitura de arquivo, com o caminho inteiro |
| `E-DROP` | — | eventos descartados por excesso de buffer (não deve aparecer) |

## 3. O que cada linha responde

### Boot e leitura do pendrive (a dúvida que derrubou a 0.2.108)

| Linha | Significa |
|---|---|
| `# init ok caminho=<n> …` | qual caminho de log funcionou (`n` = índice da lista de tentativas) |
| `E,…,chart_layout,<layout>,<status>` | **qual dos 4 layouts do chart abriu**. 0 = `songs/<id>/<d>.chart`; se vier 1, 2 ou 3, o layout 0 não abre no console e o jogo já resolve sozinho |
| `E,…,palco_pacote,<prop>,<rc>` | o `.cfz4` do prop abriu |
| `E,…,char_carregado,<papel>,<bytes>` / `char_pacote_ruim,<papel>,<rc>` | o `.chp4` abriu e decodificou / recusou |
| `IO,open,<caminho>,<fd>,<err>` | a abertura crua, quando o resultado não é óbvio |

### Áudio (a maior incógnita de tempo)

| Linha | Significa |
|---|---|
| `E,…,play_audio_path,<fd>,<0>` | abriu o `stems.pcm` **da música** (fd ≥ 0). Se vier `-1`, caiu na reserva |
| `E,…,play_audio_ausente,<0>,<nivel>` | **não achou** o stems daquela música: fica sem som |
| `E,…,tutorial_ring_start,0,<fd>` | o anel começou a ler o arquivo |
| `E,…,tutorial_audio_start,<quadro>,<amostras>` | o audsrv aceitou os primeiros bytes |
| `F,…,<underruns>` (último campo) | a fila do audsrv ficou vazia em algum quadro: **é I/O do pendrive, não o mixer**. Se este número subir, reduza a taxa de leitura, não o áudio |

### Cutscene e fim do `sserafim` (o caminho mais longo do jogo)

```
cut_cena,1,<amostras>              a cena começou
cut_fim,<amostras>,<quadro>        a cena acabou (730/24 s)
cut_libera,<amostras>,<0>          o chute final devolve as texturas
countdown_step,1..5,<quadro>       a contagem depois da cena
tutorial_audio_start,<quadro>,<amostras>
```

No fim da música:

```
cut_fim_textura,0,283504           end1 na VRAM
cut_endstuff,1,311808              o evento chegou (folga do pool)
cut_fim_textura,1,41040            end2 (a primeira já saiu)
cut_endstuff2,1,554240
cut_endstuff_apaga,595456,0
cut_endstuff_fim,432000,595456     9,000 s
cut_fim_quadros,237,240            as duas foram DESENHADAS (3,95 s e 4,00 s)
endstuff_endsong,<quadro>,<amostras>
state,9,<quadro>                   o Resultado abriu
```

Se `cut_fim_sem_vram` aparecer, a folga do pool no console é menor que a do
harness (o número vem no evento): a decisão é reduzir a arte da cena, nunca o
evento.

### Quadros, pacote e VRAM

| Linha | Significa |
|---|---|
| `F,…,<quads>,<qwords>,…` | quantos quads e quadwords o quadro gastou (o teto do DMA é `QWC_MAX`) |
| `F,…,<fps>.<centésimos>,…` | o ritmo real; abaixo de ~59 o gargalo é CPU/GS |
| `E,…,palco_sem_vram,<prop>,<folga>` | um prop não coube: não desenha |
| `E,…,char_sem_vram,<papel>,<folga>` | um personagem não coube |
| `E,…,custo_quadros,<n>,<upload de personagem>` | o resumo de custo (a mesma tabela que o sweep usa) |

## 4. As perguntas que só a rodada no console responde

1. **O `usbhdfsd` abre `songs/<id>/<d>.chart`?** (`chart_layout`) — no PC abre;
   na 0.2.x o console falhou nesse layout. Item nº 1.
2. **O anel de áudio sustenta 1,08 GB de stems em FAT32 sem underrun?**
   (último campo das linhas `F`).
3. **O `audsrv` toca 48 kHz estéreo por 3 minutos sem deriva?** comparar
   `tutorial_audio_end` com o `song_end_sample` do chart.
4. **O pico de VRAM da cutscene do `sserafim` cabe no GS de verdade?**
   (`cut_pool_min`, `cut_sem_vram`) — o harness mede o mesmo pool, mas não é o GS.
5. **O pad responde no meio do jogo?** (`F,…,<botoes>,…` tem de mudar quando se
   aperta).
6. **O `mc0:` grava o save de 1 992 B?** (`save_gravado,1992,0`).
7. **As cores estão certas?** o despejo `FNF_SCREEN_*.rgba.txt` responde sem
   filmar a TV.
8. **Os dois `.cfz4` do fim da música foram para a RAM no começo da cena?**
   O `end1` (283 504 B descomprimido) é lido do pendrive **no começo da
   cutscene**, comprimido (39 951 B), junto com o `end2` (10 102 B) — nunca nos
   últimos 9 s da música, quando o anel de áudio não pode parar. A prova no log
   é `cut_fim_pre,0,39951` e `cut_fim_pre,1,10102` no início da cena; se vier
   `cut_fim_pre_falha,<i>,<erro>`, o jogo cai no caminho antigo (lê no evento) e
   mesmo assim desenha. O que o console decide é **se o fallback foi usado** e
   **se a leitura dos 50 KB no começo da cena** atrasou algum pacote de palco
   (linhas `F` com `fps` baixo logo depois de `cut_cena,1`).

## 5. Como mandar o resultado

O que responde mais por byte: o `FNF_COLHEITA.txt` da primeira música
(`tutorial`) e o da `spaghetti` (que exercita cutscene, personagens do script,
piso em perspectiva, poeira, os dois sons do chute e a sequência do fim). Se o
arquivo passar de ~40 MB, mande só as linhas `E,`, `#` e `IO,` — o resto é
despejo de desenho.

Com esses dois arquivos, as oito perguntas acima deixam de ser hipótese.
