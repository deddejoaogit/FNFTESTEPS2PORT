# 0.2.170 — análise da regressão da 0.2.169 + processo novo

O utilizador pediu: (1) causa da tela preta da 0.2.169 **com certeza**;
(2) processo novo (planear→provar→entregar); (3) referência do ps2sdk;
(4) revisão do código inteiro e fim dos bugs visíveis.

## PROVADO (não é opção, é facto medido/verificado na fonte)

1. **A memória NÃO é a causa da tela preta.** Mapa do linker da 0.2.170:
   `_end = 0x0061a2d8` (6,4 MB do ELF+BSS), pilha própria de 128 KB noutro
   sítio, 32 MB de RAM. O crescimento do buffer de telemetria (64K→192K)
   não colide com nada.
2. **O `PollSema` do EE é destrutivo** (ps2tek "Decrements the count variable";
   medida em hardware ps2dev 2005: decrementa mesmo a negativos). O
   `io_gate_trylock` antigo (`if(PollSema(io_sema)!=0) return 0;`) podia roubar
   o contador do portão (1→0→-1) e o `WaitSema` seguinte ficava **bloqueado
   para sempre** — incompatível com a vida do jogo. Corrigido: o portão
   (src/audio_ring.c) usa SÓ WaitSema/SignalSema + dono recursivo
   (GetThreadId); o trylock testa o dono em RAM e nunca toca no contador
   quando ocupado. O vigia escreve sem portão (é o último recurso dele).
3. **Ferramentas que liam um ficheiro que o jogo já não escreve.** O log de
   telemetria passou a chamar-se `FNF_LOG%02u.txt` (0.2.162) mas
   `check_stage_budget.py`, `run_tests.sh` e `analisa_log.py` ainda liam
   `FNF_COLHEITA.txt` — a porta dos testes ficava sempre vermelha num
   checkout limpo. Corrigido (aceita ambos; qualquer FNF_LOG*.txt).
4. **Bug de navegação/menu é pré-existente e está provado por controlo A/B**:
   no harness, o enredo de botões do teste (200:CROSS,420:CROSS,...) chega aos
   estados 6 e 7 mas **nunca ao estado 8** (tutorial) — igual com e sem as
   minhas mudanças. É o mesmo sintoma dos "botões bugados no main menu" do
   console. É o próximo alvo da revisão.
5. O teste de áudio do harness não compilava (shim sem `GetThreadId`) e a
   suite morria antes no `numpy` ausente — corrigidos (shim completo;
   numpy instalado no ambiente).

## HIPÓTESE (a causa da tela preta da 0.2.169 — ainda SEM prova)

- **Candidata principal: o portão da 0.2.169 com `PollSema`** (facto 2). Se o
  `PollSema` devolver valor != 0 no sucesso, o boot rouba o contador já no 2.º
  `boot_mark` e a primeira `WaitSema` (flush de telemetria / abertura
  protegida) fica presa para sempre = jogo não desenha mais = tela preta sem
  som. A outra via possível (primeiro `CreateSema` com campos não
  inicializados) está eliminada por construção neste portão novo.
- **O que falta para PROVAR**: o `BOOT.txt` + `FNF_LOG*`/`FNF_TRAVA` que o
  console escreveu NA SESSÃO PRETA (0.2.169). O `BOOT.txt` diz o último passo
  ("passo N: ...") completado — é a ferramenta certa para fechar a questão.

## FALTA (dados pedidos ao utilizador)

1. Do cartão da **sessão preta** (0.2.169): `BOOT.txt`, `FNF_LOG*.txt`,
   `FNF_TRAVA.txt` se existir. E destroelo? Não: mantenha-os.
2. Correr a **0.2.170** (corrigida) e devolver a pasta `FNFPS2` inteira
   (protocolo na mensagem). Se arrancar → causa fechada (era o portão).
   Se não arrancar → o BOOT.txt novo diz exactamente onde parou.

## Processo novo (pedido do utilizador)

1. **PLANO antes de código** (mensagem com o plano; só depois implementar).
2. **Uma causa por build** (regra 8); nada de misturas.
3. **Sem dado não se escolhe causa** (regra 4): a build seguinte COLHE.
4. **Revisão total do código** por módulos, com portas de teste por módulo:
   ver plano na mensagem de 2026-11-07.
5. **Referência obrigatória**: `docs/PS2SDK_REFERENCIA.md` (gerada dos headers
   reais; capítulo de semânticas verificadas). Nada de semânticas inventadas.
6. Terminar sempre com **provado / hipótese / falta** (regra 10).

## Estado do teste nesta build
- Compilação EE: 0 erros; suíte: 184+ ok; o teste de navegação (estado 8)
  continua vermelho — é o item 4 da lista de provados, alvo da revisão.
