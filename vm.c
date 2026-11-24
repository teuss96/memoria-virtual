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

#define PAGE_SIZE 256
#define NUM_PAGES 256
#ifndef NUM_FRAMES
#define NUM_FRAMES 128 // metade das 256 paginas: obriga a substituir paginas
#endif
#define BACKING_STORE "BACKING_STORE.bin"
#define OUTPUT_FILE "correct.txt"

enum policy { FIFO, LRU };

static signed char memory[NUM_FRAMES][PAGE_SIZE]; // memoria fisica
static int page_table[NUM_PAGES];                 // quadro da pagina; -1 = fora da memoria
static int frame_page[NUM_FRAMES];                // pagina no quadro; -1 = quadro livre
static unsigned long frame_stamp[NUM_FRAMES];     // instante em que o quadro foi carregado
static unsigned long now;                         // relogio logico: numero do acesso atual

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

// abre o BACKING_STORE e confere o tamanho antes de qualquer traducao
static FILE *open_store(void)
{
    FILE *f = fopen(BACKING_STORE, "rb");

    if (f == NULL)
        die("nao foi possivel abrir " BACKING_STORE ": %s", strerror(errno));
    if (fseek(f, 0, SEEK_END) != 0 || ftell(f) < (long)NUM_PAGES * PAGE_SIZE)
        die(BACKING_STORE " tem menos de %d bytes", NUM_PAGES * PAGE_SIZE);
    return f;
}

// indice do menor carimbo; no empate vence o menor indice, entao as posicoes
// nunca usadas (carimbo 0) sao ocupadas em ordem crescente
static int oldest(const unsigned long *stamp, int n)
{
    int i, best = 0;

    for (i = 1; i < n; i++)
        if (stamp[i] < stamp[best])
            best = i;
    return best;
}

// falta de pagina: traz a pagina do BACKING_STORE e devolve o quadro usado
static int page_in(FILE *store, int page)
{
    int frame = oldest(frame_stamp, NUM_FRAMES);

    if (frame_page[frame] != -1)
        page_table[frame_page[frame]] = -1; // a vitima sai da memoria
    if (fseek(store, (long)page * PAGE_SIZE, SEEK_SET) != 0 ||
        fread(memory[frame], 1, PAGE_SIZE, store) != PAGE_SIZE)
        die("falha ao ler a pagina %d de " BACKING_STORE, page);
    frame_page[frame] = page;
    page_table[page] = frame;
    frame_stamp[frame] = now;
    return frame;
}

int main(int argc, char **argv)
{
    enum policy page_policy, tlb_policy;
    uint32_t *addrs;
    size_t n_addrs, i;
    unsigned long faults = 0;
    FILE *store, *out;

    if (argc != 4)
        die("numero de argumentos invalido\n"
            "uso: %s <arquivo_de_enderecos> <fifo|lru> <fifo|lru>",
            argv[0] != NULL ? argv[0] : "./vm");
    page_policy = parse_policy(argv[2], "de pagina");
    tlb_policy = parse_policy(argv[3], "da TLB");

    addrs = load_addresses(argv[1], &n_addrs);
    store = open_store();
    out = fopen(OUTPUT_FILE, "w");
    if (out == NULL)
        die("nao foi possivel criar " OUTPUT_FILE ": %s", strerror(errno));

    for (i = 0; i < NUM_PAGES; i++)
        page_table[i] = -1;
    for (i = 0; i < NUM_FRAMES; i++)
        frame_page[i] = -1;

    for (i = 0; i < n_addrs; i++) {
        uint32_t addr = addrs[i] & 0xFFFF; // so os 16 bits menos significativos
        int page = (int)(addr >> 8);
        int offset = (int)(addr & 0xFF);
        int frame = page_table[page];

        now++;
        if (frame == -1) {
            faults++;
            frame = page_in(store, page);
        }
        fprintf(out, "Virtual address: %lu Physical address: %d Value: %d\n",
                (unsigned long)addrs[i], frame * PAGE_SIZE + offset, memory[frame][offset]);
    }
    fprintf(out, "Number of Translated Addresses = %lu\n", (unsigned long)n_addrs);
    fprintf(out, "Page Faults = %lu\n", faults);
    fprintf(out, "Page Fault Rate = %.3f\n", (double)faults / (double)n_addrs);

    if (ferror(out) || fclose(out) != 0)
        die("falha ao gravar " OUTPUT_FILE);
    fclose(store);
    (void)page_policy;
    (void)tlb_policy;
    free(addrs);
    return 0;
}
