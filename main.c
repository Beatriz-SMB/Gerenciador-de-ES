#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "escalonadorPrioridade.h"

#define TRUE 1

int main() {    
    printf("Arquivo principal\n");

    const char *filename = "entrada_ES.txt";
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    // Lê o algoritmo e o clock
    char *alg;
    int clock_cpu;
    char linha[100];
    if (fgets(linha, sizeof(linha), fp)) {
        alg = strtok(linha, "|");
        printf("O algoritmo é: %s\n", alg);
    }
    
    if (strcmp(alg, "prioridade") == 0) {
        escalonadorPrioridade();
    } 
    else {
        printf("Erro: %s\n", alg);
    }

    return 0;
}