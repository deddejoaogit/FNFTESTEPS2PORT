# O QUE FALTA IMPLEMENTAR — FNF PS2 (actualizado na 0.2.178)

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
- [x] VALORES da TV no txt (provado na 0.2.177: `# VALORES f=30865 ...`).
- [x] Extrador v6 com PNG (MINIATURA/capturas).
- [~] **Captura "save state gigante"** — implementada na 0.2.178 (imagem+texto
      em RAM, escrita pela thread escritora; ver CORRECOES #5-#6).
- [ ] História Q no ficheiro mesmo quando o USB está doente (0.2.178: o corpo
      do TRAVA passa a incluir as amostras Q directamente).

## 12. CAUSAS SEPARADAS CONHECIDAS (não são "features")
- **pad=0000**: os registos nunca vêm bits do pad — causa própria, à parte de
  tudo o resto (o jogo lê o pad? o extractor mostra 0000 sempre).
- **usbhdfsd**: as operações de CRIAÇÃO/EXTENSÃO de ficheiros morrem depois de
  minutos de fluxo de áudio (leituras continuam boas). Regra de ouro: os logs
  vivem em RAM e escrevem no TRAVA; pré-criar ficheiros no boot (0.2.178).
- **FNF_SCREEN** = código morto (0 bytes para sempre) — remover ou ignorar.

---
*Actualizar este documento a cada sessão. As correções do utilizador vão para
`CORRECOES.md`; o que falta IMPLEMENTAR fica aqui.*
