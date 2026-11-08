# REGISTO DE TUDO O QUE O UTILIZADOR CITOU (pedido explicito, 0.2.183)

| # | O que citou | Estado | Onde resolve |
|---|---|---|---|
| U1 | Tela preta no ARRANQUE (0.2.180) | RESOLVIDO 0.2.181 | CORRECOES #18 |
| U2 | "Tela bugada com numeros" = pintura do vigia | RESOLVIDO 0.2.182 | CORRECOES #20 (non-destructive) |
| U3 | Texturas/personagens a DESAPARECER no tutorial | RESOLVIDO 0.2.182 | CORRECOES #20 (o vigia destruia o GS) |
| U4 | Transicoes de menu travadas (3-8,4 s) | RESOLVIDO 0.2.182 | CORRECOES #20 (escritas do log no laco) |
| U5 | Barra de vida parada, SEM icones | RESOLVIDO 0.2.182 (VRAM morta) | CORRECOES #20; icones em teste |
| U6 | SFX do clique tarde | RESOLVIDO 0.2.182 (directo no canal) | CORRECOES #20 |
| U7 | Sem musica no tutorial | RESOLVIDO 0.2.182 (produtor EV=) | CORRECOES #20; ver U10 |
| U8 | Tela preta FINAL (freeze) | ABERTO | CORRECOES #21 |
| U9 | Story menu visualmente bugado | ABERTO (trabalho visual) | ver FASE 3 |
| U10 | **Freeplay menu BUGADO -- por isso JOGA SEMPRE PELO STORY MODE** | ABERTO | FASE 3; NOTA: testar sempre pelo STORY |
| U11 | Entrar no TUTORIAL = tela de numeros + sem som + trava | RESOLVIDO 0.2.183/184 | CORRECOES #21/22 ||
| U12 | Botoes do menu nao funcionam (pad=0000) | ABERTO (causa separada) | CORRECOES #12 |
| U13 | Laco bugado/traço sob os botoes | ABERTO (visual) | FASE 3 |
| U14 | Game over / vitoria | POR IMPLEMENTAR | FASE 1-2 |
| U15 | Icones dos personagens no HUD | POR IMPLEMENTAR | FASE 2 |
| U16 | fps | POR MEDIR | FASE 3 |
| U17 | Logs = 4-5 txt de nome fixo | FEITO 0.2.180 | layouts |
| U18 | Logs DETETAR TUDO mas sem nunca estragar o jogo | FEITO 0.2.182/183 | vigia non-destructive |
| U19 | Extrator no fim de TODA a resposta | EM VIGOR | extrair_diagnostico.py |
| U20 | "diga exatamente o que devo testar" no fim | EM VIGOR | respostas |
| U21 | Nenhum evento do original pode ser removido/encurtado | REGRA | REGRAS.md |
| U22 | Documentos CORRECOES/IMPLEMENTAR actualizados | EM VIGOR | estes |
| U23 | Nao prometer 100% | REGRA | respostas |
| U24 | "Poderia remover esta tela?" (tela de numeros do vigia) | FEITO 0.2.184 | CORRECOES #22 |
| U25 | Personagens/icones/textos em falta no tutorial | ABERTO (carga falha) | CORRECOES #23 |
| U26 | Ecrã de resultados BUGADO + trava no fim | ABERTO (GS com dados lixo) | CORRECOES #23 |
| U27 | Sem MUSICA no tutorial | RESOLVIDO 0.2.184/185 (som TOCOU) | CORRECOES #23 |
| U28 | Tela preta do tutorial + "textura completamente bugada" no fade | ABERTO (cargas falham no stop do switch) | CORRECOES #23 |
| U29 | "nao vejo todas as imagens" (SELECT+R3 sem imagens) | CORRIGIDO 0.2.185 (capturas pausadas) | CORRECOES #23 |
| U30 | "nao consigo mandar os .bin" | RESPOSTA: nao precisa — o extrator le-os no PC | extrair_diagnostico.py |

**NOTA DE TESTE (U10/U11)**: o utilizador joga STORY MODE (freeplay esta' bugado).
O tutorial testado e' o do STORY MODE. Toda a instrucao de teste comeca assim.

# O QUE FALTA IMPLEMENTAR — FNF PS2 (actualizado na 0.2.185)

**ESTADO ACTUAL**: 0.2.185 "O SWITCH NAO PODE MATAR O PRODUTOR NEM O DRIVER".
Teste 0.2.184 PROVOU: o SOM TOCOU no tutorial/resultados (1.ª vez), o jogo
sobreviveu ao engasgo, os resultados apareceram animados. A cadeia causal da
tela preta está medida e corrigida (CORRECOES #23): stop do anel com
TerminateThread sobre read pendente → close colide → driver encaixa → iow
segura o portão → cargas todas falham. 0.2.185: pausa-primeiro no stop, sem
meta-ops nos fds novos, iow não escreve com o USB doente, capturas com a
janela pausada (as imagens do SELECT+R3 DEVEM sair).
**ABERTOS**: tela preta do tutorial (confirmar se as cargas passam a pegar),
resultados sem personagens + freeze no fim (GS com dados lixo), personagens/
ícones/textos, pad=0000 nos registos (input funciona — proibido pelos menus),
transições/SFX, game over, vitória, fps, freeplay (joga-se pelo STORY).

Documento pedido pelo utilizador: **absolutamente tudo o que falta implementar
no jogo**, dividido por secções/fases (menus, gameplay, etc.). Cada entrada diz
O QUE falta, a FONTE (original Haxe/flixel v0.8.6 em `/tmp/funk86`, pedido do
utilizador, ou dado medido) e o ESTADO.

Convenções: `[ ]` falta · `[x]` feito e provado · `[~]` feito mas com defeito
(ver `CORRECOES.md`) · `[?]` não verificado ainda.

**Estado geral (0.2.178):** ~92 % do código existe; joga-se o título, main menu,
freeplay e story menu com música; entra-se em gameplay. O que falta é sobretudo
APARÊNCIA/COMPORTAMENTO fiel ao original e as telas finais (game over, vitória).

---

## 1. TÍTULO (title state)
- [x] Logo animada (beat), gf a dançar, "Press Enter to Begin" a piscar.
- [~] Transição título → main menu **diferente do original** (citada pelo
      utilizador na 0.2.177; ver CORRECOES #1-#2).
- [ ] SFX de confirmação no TIMING certo (hoje toca DEPOIS de entrar no ecrã —
      CORRECOES #1).

## 2. MENU PRINCIPAL (main menu)
- [ ] **Botões do menu** — `pad=0000` registado nos logs: os botões não chegam
      aos registos (CAUSA SEPARADA — o pad em si; investigar leitura/mapeamento).
- [ ] Ícones dos botões (story mode / freeplay / credits…) — não verificados.
- [ ] Animação de hover/selecção dos botões.
- [ ] Ecrã de créditos (existe no original).

## 3. FREEPLAY
- [~] Entrada/saída com transições diferentes do original (citada).
- [ ] Lista de músicas completa com scroll fiel (nomes, semanas, difficulty).
- [ ] Ícones de health na lista; score guardado por música (save).
- [ ] Preview de áudio ao navegar (a janela prev_ini/prev_fim existe no
      audio_ring — falta ligar à navegação como o original).

## 4. STORY MODE (selecção de semana)
- [~] **Textos simples (sem fonte especial) BUGADOS** — citados pelo utilizador
      na 0.2.177 (CORRECOES #4). Os títulos com fonte especial parecem OK.
- [~] **Títulos + texto INVERTIDOS ao voltar do freeplay** (0.2.179,
      UTILIZADOR: "os botões do story menu estavam de novo bugados após
      entrar no free mode e depois no story menu"). Prova visual em
      `evidencia-console/0.2.179_story_flip.png`. Repro: título → story (OK)
      → freeplay → story (BUGADO). Próxima causa visual a atacar; o
      `EV=STORY_RESTORE` do LOG VIVO traz as dims para provar a causa.
- [ ] Dificuldade (easy/normal/hard) com tween (o código tem story_diff_tween).
- [ ] Animação de entrada da semana; setas; animação da gf/bf do story menu.

## 5. GAMEPLAY (a música)
- [x] Notas, juízo (sick/good/bad/shit), combo, score, health bar.
- [~] **Ícones de health** (bf/gf/dad) — não verificados visualmente.
- [ ] **Splashes** de note hit (o motor splash existe — falta confirmar em jogo).
- [ ] **A "linha bugada"** (citada em sessões anteriores) — desenho da strumline.
- [ ] Contador de **FPS** (não existe; o original mostra? — verificar e medir).
- [ ] Eventos de palco (câmaras, shake, props) — o motor stage_script existe;
      faltam os scripts das músicas reais conforme o original.
- [ ] Pause menu (o código tem g_paused; falta o menu completo com as opções).

## 6. GAME OVER
- [ ] **Tela de game over completa** (bf morto, texto, opções) — `gameover_dados.h`
      existe; falta o ecrã, os sons e o comportamento do original.
- [ ] Retry / Back to menu com os SFX certos.

## 7. VITÓRIA / RESULTADOS
- [ ] **Tela de resultados** (tally, rank, score) — `resultado*.c/h` e
      `resultado_rank*` existem (motores prontos); falta a ligação ao fim da
      música e a fidelidade visual.

## 8. ÁUDIO
- [x] Música em fluxo (stems), loop do título (provado: lr=119 MB na 0.2.177),
      anel de 1 MB, underruns controlados.
- [ ] SFX de UI todos mapeados (click tarde = CORRECOES #1).
- [ ] Mixing de stems (o motor audio_mix existe — verificar volumes por voz).
- [ ] Sons de game over / vitória.

## 9. UI / INTERFACE GERAL
- [ ] **Fontes**: os textos "simples" (sem fonte especial) estão bugados no
      story menu (CORRECOES #4) — rever o caminho de desenho dos glifos simples.
- [ ] Transições fiéis em TODOS os caminhos (título→menu, freeplay, story) —
      ver CORRECOES #2-#3.
- [ ] FPS overlay (ver secção 5).
- [ ] Modo 16:9 / viewport (o código tem aspect169/viewport360 — confirmar).

## 10. SAVE / PERSISTÊNCIA
- [ ] Guardar score/highscore por música e por semana (engine/save.c existe).
- [ ] Guardar opções (volume, etc.).

## 11. DIAGNÓSTICO / LOGS (não é jogo, mas é o nosso olho)
- [x] VALORES da TV no txt (provado na 0.2.177).
- [x] Extrador v8 com PNG + LOG VIVO + slots do layout novo (e layout antigo).
- [x] **LOG VIVO** (0.2.179): anel circular pré-alocado — SEMPRE regista
      transições, acções, erros, anomalias, capturas e o quadro a 2 Hz.
- [x] **Captura "save state gigante"** (0.2.179/180): `FNF_SAVESTATE.bin`
      (visual/desempenho/sonora/pad/marcas/Q) + imagem `FNF_SHOT.rgba`
      320×224, escrita pela thread escritora sobre clusters pré-alocados.
- [x] Amostras Q no corpo do TRAVA (0.2.178).
- [x] **5 TXT de nomes fixos + 2 bin (0.2.180/181, pedido do utilizador):**
      `FNF_BOOT.TXT` (marca+anel de marcas), `FNF_LOG.TXT` (anel de despejos
      Q/eventos/anomalias, 512 KB), `FNF_TRAVA.TXT` (slots VIGIA/MARCADOR/
      corpo/CONGELADO/AV), `FNF_TELA.TXT` (slots RETRATO/SESSAO/FIM/TV/
      ESTADO, 128 KB), `FNF_ERRO.TXT` (fatal/aviso) + `FNF_LOGVIVO.bin` +
      `FNF_SHOT.rgba` + `FNF_SAVESTATE.bin`. Fim dos 32+12 ficheiros
      rotativos. Pré-alocação = padrão provado (O_TRUNC + zeros até ≤512 KB)
      com marca `PREALLOC <nome>` por ficheiro; em jogo só `lseek+write`+
      `close` (CORRECOES #16/#17/#18).

## 12. CAUSAS SEPARADAS CONHECIDAS (não são "features")
- **pad=0000**: os registos nunca vêm bits do pad — causa própria, à parte de
  tudo o resto (o jogo lê o pad? o extractor mostra 0000 sempre).
- **usbhdfsd**: as operações de CRIAÇÃO/EXTENSÃO de ficheiros morrem depois de
  minutos de fluxo de áudio (leituras continuam boas). Regra de ouro: os logs
  vivem em RAM e escrevem no TRAVA; pré-criar ficheiros no boot (0.2.178).
- **FNF_SCREEN** = código morto (0 bytes para sempre) — remover ou ignorar.

## 13. ESTADO 0.2.186 (campanha 08/11/2026 — os 3 bugs citados)
- [~] **Música engasga com fps baixo** — Fix A feito (bomba em thread dedicada,
  audsrv thread-safe verificado no ps2sdk); harness ×2 + ASan ×3 verdes;
  música reproduz no harness. Falta a confirmação na consola.
- [~] **Personagens/eventos não aparecem** — Fix B feito (descodificação v3 dos
  eventos FOCUS/ZOOM/SCROLL/BOP/HEALTH_ICON em `chart_events.c`; 24/24 testes
  de unidade). Falta a confirmação na consola (jogar e ver os efeitos).
- [~] **Freeplay completamente bugado** — `freeplay_dados.h` == plan.json
  (campo a campo, verificado); os harnesses freeplay/gameplay/resultado_rank
  existem mas dependem do canal de log do harness (ver #24.3) para validar —
  causa visual EM ABERTO.
- [ ] **Canal de telemetria→ficheiro do harness para ao quadro 1-584** (causa
  19, CORRECOES #24.3): o jogo corre os 1 402 quadros, mas os despejos Q/E
  morrem cedo — é o próximo alvo (marcação fina no iow + re-arm do
  `g_fn_write_ok`), e é a MESMA classe do "FNF_LOG parado" da consola.
- [ ] Janela do accept do menu = 82 quadros (16 toggles×3,6 + 24 de
  `menu_exit_frames`) — fiel ao original; os roteiros do harness já contam com
  isto; nada a corrigir, só documentado.

## 14. ESTADO 0.2.187 (campanha 08/11/2026 — TODOS os bugs do teste 0.2.186)

FEITO (ver CORRECOES #25 para as provas):
- [x] **SFX instantâneos (vozes ADPCM/SPU2)** — 7 samples (5 `assets/result/*.pcm`
  + slices scroll/confirm do menu_sfx) codificados no boot para ADPCM
  (header 16 B do ps2sdk, pitch 4551); fallback para o mix no anel se o load
  falhar (`SFX_ADPCM 0` desliga tudo). Risco: área SPU2 0x5010 vs streaming
  (comentário do ps2sdk) — TESTAR na consola: a música não pode "estravar"
  quando os sons tocam.
- [x] **Freeze story menu fechado** — RPCs do audsrv para a thread da bomba
  (`g_arpc_vara/varb`); o loop nunca mais bloqueia no IOP.
- [x] **FPS do main menu** — cache dos uploads da animação (só frames mudados).
- [x] **Textos** — avanços proporcionais (VCR 0.625, HUD 0.3125).
- [x] **Capturas ilimitadas** — 8 slots rotativos pré-alocados (7 esparsos),
  guarda de carga, escrita por fatias de 20 KB, extractor v9.4.
- [x] **Etiquetagem** — VERSION escrito ANTES do build (0.2.187 embutido 6×).

FALTA (por ordem):
- [ ] **Telemetria do harness** (pré-existente): os despejos Q/E param de ser
  drenados ~f=610, logo a seguir ao savestado pedido pelo vigia — o JOGO corre
  os 7602 quadros (harness PASS) mas os checks da secção "gameplay de ponta a
  ponta" leem um log morto (e esperavam `FNF_COLHEITA.txt`, extinto 0.2.164).
  Próximo passo: marcação fina no iow (quem deixa de chamar o despejo) +
  fallback de nomes nos scripts já feito (run_gameplay_harness.sh).
- [ ] **Marca d'água do anel em menus** (0,7 s) — adiada: o caminho ADPCM cobre
  o atraso e mexer no anel é o componente mais sensível (sem regressões).
- [ ] Consola: confirmar 8 slots + capturas ilimitadas + ausência de crackle
  SPU2 com SFX (o extractor v9.4 já lê tudo).

---
*Actualizar este documento a cada sessão. As correções do utilizador vão para
`CORRECOES.md`; o que falta IMPLEMENTAR fica aqui.*
