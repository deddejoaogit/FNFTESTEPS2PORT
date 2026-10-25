# PASSAGEM DE BASTÃO — FNF PS2

**Para a IA que for continuar este projeto. Leia nesta ordem:**

1. **`REGRAS.md`** — as regras do projeto. A regra 1 é inegociável.
2. **`CONHECIMENTO.md`** — tudo que foi medido e decidido, com fonte. **Isto é
   meses de trabalho. Não redescubra.**
3. **Este arquivo** — estado, problemas, próximos passos.
4. **`PROXIMOS_PASSOS.md`** — detalhamento do que falta, com o que já se sabe.
5. **`docs/FNF_PS2_PORT_PLAN.md`** — o plano original do porte.

---

## 1. O que é este projeto

Porte do **Friday Night Funkin'** (web/Lime/OpenFL) para **PlayStation 2**, em
C, com o ps2sdk. Compilado para MIPS R5900 (EE), rodando em console real
(SCPH-90006) a partir de pendrive.

**Repositório:** `grasshopperconta-a11y/PS2Projects`
**Branch:** `arena/4c50659b-ps2projectes` — **fixa, não renomeie, não troque.**
**Commit:** `7037d61`

---

## 2. Estado atual, honestamente

### O que funciona

- Build compila **sem nenhum warning**, suite inteira passa (1401 quadros,
  ASan+UBSan limpos, os dois testes negativos disparam).
- Faixa visível 4:3 correta no console: 112 linhas pretas, conteúdo apenas nas
  linhas 56..391 de 640×448.
- O log funciona e é analisável — ver §5.
- O fundo do menu **não estica mais**.

### O que não funciona ainda

Consulte `CONHECIMENTO.md` §5 para a lista completa com sintomas. Resumo:

| # | Problema | Estado |
|---|---|---|
| 1 | Linha estranha abaixo dos botões do menu | **Descoberta:** cada item é desenhado 2× por quadro. **Não corrigido.** |
| 2 | Itens quebrados ao voltar do modo história | Não corrigido |
| 3 | Fundo esticando | ✅ corrigido |
| 4 | Travamento ao entrar do modo história | Não reproduzido no harness |
| 5 | Tutorial sem notas/barra/animação | Código escrito, não confirmado |
| 6 | Log com 0 bytes | ✅ corrigido (0.2.111), **não confirmado em hardware ainda** |

### ⚠️ Três builds quebradas — leia antes de fazer qualquer coisa

As builds 0.2.108, 0.2.109 e 0.2.110 **não abriram no console**. Cada uma foi
enviada com uma causa afirmada com convicção, e **nenhuma era a real quando foi
dita**. A 0.2.111 finalmente coletou o dado que explicou tudo.

**A regra que nasceu disso:** quando não há dado que distinga as causas, a
próxima build serve para **coletar dado**, nunca para tentar um conserto. E o
harness local **não prova nada** sobre I/O, tempo ou sincronização — ele finge
`write()` e `fsync()` como instantâneos.

---

## 3. Como compilar, testar e entregar

```sh
# COMPILAR (o toolchain precisa estar em /home/user/PS2Projects/google_drive/)
sh tools/with_toolchain.sh make -j4 FNF_VERSION=0.2.112-sua-versao \
    EE_BIN=build/FNF_PS2.ELF

# TESTAR
sh test/run_tests.sh

# ANALISAR O LOG
python3 tools/analisa_log.py

# MONTAR O PENDRIVE
sh tools/build_pendrive.sh      # gera build/pendrive com LEIA-ME.txt na raiz
```

**O toolchain e os 16 assets de runtime estão no Google Drive** — ids em
`CONHECIMENTO.md` §2. Baixe antes de compilar; sem eles nada funciona.

### Ordem obrigatória antes de entregar uma build

1. compilar **sem warnings**;
2. rodar a suite inteira;
3. **provar a correção específica** (o número, a linha de log, a medição);
4. reconstruir do código commitado e confirmar binário **idêntico**;
5. só então subir para o Drive.

Se a correção é sobre I/O, tempo ou sincronização, o passo 3 **não pode** ser o
harness. Tem de ser o console.

---

## 4. Layout do pendrive

```
mass:/FNFPS2/assets/...       <- na RAIZ do pendrive, obrigatoriamente
mass:/FNFPS2/FNF_PS2.ELF
mass:/FNFPS2/FNF_COLHEITA.txt <- criado pelo jogo
mass:/FNFPS2/BOOT.txt         <- criado pelo jogo
```

Se a pasta `FNFPS2` não estiver na raiz, o jogo dorme para sempre em
`SleepThread()` e o log aparece no lugar errado. O ZIP de entrega já traz
`LEIA-ME.txt` na raiz explicando isso — não retire.

---

## 5. O log — seu principal instrumento

`mass:/FNFPS2/FNF_COLHEITA.txt`, acumulativo. Formato completo em
`CONHECIMENTO.md` §6. O essencial:

- `P,fase,linha,cod` — marco de fase, escrito e **sincronizado na hora**
- `S,` — estado da máquina de estados, um por quadro
- `D,quadro,i,estado,x,y,w,h,u,v,uw,vh,alfa,vy0,vy1,vy_span,gsx0,gsy0,gsx1,gsy1`
  — **um sprite por linha**, com a geometria já recortada
- `BOOT.txt` — último passo do boot completado, e um marco por quadro nos
  primeiros 300 quadros

**`python3 tools/analisa_log.py <log>` responde sozinho** os quatro defeitos que
o usuário reportou, sem ele precisar descrever nada. Use sempre.

---

## 6. Próximos passos, em ordem

1. **Confirmar em hardware que a 0.2.111 abre e que o log funciona.** Sem isso,
   nada mais pode ser afirmado. Peça ao usuário: rodar, e mandar
   `FNF_COLHEITA.txt` + `BOOT.txt`.
2. **Corrigir o menu desenhando cada item 2× por quadro.** A descoberta já está
   feita (o log mostra 8 sprites onde deveriam ser 4). Falta achar a segunda
   chamada e removê-la. Ver `PROXIMOS_PASSOS.md` P1.
3. **Investigar os itens quebrados ao voltar do modo história.** O log já
   registra `(y,h)` de cada item por quadro.
4. **Reproduzir o travamento do modo história.** O harness atravessa o jogo
   inteiro sem travar. Se o console travar, o `BOOT.txt` com marca por quadro
   vai dizer em qual quadro.
5. **Tutorial.** Câmera travada no BF, sem notas, sem barra de acerto,
   personagens não animando, música toca.
6. **Otimização** — objetivo declarado do usuário. Ver `PROXIMOS_PASSOS.md` P5.

---

## 7. O que está neste ZIP

```
REGRAS.md                     regras do projeto (leia primeiro)
HANDOFF.md                    este arquivo
CONHECIMENTO.md               tudo que foi medido, com fonte
PROXIMOS_PASSOS.md            detalhamento do que falta
fnfps2/                       código-fonte, ferramentas e testes
  src/                        inclusive main.c, telemetry.c/.h
  tools/                      with_toolchain.sh, run_harness.sh, analisa_log.py...
  test/                       suite e harness
  Makefile, VERSION, *.md
docs/FNF_PS2_PORT_PLAN.md     plano original do porte
PS2SDK-Complete-Documentation.md   referência do ps2sdk (13 MB)
evidencia-console/            BOOT.txt, log e capturas do último teste
```

O ELF compilado **não** está aqui (o ambiente de trabalho o perdeu num reset).
Baixe do Drive: `1MZ4wEki3gQB5Hj9y7HSWE6x56HsnRmB1`, ou recompile com o comando
do §3.

---

## 8. Aviso sobre o ambiente de trabalho

O sandbox em que este projeto foi desenvolvido **é resetado periodicamente**.
Quando isso acontece:

- `google_drive/` (toolchain + assets) desaparece;
- `fnfps2/build/` desaparece;
- o histórico local do git volta ao commit inicial.

**Recuperação:** `git fetch origin`, depois
`git reset --hard $(git ls-remote origin arena/4c50659b-ps2projectes | cut -f1)`.
Confira o tip real com `git ls-remote`, não com `git log origin/...`.

Guarde arquivos importantes **no repositório**, não em `/tmp` — `/tmp` também é
limpo, e um `rm -rf` de staging já apagou três documentos deste projeto.
