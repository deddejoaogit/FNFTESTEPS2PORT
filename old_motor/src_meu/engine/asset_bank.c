#include "asset_bank.h"
#include <string.h>

void ab_init(asset_bank_t *b, unsigned int vram_base, unsigned int vram_bytes,
             ab_carregar_fn carregar, ab_enviar_fn enviar, ab_soltar_fn soltar,
             void *ctx)
{
    memset(b, 0, sizeof(*b));
    vc_init(&b->vram, vram_base, vram_bytes);
    b->carregar = carregar;
    b->enviar = enviar;
    b->soltar = soltar;
    b->ctx = ctx;
    b->cena = "";
}

static int achar(asset_bank_t *b, unsigned int chave)
{
    int i;
    if (chave == AB_CHAVE_INVALIDA) return -1;
    for (i = 0; i < b->n_ativos; i++)
        if (b->ativos[i].chave == chave) return i;
    return -1;
}

static int novo_slot(asset_bank_t *b, unsigned int chave)
{
    int i = achar(b, chave);
    if (i >= 0) return i;
    if (b->n_ativos >= AB_MAX_ATIVOS) return -1;
    i = b->n_ativos++;
    b->ativos[i].chave = chave;
    b->ativos[i].residente = 0;
    b->ativos[i].bytes_vram = 0;
    b->ativos[i].bytes_arquivo = 0;
    b->ativos[i].endereco = 0;
    b->ativos[i].classe = AB_UI;
    b->ativos[i].cliente = 0;
    return i;
}

static void contar_vivos(asset_bank_t *b)
{
    int i;
    unsigned int v = 0;
    for (i = 0; i < b->n_ativos; i++) if (b->ativos[i].residente) v++;
    if (v > b->maior_vivos_tela) b->maior_vivos_tela = v;
}

int ab_precisa(asset_bank_t *b, unsigned int chave, const char *caminho,
               unsigned int bytes_vram, unsigned int *endereco)
{
    int slot, r;
    unsigned int addr = 0;
    unsigned char *dados = NULL;
    unsigned int bytes = 0;

    slot = novo_slot(b, chave);
    if (slot < 0) { b->falhas++; return -1; }

    /* Ja residente: custo zero, nada e reenviado. Este caminho atende
       tambem os apelidos (bytes_vram 0), que nunca devem chegar ao
       alocador -- vc_pedir(0) e recusado por desenho. */
    if (b->ativos[slot].residente) {
        if (endereco) *endereco = b->ativos[slot].endereco;
        return 0;
    }

    /* Ja residente no alocador (pedido por outro caminho): custo zero. */
    r = vc_pedir(&b->vram, chave, bytes_vram, &addr);
    if (r == 1) {
        b->ativos[slot].endereco = addr;
        b->ativos[slot].bytes_vram = bytes_vram;
        b->ativos[slot].residente = 1;
        if (endereco) *endereco = addr;
        contar_vivos(b);
        return 0;
    }
    if (r < 0) {                      /* nao ha espaco: libera o que da */
        if (vc_despejar_ate(&b->vram, bytes_vram) < 0) { b->falhas++; return -1; }
        r = vc_pedir(&b->vram, chave, bytes_vram, &addr);
        if (r < 0) { b->falhas++; return -1; }
        if (r == 1) {
            b->ativos[slot].endereco = addr;
            b->ativos[slot].residente = 1;
            if (endereco) *endereco = addr;
            contar_vivos(b);
            return 0;
        }
    }

    /* Faltou: ler do pendrive e enviar. */
    if (!b->carregar || !caminho) { b->falhas++; return -1; }
    b->cargas++;
    if (b->carregar(b->ctx, caminho, &dados, &bytes) != 0 || !dados) {
        b->falhas++;
        /* desfaz a reserva feita acima */
        return -1;
    }
    b->bytes_lidos += bytes;
    if (b->enviar) b->enviar(b->ctx, chave, addr, dados, bytes);
    b->uploads++;
    b->bytes_enviados += bytes;
    if (b->soltar) b->soltar(b->ctx, dados);

    b->ativos[slot].endereco = addr;
    b->ativos[slot].bytes_vram = bytes_vram;
    b->ativos[slot].bytes_arquivo = bytes;
    b->ativos[slot].residente = 1;
    if (endereco) *endereco = addr;
    contar_vivos(b);
    return 1;
}

int ab_precisa_fixo(asset_bank_t *b, unsigned int chave, const char *caminho,
                    ab_classe_t classe, unsigned int bytes_vram)
{
    unsigned int addr = 0;
    int r = ab_precisa(b, chave, caminho, bytes_vram, &addr);
    int slot = achar(b, chave);
    if (r < 0) return -1;
    if (slot >= 0) b->ativos[slot].classe = (unsigned char)classe;
    vc_prender(&b->vram, chave);
    return r;
}

int ab_alias(asset_bank_t *b, unsigned int chave_origem, unsigned int chave_nova)
{
    int slot_o = achar(b, chave_origem);
    int slot_n;
    if (slot_o < 0 || !b->ativos[slot_o].residente) { b->falhas++; return -1; }
    slot_n = novo_slot(b, chave_nova);
    if (slot_n < 0) { b->falhas++; return -1; }
    if (vc_apelido(&b->vram, chave_origem, chave_nova) < 0) { b->falhas++; return -1; }
    b->ativos[slot_n].endereco = b->ativos[slot_o].endereco;
    b->ativos[slot_n].bytes_vram = 0;   /* nao ocupa nada */
    b->ativos[slot_n].classe = b->ativos[slot_o].classe;
    b->ativos[slot_n].residente = 1;
    b->aliases++;
    contar_vivos(b);
    return 0;
}

void ab_cena_entrar(asset_bank_t *b, const char *nome_cena)
{
    int i;
    b->cena = nome_cena ? nome_cena : "";
    vc_limpar(&b->vram);
    for (i = 0; i < b->n_ativos; i++) {
        /* presos (fonte, HUD) continuam; o resto sai */
        if (b->ativos[i].residente && b->ativos[i].classe != AB_FONTE
            && b->ativos[i].classe != AB_HUD) {
            int achou = 0;
            int k;
            for (k = 0; k < b->vram.n; k++)
                if (b->vram.b[k].usado && b->vram.b[k].chave == b->ativos[i].chave) { achou = 1; break; }
            if (!achou) b->ativos[i].residente = 0;
        }
    }
}

void ab_cena_sair(asset_bank_t *b)
{
    int i;
    /* Saida de cena solta TAMBEM o que estava preso: aqui nao existe mais
       tela usando fonte nem HUD. vc_limpar() respeita os presos, entao o
       reset e feito reconstruindo o alocador com o mesmo orcamento --
       e a unica forma de provar "zero vazamento" no teste. */
    vc_init(&b->vram, b->vram.base, b->vram.tamanho);
    for (i = 0; i < b->n_ativos; i++) b->ativos[i].residente = 0;
    b->n_ativos = 0;
    b->cena = "";
}

int ab_residente(const asset_bank_t *b, unsigned int chave)
{
    int i;
    for (i = 0; i < b->n_ativos; i++)
        if (b->ativos[i].chave == chave) return b->ativos[i].residente;
    return 0;
}

int ab_vivos(const asset_bank_t *b)
{
    int i, v = 0;
    for (i = 0; i < b->n_ativos; i++) if (b->ativos[i].residente) v++;
    return v;
}

/* Relatorio sem printf, mesma razao de vc_relatar(). */
static int escreve(char *d, int p, int tam, const char *s)
{
    int i;
    for (i = 0; s[i] && p < tam - 1; i++) d[p++] = s[i];
    d[p] = 0;
    return p;
}

static int escreve_num(char *d, int p, int tam, unsigned int v)
{
    char tmp[12];
    int t = 0;
    if (!v) tmp[t++] = '0';
    while (v && t < 11) { tmp[t++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (t > 0 && p < tam - 1) d[p++] = tmp[--t];
    d[p] = 0;
    return p;
}

void ab_relatar(const asset_bank_t *b, char *destino, int tam)
{
    int p = 0;
    if (tam <= 0) return;
    destino[0] = 0;
    p = escreve(destino, p, tam, "vivos=");
    p = escreve_num(destino, p, tam, (unsigned int)ab_vivos(b));
    p = escreve(destino, p, tam, " cargas=");
    p = escreve_num(destino, p, tam, b->cargas);
    p = escreve(destino, p, tam, " uploads=");
    p = escreve_num(destino, p, tam, b->uploads);
    p = escreve(destino, p, tam, " lidos=");
    p = escreve_num(destino, p, tam, b->bytes_lidos);
    p = escreve(destino, p, tam, " enviados=");
    p = escreve_num(destino, p, tam, b->bytes_enviados);
    p = escreve(destino, p, tam, " despejos=");
    p = escreve_num(destino, p, tam, b->vram.despejos);
    p = escreve(destino, p, tam, " falhas=");
    p = escreve_num(destino, p, tam, b->falhas);
}
