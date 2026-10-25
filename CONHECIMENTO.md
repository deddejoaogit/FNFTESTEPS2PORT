# CONHECIMENTO ACUMULADO — FNF PS2

Tudo que foi medido, descoberto e decidido neste projeto, com fonte. **Nada aqui
é estimativa**: cada número tem origem (linha de log, medição sobre arquivo real,
arquivo de código, ou fato documentado do ps2sdk).

Se você for continuar este projeto, **leia este arquivo antes de escrever uma
linha de código**. Ele contém meses de medição que não estão em nenhum outro
lugar.

---

## 1. Identidade do projeto

- **O que é:** porte do Friday Night Funkin' (versão web/Lime/OpenFL) para
  PlayStation 2, em C, usando o ps2sdk.
- **Repositório:** `grasshopperconta-a11y/PS2Projects`
- **Branch de trabalho:** `arena/4c50659b-ps2projectes` (fixa, não renomear)
- **Commit atual:** `7037d61`
- **Build atual:** `0.2.111-log-reabre`
- **Regras do projeto:** `REGRAS.md` — **leia e siga**. A regra 1 é: nada,
  nunca, jamais é inventado.

---

## 2. Links (Google Drive)

- **Pasta de entregas:** `1bhBZGS67lWeSzlDxK7TeNdJSc7_Z2VC1`
- **Pasta de assets (43 arquivos):** `1XEb6Q9tRfOZU5iGjnXfYyxFzpCicYfD3`
- **ELF 0.2.111 (entregue):** `1MZ4wEki3gQB5Hj9y7HSWE6x56HsnRmB1`
- **ELF 0.2.110:** `1k0WbjQBOlvPGBRcsssmvh4huwlMEuRXN`
- **ELF 0.2.109:** `158ACsUScHVnOela2AWYOA3uRg5I3mxfZ`
- **ELF 0.2.108:** `1tajPzIX2RSX-rBQIKFpIQnWjS5H7qbny`
- **Pendrive 0.2.109 (com LEIA-ME):** `1rxXQob8hVocx3x-3bmoHlam1NFi7C6-u`
- **Toolchain (43.841.008 B, tar.xz):** `1Va-8_k4GTxXpJaX4uMApV8OOpE9_QzJc`
  ⚠️ **Atenção ao ler este id:** o caractere antes do `Jc` é **J**, não Z.
  Um erro de digitação aqui custou tempo de trabalho.

Formato de link de download:
`https://drive.google.com/uc?export=download&id=<id>`

Todos os arquivos de entrega já estão `anyoneWithLink`, role `writer`.

---

## 3. Como compilar

```sh
# 1. extrair o toolchain (faz tudo: extrai, exporta PS2DEV, limpa no exit)
sh tools/with_toolchain.sh make -j4 FNF_VERSION=0.2.112-minha-versao \
    EE_BIN=build/FNF_PS2.ELF
```

`tools/with_toolchain.sh` procura `PS2DEV_EE_MIN_FNF.tar.xz` em
`/home/user/PS2Projects/google_drive/` e extrai para
`fnfps2/build/toolchain`. Se o arquivo não estiver lá, falha com
`nao achei PS2DEV_EE_MIN_FNF.tar.xz`.

**Build tem de sair sem nenhum warning.** O projeto compila com
`-Wall -Wextra -Werror=implicit-function-declaration`.

---

## 4. Como testar

```sh
sh test/run_tests.sh          # suite inteira (unitários + harness + negativos)
sh tools/run_harness.sh 1400  # só o harness, 1400 quadros
python3 tools/analisa_log.py  # analisa o log gerado
```

O que o harness cobre: 1401 quadros, jogo inteiro (título → menu → story →
tutorial, 5 ciclos), estouro de pacote, DMA acima de QWC_MAX, asset que não
abriu, escrita na zona de guarda, ASan+UBSan.

### ⚠️ O QUE O HARNESS NÃO COBRE — ISTO JÁ CUSTOU TRÊS BUILDS

`test/ps2harness/ps2h.c` **finge** a E/S do PS2:

| Função | No harness | No PS2 real |
|---|---|---|
| `open("mass:/...")` | interceptado, vira caminho local | RPC fileXio para o IOP |
| `write()` | `syscall` direto — **instantâneo** | RPC + escrita de bloco USB — **milissegundos** |
| `fsync()` | idem, instantâneo | **flush do USB, dezenas a centenas de ms** |
| `reset_iop()`, `SifExecModuleBuffer()`, `fileXioInit()`, `sbv_patch_*` | no-op | real |

**Consequência:** qualquer correção sobre I/O, tempo, sincronização ou hardware
**não pode ser verificada pelo harness**. Se você afirmar "verificado" depois de
rodar `run_tests.sh` sobre uma correção de E/S, você está inventando.

---

## 5. Estado atual — o que funciona e o que não

### Funcionando no console (build 0.2.106, última confirmada pelo usuário)

- Faixa visível 4:3 correta: **112 linhas pretas, conteúdo apenas nas linhas
  56..391** de 640×448. Antes desta correção a mesma auditoria media 2.567
  violações.
- Jogo abre, menu funciona, story funciona, tutorial toca música.

### NÃO resolvido (o usuário testou e confirmou em hardware)

1. **Linha estranha abaixo dos botões animados do menu principal.** Sintoma
   recorrente. **Descoberta do log:** o menu desenha **cada item 2× por quadro**
   (duas passagens completas, ordem 1,2,3,4 / 1,2,3,4). No FNF original cada
   item é desenhado uma vez. Isso dobra o alpha e dobra as chamadas de desenho.
   **Ainda não corrigido.**
2. **Itens do menu quebrados ao voltar do modo história.** Sintoma recorrente.
3. **Fundo do menu esticando em vez de rolar.** ✅ **CORRIGIDO** — ver §7.
4. **Travamento ao entrar no modo história.** Não reproduzido no harness.
5. **Tutorial:** câmera travada no BF, sem notas, sem barra de acerto,
   personagens não animando, música toca. Código escrito, não confirmado.
6. **O log vinha com 0 bytes.** ✅ **CORRIGIDO** — ver §8.

### ⚠️ "O jogo não abriu" — três builds quebradas, e o que se aprendeu

As builds **0.2.108, 0.2.109 e 0.2.110 não abriram no console**. Três causas
diferentes foram afirmadas com convicção e **nenhuma era a real na hora em que
foi dita**. A sequência real:

| Build | Causa real | Status |
|---|---|---|
| 0.2.108 | `telemetry_raw()` por sprite = ~20 escritas bloqueantes no pendrive **dentro** do laço de render | corrigido em 0.2.109 |
| 0.2.109 | `telemetry_flush()` com `fsync()` **em todo quadro** = 60 sincronizações de USB por segundo | corrigido em 0.2.110 |
| 0.2.110 | `telemetry_init()` desistia após 5 s, mas o USB leva **mais** de 5 s para re-enumerar depois de `reset_iop()` | corrigido em 0.2.111 |

**A lição que virou regra:** o sintoma "não abriu" tem seis significados
possíveis. Sem dado que os distinga, a build seguinte serve para **coletar**
dado, nunca para tentar um conserto.

---

## 6. O log — formato e como lê-lo

**Caminho no console:** `mass:/FNFPS2/FNF_COLHEITA.txt`
**Caminho no harness:** `fnfps2/build/harness_assets/FNFPS2/FNF_COLHEITA.txt`

O log é **acumulativo** (O_APPEND, sem truncar). Para colheita limpa, apague o
arquivo antes de testar.

### Tipos de linha

| Prefixo | Conteúdo |
|---|---|
| `#` | comentário/cabeçalho, inclui a legenda das colunas |
| `P,fase,linha,cod` | marco de fase (boot, troca de estado). **Escrito e sincronizado na hora.** |
| `S,` | estado completo da máquina de estados, um por quadro |
| `F,` | tempo de quadro (begin/draw/vsync, qwords, fps) |
| `D,` | **um sprite por linha** — a geometria que foi ao GS |
| `E,` | evento nomeado |
| `IO,` | operação de arquivo com resultado |

### Linha `D,` (a mais valiosa)

```
D,quadro,i,estado,x,y,w,h,u,v,uw,vh,alfa,vy0,vy1,vy_span,gsx0,gsy0,gsx1,gsy1
```

Índices (0-based, `p = linha.split(',')`):
`p[1]`=quadro `p[2]`=i `p[3]`=estado `p[4]`=x `p[5]`=y `p[6]`=w `p[7]`=h
`p[8]`=u `p[9]`=v `p[10]`=uw `p[11]`=vh `p[12]`=alfa `p[13]`=vy0 `p[14]`=vy1
`p[15]`=vy_span `p[16]`=gsx0 `p[17]`=gsy0 `p[18]`=gsx1 `p[19]`=gsy1

`x,y,w,h,u,v,uw,vh` são os valores **depois** do recorte da faixa.
`vy0/vy1` são os vértices que foram ao GS.

**Como detectar os defeitos só com o log:**

- **Sprite esticado:** `h/vh` (escala tela/textura) diferente da escala do
  sprite inteiro. A referência do projeto é **1.5894** (= 406.893/256). O fundo
  do menu aparecia com `h=335.00, vh=205.56` → escala **1.6297**, ou seja
  **+2.54% de esticamento**, todo quadro.
- **Sprite fora da faixa:** `vy0` ou `vy1` fora de 56..391.
- **Item deslocado:** comparar o `(y,h)` dos itens entre a primeira visita ao
  menu e as seguintes.
- **Travamento:** onde o log para. O último `P,` com `_BEGIN` sem `_DONE`
  correspondente depois dele.

### `BOOT.txt` — o marcador que sobrevive

`mass:/FNFPS2/BOOT.txt` (ou `mass:/BOOT.txt` se a pasta não existir). Uma linha,
reescrita com `fsync` a cada passo: `passo N: <FASE> code=<código>`.

Existe porque **log nenhum explica um boot que não acontece**: se o pendrive
nunca montou, não há log. Este arquivo diz o último passo completado. Para de
escrever após o boot, e marca **quadro a quadro nos primeiros 300 quadros**.

---

## 7. O bug do fundo esticado — corrigido e provado

**Causa:** aritmética de `band_clip_phys()` em `src/main.c`. A fração de baixo
era medida a partir do `y0` **já cortado** pelo primeiro `if`. Quando o sprite
cruza a faixa dos dois lados — o fundo do menu tem 406,893 linhas físicas contra
uma faixa de 335 — a fração sai pequena demais, o trecho de textura vem estreito
e o sprite estica.

**Prova, pelo log, mesma linha antes e depois:**

```
antes : h=335.00  v=6.33  vh=205.56  ->  escala 1.6297  (+2.54%)
depois: h=335.00  v=6.33  vh=210.77  ->  escala 1.5894  ( 0.00%)
```

**Correção:** as duas frações passaram a ser medidas a partir do `y0` original.

---

## 8. O log com 0 bytes — causa raiz e correção

**Causa raiz (descoberta pelos dados do console, não por hipótese):**

`reset_iop()` **desmonta o pendrive que o lançador montou**. Depois de
recarregar `usbd`/`usbhdfsd`, a re-enumeracao do USB levou **mais de 5
segundos**. O `telemetry_init()` esperava 5 s, desistia, e o log nunca era
aberto — todas as escritas seguintes viravam no-op silencioso.

**A prova de que o `mass:` aparece depois:** o `boot_mark()` abre o **próprio**
arquivo, não depende da máquina de telemetry, e escreveu o passo 1 normalmente —
porque quando ele rodou já tinham se passado os 6,5 s de espera do init e o
pendrive tinha montado. **O dispositivo aparece; o init é que desistia cedo
demais.**

**Correção (0.2.111):** a abertura do log virou função própria,
`log_try_open()`, e ganhou uma **segunda chance tardia**, chamada de
`telemetry_phase()` e de `telemetry_state()` — uma vez por quadro, no máximo 40
vezes. Se o pendrive montar depois do boot, o log começa a sair a partir do
quadro em que isso acontecer.

**Outras causas históricas do log vazio (já corrigidas, não reintroduzir):**

- `main.c` reabria o log com `O_TRUNC` logo depois do `telemetry_init()`,
  apagando o cabeçalho — e criava um segundo escritor no mesmo arquivo.
- `capture_status()` abria/escrevia/`fsync`/fechava o arquivo a cada chamada,
  enquanto a telemetria segurava outro descritor com buffer. Dois escritores.
- `dl_sprite()` era gated por `g_dl_gravando`, então nenhuma linha de sprite era
  registrada por padrão.

---

## 9. Fatos de hardware e ps2sdk (medidos, não presumidos)

### Vídeo

- Tela física **640×448** (NTSC), framebuffer PSMCT32, double buffer.
- `vy(y) = VY_OFFSET + y * VY_FATOR`, com
  `VY_FATOR = g_aspect_169 ? 448/360 : 14/15` e
  `VY_OFFSET = g_aspect_169 ? 0 : 56`.
- Faixa publicada em 4:3: `SCISSOR_TOPO/BASE = 56/391`. Em 16:9: `0/447`.
- O log do console confirma **4:3**.
- `draw_setup_environment` emite um GIF tag packed de 15 qwords, SCISSOR para o
  framebuffer inteiro, XYOFFSET (2048,2048), `PRMODECONT = PRIM_OVERRIDE_DISABLE`.
- `draw_rect_textured` escreve seu próprio GIF tag e PRIM, então o contexto do
  PRIM sempre casa com o argumento `context` do helper (o projeto passa 0).

### `texrect_t` — layout autoritativo (`ee/draw/include/draw2d.h`)

```
{ vertex_t v0; texel_t t0; vertex_t v1; texel_t t1; color_t color; }
```
Offsets em floats: `v0.x`=0 `v0.y`=1 `v0.z`=2, `t0.s`=4 `t0.t`=5,
`v1.x`=6 **`v1.y`=7** `v1.z`=8, `t1.s`=10 `t1.t`=11, `color`=12.
`vertex_t` = 12 B (z é `unsigned int`); `texel_t`/`color_t` = 8 B.
⚠️ Ler `v1.y` no offset 3 silenciosamente devolve `t0.s`.

### Geometria do menu principal

- Fundo: `bg_y = -37.978227f - (menu_cam_y - 360.0f) * 0.085f`, altura
  **435.956454**, x = **-64**, largura **768**.
- Itens: `camoff = (menu_cam_y - 360.0f) * 0.4f`,
  `cy = (40.0f + 160.0f * mi - camoff) * .5f`.
- Slots de `mi` medidos no log: y = 94, 181, 255, 332 (mi=0..3), com alturas
  81, 57, 58, 52 e larguras 384, 242, 193, 244.

### Assets

- `menu_cache.rle`: 1.623.445 B (o script avisa que esperava 1.587.561 — o
  bloco 4 de story titles foi acrescentado). Blocos: menu [0,1288182) →
  2.961.848; font [1288182,1326524) → 294.912; story ui [1326524,1398926) →
  524.288; story titles [1398926,1587561) → 306.180.
- **Formato do token RLE:** `c<128` → copia `(c+1)*4` bytes; `c>128` → repete o
  pixel de 4 bytes seguinte `257-c` vezes (**5 bytes consumidos**); `c==128` →
  erro.
- **Fonte VCR do story:** blob em `menu_sfx.pcm` offset **264.576**, 7.672 B →
  160×120 px, grade 16×6 de células 10×20, glifo n = ch−32; RGB forçado branco,
  alpha reescalado `(a*128+127)/255`; avanço **9.375**.
- **Atlas da fonte principal:** 384×192 RGBA32, grade 16×6, célula 24×32, glifo
  n = ch−32 para ASCII 32..127; avanço `g_font_widths[n] * 0.5`.
- **Atlas dos títulos do story** (`g_story_title[9][3]`, RGBA32, 306.180 B),
  offsets (x,y,w,h): (0,222,41) (36408,180,44) (68088,180,44) (99768,179,44)
  (131272,177,44) (162424,182,46) (195912,180,46) (229032,182,50)
  (265432,167,61).

### Formato da captura de tela

`mass:/FNFPS2/FNF_SCREEN_%04u.rgba.txt` = **RGBA32 puro**, 640×448,
**1.146.880 B** por arquivo, sem cabeçalho. Ler com
`d[(y*640+x)*4 .. +3]`. Fundo bege = RGB(253,231,113); letterbox = RGB(0,0,0).
Acionada por **SELECT+R3** simultâneos.

### Áudio

audsrv, pcm s16le estéreo 48000 Hz.

---

## 10. Ferramentas do repositório

| Arquivo | O que faz |
|---|---|
| `tools/with_toolchain.sh` | **a forma correta de compilar.** Extrai o toolchain, exporta PS2DEV, limpa no exit |
| `tools/run_harness.sh` | compila e roda o harness (jogo real + draw real do ps2sdk) |
| `test/run_tests.sh` | suite inteira |
| `tools/analisa_log.py` | **lê um log e responde sozinho** os quatro defeitos do console |
| `tools/build_pendrive.sh` | monta a pasta do pendrive + `LEIA-ME.txt` na raiz |
| `tools/fix_menu_cache_block4.py` | corrige o bloco 4 do `menu_cache.rle` |
| `tools/gen_harness_assets.py` | gera os assets sintéticos do harness |
| `tools/check_assets.py`, `check_real_assets.py`, `check_harness_assets.py` | validam assets |
| `tools/vram_budget.py`, `dump_stage_geometry.py` | orçamento de VRAM e geometria |

### `analisa_log.py` — use sempre

```sh
python3 tools/analisa_log.py [caminho/do/FNF_COLHEITA.txt]
```

Sem argumento, procura em `build/harness_assets/FNFPS2/FNF_COLHEITA.txt`.
Responde, sem ninguém descrever sintoma: (1) sprite que não passa pela faixa e
sprite que não é fundo nem item; (2) escala vertical de cada sprite contra a
referência 1.5894; (3) a cara do menu na primeira visita vs. as seguintes;
(4) onde o log parou e fase que começou sem fechar. Mais: falhas declaradas
(`_FAIL`) e sprites inteiramente fora da faixa.

Foi validado num log sintético com três defeitos injetados: pegou o
esticamento de 2,54%, o menu com cara diferente e o sprite fora da faixa.

---

## 11. Layout do pendrive (o jogo exige)

```
mass:/FNFPS2/assets/...          -> assets na raiz do pendrive
mass:/FNFPS2/assets/songs/<id>/<d>.chart
mass:/FNFPS2/FNF_COLHEITA.txt    (criado pelo jogo)
mass:/FNFPS2/BOOT.txt            (criado pelo jogo)
```

A pasta `FNFPS2` **tem de estar na raiz**. Se ficar
`mass:/FNFPS2/FNFPS2/...` o jogo não acha asset nenhum, chama `SleepThread()` e
dorme para sempre — tela preta, e o log cai na raiz em vez de dentro de
`FNFPS2/`. O ZIP de entrega traz `LEIA-ME.txt` na raiz explicando isso.

---

## 12. Erros que não se devem repetir

- **`git commit -F -` com heredoc falha silenciosamente.** Escreva a mensagem
  num arquivo com python e passe `-F /tmp/msg.txt`.
- **O connector do Google Drive usa parâmetros planos**, não um objeto `input`
  aninhado.
- **O id do toolchain tem um `J`, não um `Z`.** `1Va-8_k4GTxXpJaX4uMApV8OOpE9_QzJc`.
- **O ambiente de trabalho é resetado periodicamente.** `google_drive/` e
  `fnfps2/build/` desaparecem, e o histórico local do git volta ao commit
  inicial. **Sempre recupere com `git fetch` + `git reset --hard` no tip do
  remote, e salve arquivos em `/tmp` antes de resetar.**
- **`git reset --hard origin/<branch>` pode falhar** com "could not detach HEAD"
  por causa de arquivos untracked. Apague os arquivos conflituosos antes, ou use
  o SHA: `git reset --hard <sha>`.
- **Não confie em `git log origin/<branch>` depois de um reset** — o ref pode
  estar velho. Use `git ls-remote origin <branch>` para saber o tip real.
- **`rm -rf` num diretório de staging apaga documentos que você acabou de
  escrever nele.** Escreva os documentos no repositório, não em `/tmp`.
