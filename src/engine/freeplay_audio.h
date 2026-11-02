/* O AUDIO DA TELA DE FREEPLAY -- o preview da musica e os jingles do rank.
 *
 * O QUE O ORIGINAL FAZ (`FreeplayState.hx` v0.8.6)
 *
 * 1. O PREVIEW. `changeSelection` (L2954-2955) PAUSA a musica do menu e so'
 *    `FADE_IN_DELAY` (0,25 s, L141) depois chama `playCurSongPreview`
 *    (L2967-3046). O que ela faz:
 *
 *      - `clearPreviews()` (L3046): cancela os dois `FlxTimer` do preview
 *        anterior;
 *      - `FlxG.sound.music.stop()`: o menu deixa de tocar de vez;
 *      - `previewVolume = 0,7 * dj.getMusicPreviewMult()` (L2970-2972). O
 *        unico DJ do port e' o `bf` (`dj_bf.cfz4`), e o unico
 *        `getMusicPreviewMult` do repositorio inteiro esta' no
 *        `BaseFreeplayDJ.hx` L129-132 devolvendo 1 -- medido com
 *        `grep -rn MusicPreviewMult source/`, 2 linhas, zero sobrescritas.
 *        Logo: 0,7, constante;
 *      - `FunkinSound.playMusic(id, {pathsFunction: INST, partialParams:
 *        {loadPartial: true, start: previewStartTime, end: previewEndTime}})`
 *        (L3005-3023): so' o INSTRUMENTAL, e so' um TRECHO. As fracoes vem
 *        do `playData` da musica e o padrao e' `DEFAULT_PREVIEW_START_TIME`
 *        (0) e `DEFAULT_PREVIEW_END_TIME` (0,2) -- `util/Constants.hx` L306 e
 *        L311. Nenhuma das 27 musicas do projeto tem `previewStart`/
 *        `previewEnd` no metadata (medido: 0 ocorrencias em
 *        os 27 arquivos de `assets-data/songs/`), entao TODAS usam 0 -> 20 %;
 *      - no `onLoad` (L3024-3038): `fadeIn(2, 0, previewVolume)`, um timer em
 *        `length/1000 - 2` que faz `fadeOut(2, 0)`, e um timer em
 *        `length/1000` que chama `playCurSongPreview()` de novo. Ou seja:
 *        laco infinito, 2 s de entrada e 2 s de saida;
 *      - `curSelected == 0` toca `freeplayRandom` (L2978-2986). Esse trecho
 *        NAO esta' no port por dois motivos medidos: (a) a capsula aleatoria
 *        nao existe na lista do port -- as 27 musicas, sem a capsula
 *        "?" -- e (b) `shared/music/freeplayRandom.ogg` NAO existe no
 *        repositorio de arte (`ls shared/music | grep -i random`: 0).
 *        E' dado ausente, nao um chute.
 *
 * 2. OS JINGLES DO RANK. Na cinematica de rank (`rankDisplayNew` L1190-1197 e
 *    `rankAnimSlam` L1232-1246) o original toca DOIS `playOnce` de
 *    `preload/sounds/ranks/`: um no tranco (+0,1 s do display, isto e' 600 ms
 *    depois do `rankAnimStart`) e outro no slam (1100 ms), escolhidos pelo
 *    rank NOVO. Como `switch` no Haxe NAO cai para o case seguinte (medido no
 *    manual da linguagem, `content/05-expression.md` L797-830), cada rank
 *    toca exatamente um som de cada lista.
 *
 * POR QUE ESTE MODULO E' PURO
 * As tres contas -- a janela em bytes, o ganho do fade e a escolha do jingle --
 * sao funcao do tempo e dos numeros da fonte. Aqui elas nao tocam arquivo, GS
 * nem audsrv, entao `test/test_freeplay_audio.c` as prova no PC. Quem lida com
 * o pendrive e' o `audio_ring` (`audio_ring_iniciar_preview`), e quem decide o
 * instante e' o `main.c`.
 */
#ifndef FNF_FREEPLAY_AUDIO_H
#define FNF_FREEPLAY_AUDIO_H

#include "freeplay_rank.h"

/* Taxa de todo o audio do port: o `audsrv` recebe PCM cru e todo o resto do
 * jogo (`freakyMenu_48000.pcm`, os stems, os sons de game over) ja' e' 48 kHz. */
#define FP_AUD_RATE 48000

/* `previewVolume = 0.7` (`FreeplayState.hx` L2970). Ver o cabecalho. */
#define FP_AUD_VOLUME 0.7f

/* `fadeIn(2, 0, previewVolume)` / `fadeOut(2, 0)` (L3025-3031), em ms. */
#define FP_AUD_FADE_MS 2000

/* `FreeplayState.FADE_IN_DELAY` (L141), em ms. */
#define FP_AUD_ATRASO_MS 250

/* `Constants.DEFAULT_PREVIEW_START_TIME` (L306) e `DEFAULT_PREVIEW_END_TIME`
 * (L311), como FRACAO do arquivo. */
#define FP_AUD_PREVIEW_INI 0.0f
#define FP_AUD_PREVIEW_FIM 0.2f

/* O `stems.pcm` do port tem 4 canais de 16 bits (instL, instR, voicesL,
 * voicesR) -- 8 bytes por amostra. O preview do original e' `INST`, entao os
 * dois ultimos canais ficam de fora. */
#define FP_AUD_STEMS_BYTES 8

/* ---- A JANELA DO PREVIEW --------------------------------------------------
 * Traduz as fracoes do `loadPartial` para um intervalo de BYTES no arquivo de
 * `total_bytes`. O piso e' 8 bytes para a conta bater com uma amostra inteira
 * dos 4 canais e o fim nunca passa do arquivo.
 *
 * Devolve 0 e escreve `*ini`/`*fim` (fim > ini) quando da'; -1 quando nao ha'
 * janela possivel (arquivo menor que uma amostra, ponteiro nulo). As fracoes
 * fora de 0..1 sao grampeadas -- o original ja' faz o mesmo em
 * `get_previewStartTime`/`get_previewEndTime` (L3405/L3420).
 */
int fp_aud_janela(long total_bytes, float ini, float fim, long *ini_b, long *fim_b);

/* Quantas AMOSTRAS (pares L,R) cabem na janela. */
int fp_aud_janela_amostras(long ini_b, long fim_b);

/* ---- O GANHO DO PREVIEW ---------------------------------------------------
 * Ganho em `pos` (amostras desde o comeco da janela) para uma janela de
 * `total` amostras. Reproduz os dois `FlxTween` de volume do original:
 *
 *   - `fadeIn(2, 0, vol)`: linear de 0 a `vol` nos primeiros 2 s;
 *   - `fadeOut(2, 0)`: linear de `vol` a 0 nos ULTIMOS 2 s, criado no instante
 *     `len - 2 s`. Um tween criado depois vence o outro (o `globalManager`
 *     atualiza na ordem de criacao), e por isso o ganho e' o MENOR dos dois
 *     ramos;
 *   - janelas de 2 s ou menos: `fadeStart = len - 2` fica <= 0, o `FlxTimer`
 *     do fadeOut (`/tmp/flixelf`, `FlxTimer.start` com tempo <= 0) vence no
 *     primeiro update e o `FlxTween.tween(music, {volume: 0}, 2)` captura o
 *     volume DAQUELE instante como inicio. No instante 0 o fadeIn acabou de
 *     comecar, entao o volume capturado e' 0 e o tween inteiro vai de 0 a 0:
 *     a janela curta toca em SILENCIO. E' o comportamento da fonte, nao um
 *     caso inventado -- e nenhuma das 27 musicas cai nele (o menor trecho de
 *     20 % do projeto tem mais de 20 s).
 */
float fp_aud_ganho(int pos, int total, float vol);

/* Ganho no meio de um bloco, dados os ganhos das duas pontas: o `audio_ring`
 * calcula `fp_aud_ganho` no inicio e no fim de cada bloco e interpola, em vez
 * de chamar a funcao por amostra. */
static inline float fp_aud_ganho_interp(float g0, float g1, int i, int n)
{
    if (n <= 1) return g0;
    return g0 + (g1 - g0) * ((float)i / (float)(n - 1));
}

/* ---- OS JINGLES DO RANK ---------------------------------------------------
 * Nome do arquivo (sem extensao) que o original toca em cada momento, dado o
 * rank NOVO (`fnf_rank_t`: 0 SHIT .. 5 PERFECT_GOLD). `-1` (nenhum rank) cai
 * no `default` do `switch`. Nunca devolve NULL.
 */
const char *fp_aud_jingle_novo(int rank);   /* L1190-1197, +0,1 s do display */
const char *fp_aud_jingle_slam(int rank);   /* L1232-1246, no slam           */

/* ---- A AGENDA DA CINEMATICA -----------------------------------------------
 * O que tem de acontecer entre dois quadros da cinematica. `antes` e `agora`
 * sao ms desde o `rankAnimStart`; o evento dispara quando o instante cai em
 * `(antes, agora]`. Existe para que um quadro longo (30 ms no console, mas
 * ate' 400 ms numa transicao) nao PULE um jingle -- o `FlxTimer` do original
 * tambem nao pula: ele dispara no primeiro update depois do vencimento.
 */
#define FP_AUD_EV_JINGLE_NOVO 1   /* FP_RK_T_FAKE_OFF (600 ms) */
#define FP_AUD_EV_JINGLE_SLAM 2   /* FP_RK_T_SLAM      (1100 ms) */
#define FP_AUD_EV_PREVIEW     4   /* FP_RK_FIM         (2200 ms): o
                                   * `playCurSongPreview` do fim do tremor
                                   * (L1289) recomeca o preview do zero */
int fp_aud_eventos(int antes, int agora);

#endif /* FNF_FREEPLAY_AUDIO_H */
