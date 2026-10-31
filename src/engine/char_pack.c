#include "char_pack.h"

#include "lz4_bloco.h"

#include <string.h>

static unsigned int le32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int cp_abrir(const unsigned char *pacote, unsigned int bytes, cp_pack_t *p)
{
    unsigned int tabela, off_anims, i;
    if (!p) return CHP_ERR_CABECALHO;
    memset(p, 0, sizeof(*p));
    if (!pacote) return CHP_ERR_CABECALHO;
    if (bytes < CHP_CABECALHO) return CHP_ERR_CABECALHO;
    if (memcmp(pacote, CHP_MAGIC, 4) != 0) return CHP_ERR_MAGIC;
    /* v1 e v2 tem o mesmo layout; a diferenca e' o campo 32, que no v1 era
       "reservado" (0) e no v2 guarda a escala do canvas. */
    if (le32(pacote + 4) != CHP_VERSAO && le32(pacote + 4) != 1u) return CHP_ERR_VERSAO;

    p->pacote = pacote;
    p->bytes = bytes;
    p->n_quadros = le32(pacote + 8);
    p->bytes_por_quadro = le32(pacote + 12);
    p->w = (int)le32(pacote + 16);
    p->h = (int)le32(pacote + 20);
    p->n_anims = (int)le32(pacote + 24);
    p->psm = (int)le32(pacote + 28);
    /* O campo 32 SO' vale no v2: no v1 ele era reservado e qualquer coisa nele
       e' lixo de outro escritor. x10000; 0 = nao guardada e um valor fora da
       faixa (arquivo estragado) tambem -- dividir por zero no desenho e' pior
       do que desenhar no tamanho do canvas. */
    p->escala = 0.0f;
    if (le32(pacote + 4) == CHP_VERSAO) {
        unsigned int e = le32(pacote + 32);
        /* Faixa 0,01 .. 20. O maior valor real e' 6 (pixel art: senpai); acima
           de 20 e' arquivo estragado, nao um personagem gigante. */
        if (e >= 100u && e <= 200000u) p->escala = (float)e / 10000.0f;
    }

    if (p->psm != 32 && p->psm != 16) return CHP_ERR_PSM;
    if (p->w <= 0 || p->h <= 0 || p->w > 1024 || p->h > 1024) return CHP_ERR_QUADRO;
    if (p->n_quadros == 0 || p->n_quadros > 4096u) return CHP_ERR_QUADRO;
    /* Um quadro tem de bater com o canvas declarado. Se nao bater, o pacote foi
       montado com outro tamanho e desenharia lixo -- melhor nao entrar em cena. */
    if (p->bytes_por_quadro !=
        (unsigned int)p->w * (unsigned int)p->h * ((unsigned int)p->psm / 8u))
        return CHP_ERR_TAMANHO;

    if (p->n_anims <= 0 || p->n_anims > CHP_MAX_ANIMS) return CHP_ERR_ANIMS;

    tabela = CHP_CABECALHO + p->n_quadros * 8u;
    if (tabela > bytes) return CHP_ERR_TABELA;
    off_anims = tabela;
    if (off_anims + (unsigned int)p->n_anims * CHP_ANIM_ENTRADA > bytes) return CHP_ERR_TABELA;

    for (i = 0; i < p->n_quadros; i++) {
        unsigned int off = le32(pacote + CHP_CABECALHO + i * 8u);
        unsigned int tam = le32(pacote + CHP_CABECALHO + i * 8u + 4u);
        if (!tam || off > bytes || tam > bytes - off) return CHP_ERR_BLOCO;
    }

    for (i = 0; i < (unsigned int)p->n_anims; i++) {
        const unsigned char *e = pacote + off_anims + i * CHP_ANIM_ENTRADA;
        cp_anim_t *a = &p->anim[i];
        memcpy(a->nome, e, CHP_NOME);
        a->nome[CHP_NOME - 1] = 0;          /* nome sem NUL nao vira lixo na tela */
        a->primeiro = (int)le32(e + CHP_NOME);
        a->quantos = (int)le32(e + CHP_NOME + 4u);
        a->fps100 = (int)le32(e + CHP_NOME + 8u);
        a->looped = (int)le32(e + CHP_NOME + 12u);
    }
    return 0;
}

int cp_conferir(const cp_pack_t *p)
{
    int i;
    if (!p) return CHP_ERR_CABECALHO;
    for (i = 0; i < p->n_anims; i++) {
        const cp_anim_t *a = &p->anim[i];
        if (a->quantos <= 0) return CHP_ERR_TABELA;
        if (a->primeiro < 0) return CHP_ERR_TABELA;
        if ((unsigned int)(a->primeiro + a->quantos) > p->n_quadros) return CHP_ERR_TABELA;
        if (a->fps100 < 0) return CHP_ERR_TABELA;
    }
    return 0;
}

int cp_quadro(const cp_pack_t *p, unsigned int q, unsigned char *dst)
{
    unsigned int off, tam;
    int r;
    if (!p || !p->pacote || !dst) return CHP_ERR_BLOCO;
    if (q >= p->n_quadros) return CHP_ERR_QUADRO;
    off = le32(p->pacote + CHP_CABECALHO + q * 8u);
    tam = le32(p->pacote + CHP_CABECALHO + q * 8u + 4u);
    r = lz4_bloco_decode(p->pacote + off, tam, dst, p->bytes_por_quadro);
    return r == 0 ? 0 : CHP_ERR_LZ4;
}

int cp_anim_i(const cp_pack_t *p, const char *nome)
{
    int i, achou = -1;
    if (!p || !nome) return -1;
    /* A ULTIMA de mesmo nome, e nao a primeira. Medido na arte do original:
       "Boyfriend (Holding GF)" tem DUAS animacoes chamadas `singUP` (uma e' o
       "BF Dead with GF Loop", a outra o "BF NOTE UP0"). O jogo original carrega
       as animacoes num Map por nome (FlxAnimationUtil.addByPrefix ->
       FlxAnimationController._animations.set), entao a segunda SOBRESCREVE a
       primeira e e' ela que toca. Devolver a primeira aqui poria em cena o
       quadro errado nesse personagem -- e so' nele, o que e' o tipo de defeito
       que passa por todos os testes e aparece uma vez na tela. */
    for (i = 0; i < p->n_anims; i++)
        if (strncmp(p->anim[i].nome, nome, CHP_NOME) == 0) achou = i;
    return achou;
}

int cp_seq(const cp_pack_t *p, int anim, ch_seq_t *out)
{
    const cp_anim_t *a;
    if (!p || !out || anim < 0 || anim >= p->n_anims) return -1;
    a = &p->anim[anim];
    out->first = a->primeiro;
    out->count = a->quantos;
    out->fps100 = a->fps100;
    out->looped = a->looped;
    return 0;
}

const char *cp_erro(int rc)
{
    switch (rc) {
    case 0: return "ok";
    case CHP_ERR_MAGIC: return "nao e' um pacote CHP4";
    case CHP_ERR_VERSAO: return "versao do pacote diferente";
    case CHP_ERR_CABECALHO: return "cabecalho curto";
    case CHP_ERR_TABELA: return "tabela de quadros ou de animacoes fora do arquivo";
    case CHP_ERR_ANIMS: return "numero de animacoes invalido";
    case CHP_ERR_QUADRO: return "tamanho de quadro ou de canvas invalido";
    case CHP_ERR_PSM: return "formato de pixel diferente de 32 e 16";
    case CHP_ERR_BLOCO: return "bloco de quadro fora do arquivo";
    case CHP_ERR_TAMANHO: return "bytes por quadro nao batem com o canvas";
    case CHP_ERR_LZ4: return "bloco LZ4 nao decodificou no tamanho pedido";
    default: return "erro desconhecido do pacote de personagem";
    }
}

float cp_escala(const cp_pack_t *p)
{
    /* Sem escala guardada, 1,0 e' a unica resposta honesta: diz "o canvas esta'
       no tamanho do original". O chamador avisa no log quando isso acontece. */
    if (!p || p->escala <= 0.0f) return 1.0f;
    return p->escala;
}

unsigned int cp_tex_bytes(int w, int h, int psm)
{
    unsigned int stride, b;
    if (w <= 0 || h <= 0) return 0;
    stride = ((unsigned int)w + 63u) & ~63u;
    b = stride * (unsigned int)h * (unsigned int)(psm == 16 ? 2 : 4);
    /* O upload escreve exatamente h linhas com este passo: nada abaixo disto
       pode ser lido nem escrito, e nada acima e' preciso reservar. */
    return (b + 255u) & ~255u;
}
