# Modelo de carregamento — o que muda no `main.c` e por quê

## 1. Como está hoje (medido no port 0.2.112)

| item | estado |
|---|---|
| reservas de VRAM | 6, **todas** na inicialização |
| liberações de VRAM | **0** em todo o projeto |
| soma dos 8 buffers | 4.169.728 / 4.194.304 B = 99,4 % |
| sobra no comentário do código | 163.840 B |
| pacotes dentro do executável | `title_packs.S` + `story_packs.S` = 2.929.792 B |
| telas servidas por esses pacotes | 2 de 4 |
| caminho correto já existente | pacotes do tutorial: abrem do pendrive, fecham ao sair |

O resultado é estrutural: o jogo **não tem como** carregar uma segunda
música, porque não existe espaço e não existe caminho de liberação.

## 2. Como fica

```
boot
 ├─ ab_init(&banco, 2293760, 1900544, ...)     região de textura
 ├─ ab_precisa_fixo(fonte)                     fica preso
 └─ ab_precisa_fixo(hud)                       fica preso

entrar na música
 ├─ ab_cena_entrar(&banco, "tankmanBattlefield")
 ├─ ab_precisa(palco...)      -> carrega do pendrive e envia
 ├─ ab_precisa(personagem...) -> idem
 └─ ab_alias(henchmen, henchmen_b)  -> 5 sprites, 1 bloco

sair da música
 └─ ab_cena_sair(&banco)   -> região inteira devolvida (testado: 1.900.544 B)
```

Nada é reservado "para sempre" na inicialização. Cada pedido acontece no
momento em que a textura é necessária, com despejo automático do menos
usado recentemente quando falta espaço contíguo.

## 3. O que apagar do projeto

1. `src/title_packs.S` e `src/story_packs.S` — são os 2.929.792 B dentro do
   executável. Os `.frames` descomprimidos já vivem no pendrive; o
   `.incbin` é uma segunda cópia que só o título e a história usam.
2. As variáveis globais `tex`, `tex2`, `gftex`, `stagetex`, `fonttex` e as
   reservas correspondentes no boot.
3. O comentário do orçamento com "sobram 163.840 B" — deixa de ser verdade.

## 4. O que acrescentar (patch mínimo no `main.c`)

```c
#include "engine/asset_bank.h"
#include "engine/bootlog.h"

static asset_bank_t g_banco;
static bootlog_t    g_boot;

/* E/S real do console: carregar do pendrive, enviar ao GS, liberar. */
static int  carregar_real(void *ctx, const char *caminho,
                          unsigned char **dados, unsigned int *bytes);
static void enviar_real(void *ctx, unsigned int chave, unsigned int vram,
                        const unsigned char *d, unsigned int n);
static void soltar_real(void *ctx, unsigned char *d);

/* no boot */
ab_init(&g_banco, 2293760 /* fim dos dois framebuffers */, 1900544,
        carregar_real, enviar_real, soltar_real, NULL);
ab_precisa_fixo(&g_banco, CH_FONTE, "mass:/FNFPS2/assets/font.rgba", AB_FONTE, 327680);
ab_precisa_fixo(&g_banco, CH_HUD,   "mass:/FNFPS2/assets/hud.lz4f",  AB_HUD,   294912);

/* na entrada da música */
ab_cena_entrar(&g_banco, fase->nome_palco);
ab_precisa(&g_banco, chave_palco, caminho_palco, bytes_palco, &addr);

/* no desenho: quem desenha pede por chave, não por variável global */
unsigned int vram;
if (ab_precisa(&g_banco, chave_sprite, caminho, bytes, &vram) < 0) {
    bootlog_alerta(&g_boot, "SEM_VRAM", (int)chave_sprite);
}

/* na saída da música */
ab_cena_sair(&g_banco);
```

## 5. O que está provado por teste (e o que não está)

Provado em `tests/test_asset_bank.c`, com o orçamento real do console:

* 31 texturas vivas + 3 fixas = 34 entradas, **0 falhas**, 0 despejos
  depois de aquecido;
* 5 sprites apontando para a mesma folha ocupam **um** bloco
  (`ab_alias`), sem gastar VRAM;
* palco com mais conteúdo do que a região: o cache despeja durante o
  carregamento e **não falha**;
* 200 trocas de cena: a região volta a 1.900.544 B livres, sem
  fragmentação (`vc_maior_buraco == 1.900.544`);
* pedido maior que a região: erro devolvido e contado, não silêncio;
* nenhuma alocação pode terminar fora de `[base, base+tamanho]` — a guarda
  que o PC não tinha e que explica o travamento no console.

Não provado (precisa do console): taxa real de leitura do pendrive durante
o jogo, tempo de upload do GS por quadro, e se o despejo no meio do
desenho cabe no orçamento de 16,68 ms de um quadro. O instrumento para
medir isso é o `bootlog` (`BF_AUDIO_LEITURA`, `BF_DESENHO`, alerta
`QUADRO_LENTO`).
