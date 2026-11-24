#!/bin/sh
# uso: sh testes/run.sh [executavel]
# Cada caso roda em um diretorio temporario com uma copia do BACKING_STORE.bin,
# entao nenhum correct.txt e gravado no repositorio. Sai com status 1 se algum
# caso falhar. Os casos conferidos com testes/referencia.py sao pulados se nao
# houver python3.

VM=$(realpath "${1:-./vm}")
DIR=$(cd "$(dirname "$0")" && pwd)
RAIZ=$(dirname "$DIR")
TMP=$(mktemp -d)
trap 'chmod -R u+rw "$TMP"; rm -rf "$TMP"' EXIT
pass=0
fail=0
pulados=0

ok()  { pass=$((pass + 1)); echo "ok   - $1"; }
bad() { fail=$((fail + 1)); echo "FAIL - $1"; }

# novo: diretorio limpo com o BACKING_STORE.bin do enunciado
novo() {
    cd "$TMP" && rm -rf caso && mkdir caso && cd caso || exit 1
    cp "$RAIZ/BACKING_STORE.bin" .
}

# confere "nome" esperado args... : espera sucesso e correct.txt igual ao esperado
confere() {
    nome=$1; esperado=$2; shift 2
    rm -f correct.txt
    if "$VM" "$@" > saida.txt 2>&1 && cmp -s correct.txt "$esperado"; then
        ok "$nome"
    else
        bad "$nome"
        cat saida.txt
        [ -f correct.txt ] && diff correct.txt "$esperado" | head -10
    fi
}

# modelo "nome" enderecos pagina tlb : o esperado vem do modelo de referencia
modelo() {
    if ! command -v python3 > /dev/null 2>&1; then
        pulados=$((pulados + 1)); echo "pula - $1 (sem python3)"
        return
    fi
    python3 "$DIR/referencia.py" "$2" "$3" "$4" > esperado.txt
    confere "$1" esperado.txt "$2" "$3" "$4"
}

# tem "nome" "linha" : a linha precisa existir no correct.txt do ultimo caso
tem() {
    if grep -qx "$2" correct.txt 2> /dev/null; then ok "$1"; else bad "$1"; fi
}

# erro "nome" "trecho da mensagem" args... : espera a mensagem, status de erro
# (sem crash) e nenhum correct.txt criado
erro() {
    nome=$1; trecho=$2; shift 2
    rm -f correct.txt
    "$VM" "$@" > saida.txt 2>&1
    st=$?
    if [ $st -ne 0 ] && [ $st -lt 126 ] && grep -q "$trecho" saida.txt && [ ! -e correct.txt ]; then
        ok "$nome -> $(head -1 saida.txt)"
    else
        bad "$nome (status $st)"
        cat saida.txt
    fi
}

echo "== saida correta"

novo
confere "gabarito do professor (./vm addresses.txt lru fifo)" \
    "$DIR/esperado_lru_fifo.txt" "$RAIZ/addresses.txt" lru fifo

sed 's/$/\r/' "$RAIZ/addresses.txt" > crlf.txt
confere "mesmo arquivo com fim de linha CRLF" "$DIR/esperado_lru_fifo.txt" crlf.txt lru fifo

for p in fifo lru; do
    for t in fifo lru; do
        modelo "addresses.txt com pagina=$p tlb=$t" "$RAIZ/addresses.txt" $p $t
    done
done

# 3000 enderecos: 60% em 20 paginas quentes (acertos na TLB), o resto espalhado
awk 'BEGIN { srand(7); for (i = 0; i < 3000; i++) {
    p = (rand() < 0.6) ? int(rand() * 20) : int(rand() * 256)
    print p * 256 + int(rand() * 256) } }' > aleatorio.txt
for p in fifo lru; do
    for t in fifo lru; do
        modelo "3000 enderecos aleatorios com pagina=$p tlb=$t" aleatorio.txt $p $t
    done
done

# A pagina 0 fica na TLB (lru) por ser usada o tempo todo, mas e a primeira a
# sair da memoria (fifo) quando a pagina 128 chega. A TLB tem de esquecer a
# pagina 0: o acesso seguinte a ela e uma falta, a 130a.
{
    echo 3
    i=1
    while [ $i -le 127 ]; do echo $((i * 256 + 3)); echo 3; i=$((i + 1)); done
    echo $((128 * 256 + 3))
    echo 3
} > vitima_na_tlb.txt
modelo "pagina removida da memoria tambem sai da TLB" vitima_na_tlb.txt fifo lru
tem "  o reacesso a pagina removida conta como falta" "Page Faults = 130"
tem "  e nao como acerto na TLB" "TLB Hits = 127"

printf '16916 \n\n  62493\n\t30198' > espacos.txt
printf '%s\n' \
    'Virtual address: 16916 TLB: 0 Physical address: 20 Value: 0' \
    'Virtual address: 62493 TLB: 1 Physical address: 285 Value: 0' \
    'Virtual address: 30198 TLB: 2 Physical address: 758 Value: 29' \
    'Number of Translated Addresses = 3' \
    'Page Faults = 3' \
    'Page Fault Rate = 1.000' \
    'TLB Hits = 0' \
    'TLB Hit Rate = 0.000' > esperado.txt
confere "espacos, linha em branco e ultima linha sem quebra de linha" esperado.txt espacos.txt lru fifo

printf '30198\n30198\n95734\n' > mascara.txt
printf '%s\n' \
    'Virtual address: 30198 TLB: 0 Physical address: 246 Value: 29' \
    'Virtual address: 30198 TLB: 0 Physical address: 246 Value: 29' \
    'Virtual address: 95734 TLB: 0 Physical address: 246 Value: 29' \
    'Number of Translated Addresses = 3' \
    'Page Faults = 1' \
    'Page Fault Rate = 0.333' \
    'TLB Hits = 2' \
    'TLB Hit Rate = 0.667' > esperado.txt
confere "endereco acima de 16 bits usa so os 16 bits baixos (95734 = 65536 + 30198)" \
    esperado.txt mascara.txt fifo fifo

echo "== tratamento de erro"

novo
cp "$RAIZ/addresses.txt" .
erro "sem argumentos" "numero de argumentos"
erro "so o arquivo" "numero de argumentos" addresses.txt
erro "falta o algoritmo da TLB" "numero de argumentos" addresses.txt lru
erro "argumento sobrando" "numero de argumentos" addresses.txt lru fifo fifo
erro "algoritmo de pagina desconhecido" "de pagina invalido" addresses.txt clock fifo
erro "algoritmo da TLB desconhecido" "da TLB invalido" addresses.txt lru random
erro "algoritmo em maiusculas" "de pagina invalido" addresses.txt LRU fifo
erro "algoritmo vazio" "da TLB invalido" addresses.txt lru ""
erro "argumentos fora de ordem" "da TLB invalido: 'addresses.txt'" lru fifo addresses.txt

erro "arquivo de enderecos inexistente" "nao foi possivel abrir" nao_existe.txt lru fifo
mkdir pasta
erro "diretorio no lugar do arquivo" "falha ao ler" pasta lru fifo
if [ "$(id -u)" -ne 0 ]; then
    cp addresses.txt sem_permissao.txt && chmod 000 sem_permissao.txt
    erro "arquivo sem permissao de leitura" "nao foi possivel abrir" sem_permissao.txt lru fifo
fi

: > vazio.txt
erro "arquivo vazio" "nenhum endereco" vazio.txt lru fifo
printf '\n  \n\n' > brancos.txt
erro "arquivo so com linhas em branco" "nenhum endereco" brancos.txt lru fifo
printf '16916\nabc\n' > letras.txt
erro "linha com letras" "linha 2: esperado um endereco" letras.txt lru fifo
printf '16916\n-5\n' > negativo.txt
erro "endereco negativo" "linha 2: esperado um endereco" negativo.txt lru fifo
printf '16916 62493\n' > dois.txt
erro "dois enderecos na mesma linha" "linha 1: texto inesperado" dois.txt lru fifo
printf '16916,62493\n' > virgula.txt
erro "enderecos separados por virgula" "linha 1: texto inesperado" virgula.txt lru fifo
printf '12.5\n' > decimal.txt
erro "numero com casa decimal" "linha 1: texto inesperado" decimal.txt lru fifo
printf '0x4214\n' > hexa.txt
erro "endereco em hexadecimal" "linha 1: texto inesperado" hexa.txt lru fifo
printf '4294967296\n' > grande.txt
erro "endereco maior que 32 bits" "nao cabe em 32 bits" grande.txt lru fifo
printf '99999999999999999999999999\n' > enorme.txt
erro "endereco que estoura unsigned long long" "nao cabe em 32 bits" enorme.txt lru fifo
awk 'BEGIN { for (i = 0; i < 200; i++) printf "1"; print "" }' > longa.txt
erro "linha com 200 digitos" "linha longa demais" longa.txt lru fifo
head -999 addresses.txt > fim_ruim.txt && echo 'fim' >> fim_ruim.txt
erro "erro so na ultima linha: nada e gravado" "linha 1000: esperado um endereco" fim_ruim.txt lru fifo
erro "arquivo binario no lugar dos enderecos" "linha" BACKING_STORE.bin lru fifo

rm BACKING_STORE.bin
erro "BACKING_STORE.bin ausente" "nao foi possivel abrir BACKING_STORE.bin" addresses.txt lru fifo
head -c 1000 "$RAIZ/BACKING_STORE.bin" > BACKING_STORE.bin
erro "BACKING_STORE.bin truncado" "tem menos de 65536 bytes" addresses.txt lru fifo

echo "== $pass ok, $fail falharam, $pulados pulados"
[ "$fail" -eq 0 ]
