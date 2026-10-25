#include "io_fila.h"

void io_init(io_fila_t *f,
             int (*ler)(int, void *, unsigned int),
             int (*escrever)(int, const void *, unsigned int))
{
    int i;
    for (i = 0; i < IO_MAX_PEDIDOS; i++) {
        f->p[i].ativo = 0;
        f->p[i].pronto = 0;
        f->p[i].feitos = 0;
        f->p[i].resultado = 0;
    }
    f->relogio = 1;
    f->ler = ler;
    f->escrever = escrever;
    f->fatias = 0;
    f->trocas_de_dono = 0;
    f->maior_espera = 0;
}

int io_enfileirar(io_fila_t *f, int tipo, int fd, void *buf,
                  unsigned int bytes, int prio)
{
    int i;
    for (i = 0; i < IO_MAX_PEDIDOS; i++) {
        if (f->p[i].ativo) continue;
        f->p[i].tipo = tipo;
        f->p[i].fd = fd;
        f->p[i].prio = prio;
        f->p[i].buf = (unsigned char *)buf;
        f->p[i].bytes = bytes;
        f->p[i].feitos = 0;
        f->p[i].ativo = 1;
        f->p[i].pronto = 0;
        f->p[i].resultado = 0;
        f->p[i].selo = f->relogio++;
        return i;
    }
    return -1;
}

/* Escolhe o pedido mais prioritario ainda nao terminado. Entre iguais,
 * o que chegou primeiro: assim nenhum pedido fica para tras. */
static int escolher(io_fila_t *f)
{
    int i, melhor = -1;
    for (i = 0; i < IO_MAX_PEDIDOS; i++) {
        if (!f->p[i].ativo || f->p[i].pronto) continue;
        if (melhor < 0) { melhor = i; continue; }
        if (f->p[i].prio < f->p[melhor].prio) { melhor = i; continue; }
        if (f->p[i].prio == f->p[melhor].prio &&
            f->p[i].selo < f->p[melhor].selo) melhor = i;
    }
    return melhor;
}

int io_bombear(io_fila_t *f)
{
    static int ultimo = -1;
    int i = escolher(f);
    unsigned int resta, fatia;
    int n;

    if (i < 0) return 0;
    if (i != ultimo) { f->trocas_de_dono++; ultimo = i; }

    resta = f->p[i].bytes - f->p[i].feitos;
    fatia = resta < IO_FATIA ? resta : IO_FATIA;

    if (f->p[i].tipo == IO_LER)
        n = f->ler(f->p[i].fd, f->p[i].buf + f->p[i].feitos, fatia);
    else
        n = f->escrever(f->p[i].fd, f->p[i].buf + f->p[i].feitos, fatia);

    f->fatias++;

    if (n <= 0) {
        /* Erro ou fim de arquivo: encerra com o que ja foi feito. */
        f->p[i].resultado = f->p[i].feitos ? (int)f->p[i].feitos : n;
        f->p[i].pronto = 1;
        return 1;
    }
    f->p[i].feitos += (unsigned int)n;
    if (f->p[i].feitos >= f->p[i].bytes) {
        f->p[i].resultado = (int)f->p[i].feitos;
        f->p[i].pronto = 1;
    }
    return 1;
}

int io_terminado(const io_fila_t *f, int id)
{
    if (id < 0 || id >= IO_MAX_PEDIDOS) return 0;
    return f->p[id].ativo && f->p[id].pronto;
}

int io_resultado(io_fila_t *f, int id)
{
    int r;
    if (id < 0 || id >= IO_MAX_PEDIDOS || !f->p[id].ativo) return -1;
    r = f->p[id].resultado;
    f->p[id].ativo = 0;
    f->p[id].pronto = 0;
    return r;
}

int io_pendentes(const io_fila_t *f)
{
    int i, n = 0;
    for (i = 0; i < IO_MAX_PEDIDOS; i++)
        if (f->p[i].ativo && !f->p[i].pronto) n++;
    return n;
}
