// Gerenciador de memoria virtual (Operating System Concepts, 10a ed., P-51)
//
// uso: ./vm <arquivo_de_enderecos> <fifo|lru> <fifo|lru>
//   2o argumento: algoritmo de substituicao de pagina
//   3o argumento: algoritmo de substituicao da TLB

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
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

// le o arquivo inteiro antes de traduzir: se alguma linha estiver fora do
// formato (um endereco inteiro sem sinal por linha) nada e gravado na saida
static uint32_t *load_addresses(const char *path, size_t *count)
{
    FILE *f = fopen(path, "r");
    uint32_t *addrs = NULL;
    size_t n = 0, cap = 0;
    unsigned long line_no = 0;
    char line[64];

    if (f == NULL)
        die("nao foi possivel abrir '%s': %s", path, strerror(errno));
    while (fgets(line, sizeof line, f) != NULL) {
        char *p = line, *end;
        unsigned long long v;

        line_no++;
        if (strchr(line, '\n') == NULL && !feof(f))
            die("'%s', linha %lu: linha longa demais", path, line_no);
        while (isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            continue; // linha em branco
        if (!isdigit((unsigned char)*p))
            die("'%s', linha %lu: esperado um endereco inteiro sem sinal", path, line_no);
        errno = 0;
        v = strtoull(p, &end, 10);
        if (errno != 0 || v > UINT32_MAX)
            die("'%s', linha %lu: endereco nao cabe em 32 bits", path, line_no);
        while (isspace((unsigned char)*end))
            end++;
        if (*end != '\0')
            die("'%s', linha %lu: texto inesperado depois do endereco", path, line_no);
        if (n == cap) {
            cap = cap != 0 ? cap * 2 : 1024;
            addrs = realloc(addrs, cap * sizeof *addrs);
            if (addrs == NULL)
                die("memoria insuficiente para ler '%s'", path);
        }
        addrs[n++] = (uint32_t)v;
    }
    if (ferror(f))
        die("falha ao ler '%s'", path);
    fclose(f);
    if (n == 0)
        die("'%s' nao contem nenhum endereco", path);
    *count = n;
    return addrs;
}

int main(int argc, char **argv)
{
    enum policy page_policy, tlb_policy;
    uint32_t *addrs;
    size_t n_addrs;

    if (argc != 4)
        die("numero de argumentos invalido\n"
            "uso: %s <arquivo_de_enderecos> <fifo|lru> <fifo|lru>",
            argv[0] != NULL ? argv[0] : "./vm");
    page_policy = parse_policy(argv[2], "de pagina");
    tlb_policy = parse_policy(argv[3], "da TLB");

    addrs = load_addresses(argv[1], &n_addrs);

    (void)page_policy;
    (void)tlb_policy;
    free(addrs);
    return 0;
}
