# Onde o jogo para — 2026-11-04 (builds 0.2.161 a 0.2.163)

## O que o console devolveu, em ordem

| build | o que apareceu | o que os arquivos disseram |
|---|---|---|
| 0.2.161 | barras, depois tela preta | `BOOT.txt`: `passo 33: F0_FRAMEBUFFER_SET`; `FNF_COLHEITA.txt`: **0 bytes** |
| 0.2.162 | barras + sinal de vida **parado no 1º quadrado** | igual: `passo 33`, mas agora o log veio completo até `P,F0_FRAMEBUFFER_SET` |

`F0_FRAMEBUFFER_SET` é o **último** marco do quadro 0. O sinal de vida andava a
cada 15 quadros e parou no primeiro → o jogo não chegou ao quadro 15. Não é
tela preta por falta de desenho: o desenho do quadro 0 saiu (o `qw` do pacote
está no log: 111 quadwords) e a tela ficou congelada nele.

## O que existe entre esse marco e o seguinte

```
F0_FRAMEBUFFER_SET            <- último marco gravado
  after_vsync = GetTimerSystemTime()
  if (deferred_menu_start && frame_no>=3) ...     (frame 0: não)
  if (g_dl_gravando) ...                          (não)
  dl_dump()                                       (0.2.162: só sob pedido)
  telemetry_frame()                               (RAM)
  if (frame_no&63)==1 -> telemetry_flush()        <-- ÚNICA E/S
  frame_mark("F.POST")                            <- seria o próximo marco
```

`telemetry_flush()` abre o log com **O_APPEND**, escreve ~4 KB, faz `fsync` e
`close`. Os marcos que sempre funcionaram (`BOOT.txt`) abrem com **O_TRUNC**,
escrevem 36 bytes, sincronizam e fecham — e funcionaram **15 vezes dentro do
próprio quadro 0**. A diferença é o append.

## A decisão da 0.2.163

Tirar **toda** a E/S de dentro do laço (flush e `frame_mark`) e pôr o marco na
tela: 16 quadrados no rodapé. Nada ali depende do pendrive, então o jogo não
pode mais ser vítima do próprio diagnóstico.

Se a 0.2.163 rodar, a causa está confirmada e o log volta depois por um caminho
que se prove seguro (O_TRUNC + bloco pequeno + close, o do `BOOT.txt`).
Se ela parar no mesmo ponto, os quadrados dizem em qual passo — e aí a causa não
é E/S.

## PROVA 3

Bateria focada na pergunta, com o relatório regravado **antes** de cada passo
arriscado (se travar, o arquivo no cartão diz qual era): TRUNC 36 B, APPEND
200 B, APPEND 4 KB, TRUNC 4 KB, arquivo a crescer, 5 MB por append, dois
descritores ao mesmo tempo, um descritor preso + outro escrevendo, 200 opens,
unlink+recriar.
