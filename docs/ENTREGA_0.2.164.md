# Entrega 0.2.164 — vocais, log e PROVA 4

Data: 2026-11-04
ELF do jogo: `FNF_PS2_0.2.164.ELF` — **6 575 512 B**, md5 `e330b14bf74bf892b046f30e30b4c70c`
PROVA 4: `PROVA4_PS2.ELF` — **1 679 852 B**
Cartão de prova: `tools/cartao_de_prova.py` → **9 itens, 0 fora do limite**
Harness de PC: `test/run_tests.sh` → **37 `ok`**, 1 falha **pré-existente** (orçamento de VRAM dos 5 palcos: `phillyBlazin`, `limoRide`, `mainStage`, `phillyStreets`, `sserafim`)

> Retificação: a mensagem do commit `f95dc79` diz "41 ok". A contagem certa é **37 ok + 1 falha pré-existente**; o número 41 estava errado e nunca foi medido.

---

## O que mudou desde a 0.2.163

### 1. O bug dos vocais — encontrado e corrigido

O gesto `SELECT + R3` faz **duas** coisas ao mesmo tempo no código:

* `main.c:12019` arranca a **captura de tela** (pausa o áudio ~0,5 s, copia o framebuffer, reabre o stem e reposiciona no mesmo sample);
* `main.c:9030` alternava `voices_enabled`.

A condição era `(pressed & PAD_R3) && !(g_held & PAD_SELECT)`. Quando SELECT e R3 chegam **no mesmo quadro** — que é exactamente o que acontece quando se carrega no chord de captura — `g_held` ainda não tem SELECT, a condição passa, e as vozes ficam **desligadas para o resto da sessão**. Foi isso que aconteceu no console: depois do SELECT+R3 no menu, a Week 2 e a Week 3 tocaram sem vocais, sem nada na tela a dizer porquê.

Correção:

* o R3 só alterna se SELECT não estiver carregado **nem agora nem no quadro anterior**;
* o novo estado vai para o log (`E,vozes_alternadas,...` na 0.2.164);
* enquanto as vozes estiverem desligadas aparece **"VOZES DESLIGADAS (R3 LIGA)"** no fundo do ecrã.

Consequência prática: com esta build os vocais **têm** de voltar. Se não voltarem, então o problema está dentro do `stems.pcm` — e é exactamente isso que a PROVA 4 mede (secção 2).

### 2. O log deixou de ser um arquivo só

O `FNF_COLHEITA.txt` **já não existe**. Cada despejo passa a ser um arquivo novo, numerado:

* `mass:/FNFPS2/FNF_LOG00.txt` … `FNF_LOG31.txt` (roda em módulo 32, os mais antigos são reescritos);
* se `mass:/FNFPS2/` não existir, tenta `mass:/`, depois `host:/`, depois `mc0:/`;
* gravação `O_TRUNC` + `write` + `fsync` + `close` — a mesma receita do `BOOT.txt`, que funcionou 401 vezes. **Nunca `O_APPEND`**: foi o append que matou as 0.2.161/0.2.162 no fim do quadro 0.

Frequência: **um despejo a cada 120 quadros (2 s)**. Custo medido pela PROVA 2: leitura de 0,70–0,75 MB/s; ~8 KB por despejo ⇒ ~11 ms a cada 2 s ⇒ **~0,5 %**. Os marcos de boot caíram de 400 para 60 (eram 80 s de E/S a 5 marcas/s).

Isto repõe o que faltava para medir o lag: as linhas **por quadro** (`F,...`) e os **eventos** (`E,...`), que a 0.2.163 já não registava. **Envia os `FNF_LOG*.txt`** — e, se quiseres o log completo de uma sessão longa, o `FNF_LOG` cujo número for o mais recente seguido dos anteriores.

### 3. PROVA 4 — as três perguntas que ficaram abertas

O ELF `PROVA4_PS2.ELF` corre sozinho e regrava `PROVA4.txt` (na raiz do pendrive **e** em `FNFPS2/`) a cada secção, com um ecrã a mostrar o progresso (barras verdes acesas = passos concluídos).

1. **A grade real da fonte.** Perfil de tinta por coluna (384) e por linha (192) do `font.rgba`, mais a contagem de corredores vazios para cada grade candidata (16/20/24/32 px) e a bbox das 8 primeiras células na grade 24×32 que o jogo assume. Se a grade real não for 16×6 de 24×32, os rótulos do menu saem como pedaços de dois glifos — que é exactamente o que a captura mostra.
2. **Se os vocais existem no arquivo.** Lê 3 posições (25 %, 50 %, 75 %) dos primeiros 6 `stems.pcm` e conta a fracção de amostras **não nulas** nos canais `Inst` (L+R) e `Vox` (L+R). `vox=0%` ⇒ o arquivo não tem vozes (falta de dado na geração). `vox>0%` ⇒ era só o toggle, e a 0.2.164 resolve.
3. **Se os palcos e personagens estão bem empacotados.** Abre **todos** os `.CFZ`/`.CHP` de `stages/` e `chars/`, e mostra magic, versão, nº de quadros e bytes por quadro. O que não for `CFZ4`/`CHP4` sai com os 4 bytes do magic à vista — sem adivinhas.

---

## Ordem de teste

1. Copiar `FNF_PS2_0.2.164.ELF` para o pendrive (como antes) e correr.
2. Jogar até à Week 2 e confirmar se os vocais estão lá **sem** carregar em SELECT+R3. Se carregares, vê se aparece o aviso "VOZES DESLIGADAS".
3. Sair e enviar os `FNF_LOG*.txt`.
4. Correr `PROVA4_PS2.ELF` (ocupa o lugar do jogo no pendrive; não precisa de mais nada) e enviar `PROVA4.txt`.

## Provado / hipótese / falta

* **Provado**: a 0.2.163 abre o jogo no console dele (6 576 360 B, md5 `4bd3f9b7…`, 3221 quadros de sessão, 0 falhas de DMA/GS registadas); o chord de captura e o toggle das vozes partilham o R3 (duas linhas de código, `9030` e `12019`); o append era o que matava o quadro 0.
* **Hipótese (a medir com a PROVA 4)**: a grade da fonte não é 16×6 de 24×32; os `stems.pcm` podem não ter canais de voz; os pacotes de palco/personagem podem não estar no formato que o leitor espera.
* **Falta**: o lag (as linhas `F,` do log novo dão o orçamento por subsistema); Week 2/3 pretas — a build seguinte decide com `palco_falha` no log; MERCH/OPTIONS/CREDITS; os traços do título; 5 palcos acima do orçamento de VRAM.
