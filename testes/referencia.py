#!/usr/bin/env python3
# Modelo de referencia do gerenciador de memoria virtual, escrito de forma
# independente do vm.c (dicionarios e listas, sem threads). Serve so para
# conferir a saida do vm nas combinacoes que nao tem gabarito do professor.
#
# uso: python3 referencia.py <enderecos> <fifo|lru> <fifo|lru> [BACKING_STORE]
# A saida vai para stdout no mesmo formato do correct.txt.
import sys

FRAMES, TLB = 128, 16


def simular(enderecos, memoria_secundaria, alg_pagina, alg_tlb):
    tabela = {}                    # pagina -> quadro
    pagina_do_quadro = [None] * FRAMES
    uso_quadro = [0] * FRAMES      # fifo: carga; lru: ultimo uso
    tlb = [None] * TLB             # (pagina, quadro)
    uso_tlb = [0] * TLB
    faltas = acertos = relogio = 0
    linhas = []
    for endereco in enderecos:
        relogio += 1
        pagina, desloc = (endereco >> 8) & 0xFF, endereco & 0xFF
        pos = next((i for i, e in enumerate(tlb) if e and e[0] == pagina), None)
        if pos is not None:
            acertos += 1
            quadro = tlb[pos][1]
            if alg_tlb == "lru":
                uso_tlb[pos] = relogio
            if alg_pagina == "lru":
                uso_quadro[quadro] = relogio
        else:
            if pagina in tabela:
                quadro = tabela[pagina]
                if alg_pagina == "lru":
                    uso_quadro[quadro] = relogio
            else:
                faltas += 1
                quadro = min(range(FRAMES), key=lambda q: uso_quadro[q])
                vitima = pagina_do_quadro[quadro]
                if vitima is not None:
                    del tabela[vitima]
                    tlb = [None if e and e[0] == vitima else e for e in tlb]
                pagina_do_quadro[quadro] = pagina
                tabela[pagina] = quadro
                uso_quadro[quadro] = relogio
            pos = min(range(TLB), key=lambda i: uso_tlb[i])
            tlb[pos] = (pagina, quadro)
            uso_tlb[pos] = relogio
        valor = memoria_secundaria[pagina * 256 + desloc]
        if valor > 127:
            valor -= 256
        linhas.append("Virtual address: %d TLB: %d Physical address: %d Value: %d"
                      % (endereco, pos, quadro * 256 + desloc, valor))
    total = len(enderecos)
    linhas.append("Number of Translated Addresses = %d" % total)
    linhas.append("Page Faults = %d" % faltas)
    linhas.append("Page Fault Rate = %.3f" % (faltas / total))
    linhas.append("TLB Hits = %d" % acertos)
    linhas.append("TLB Hit Rate = %.3f" % (acertos / total))
    return linhas


if __name__ == "__main__":
    arq, alg_pagina, alg_tlb = sys.argv[1:4]
    store = sys.argv[4] if len(sys.argv) > 4 else "BACKING_STORE.bin"
    with open(arq) as f:
        enderecos = [int(linha) for linha in f if linha.strip()]
    with open(store, "rb") as f:
        dados = f.read()
    print("\n".join(simular(enderecos, dados, alg_pagina, alg_tlb)))
