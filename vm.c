// Gerenciador de memoria virtual (Operating System Concepts, 10a ed., P-51)
//
// uso: ./vm <arquivo_de_enderecos> <fifo|lru> <fifo|lru>
//   2o argumento: algoritmo de substituicao de pagina
//   3o argumento: algoritmo de substituicao da TLB

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum policy { FIFO, LRU };

// mostra o erro e encerra o programa
static void die(const char *fmt, ...)
{
    va_list ap;

    fputs("Erro: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

// converte "fifo" ou "lru"; qualquer outro texto encerra com erro
static enum policy parse_policy(const char *s, const char *what)
{
    if (strcmp(s, "fifo") == 0)
        return FIFO;
    if (strcmp(s, "lru") == 0)
        return LRU;
    die("algoritmo de substituicao %s invalido: '%s' (use fifo ou lru)", what, s);
    return FIFO; // nao chega aqui
}

int main(int argc, char **argv)
{
    enum policy page_policy, tlb_policy;

    if (argc != 4)
        die("numero de argumentos invalido\n"
            "uso: %s <arquivo_de_enderecos> <fifo|lru> <fifo|lru>",
            argv[0] != NULL ? argv[0] : "./vm");
    page_policy = parse_policy(argv[2], "de pagina");
    tlb_policy = parse_policy(argv[3], "da TLB");

    (void)page_policy;
    (void)tlb_policy;
    return 0;
}
