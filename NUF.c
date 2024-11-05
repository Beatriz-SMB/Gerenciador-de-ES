#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include "escalonadorPrioridade.h"

#define MAX_LINE_LENGTH 256
#define MAX_PROCESSES 100
#define MAX_SEQ_LENGTH 100
#define INT_MAX 100000

int iterador = 0;

// Dados dos processos
typedef struct {
    char nome_processo[50];
    int id,
        prioridade,
        tempo_execucao,
        qtdMemoria,
        sequencia[MAX_SEQ_LENGTH],
        tamanho_sequencia,
        estadoSequencia,
        latencia,
        trocasDePaginas,
        espacoMemoria,
        vezesAcessado;
} DadosProcessos;

typedef struct { // Estrutura que representa cada espaço da memória
    int process,
        page,
        frequency;
} Memory; 

char algDeEscalonamento[50];
char politicaDeMemoria[50];
int clockCPU, tamanhoMemoria, tamanhoPagina, percentualAlocacao, acessoPorCiclo, acessosNaMemoria, trocasPaginas;
int *primaryMemory;

typedef struct {
    int id,
        start,
        end;
} ProcessInMemory;

ProcessInMemory *memoryProcesses = NULL;  // Lista de processos atualmente na memoria
ProcessInMemory *oldestProcess = NULL;    // Primeiro a entrar na fila
ProcessInMemory *newestProcess = NULL;    // Ultimo a entrar na fila
int nOfProcessesMemory = 0;

Memory *pagesInMemory = NULL; // Lista que representa a memória

DadosProcessos *listaP = NULL; // Ponteiro para a lista de processos


//------------------------------------------- Algortimo de MEMORIA NUF --------------------------------------------------------------------------------------------
int findGlobalPageToReplace(Memory *pagesInMemory, int posicao, int idProcesso) { // Função de troca de páginas no escopo Global
    int frequency = INT_MAX;
    int page = INT_MAX;
    int id = INT_MAX;
    int position = -1;
    if(listaP[posicao].espacoMemoria > 0){// Verifica se o processo ainda tem direito de espaço na memória
        for (int i = 0; i < tamanhoMemoria / tamanhoPagina; i++) { // Se tiver, ele procura páginas nos outros processos
            if (pagesInMemory[i].process != -1 && pagesInMemory[i].process != idProcesso){ 
                // Verifica se o processo existe e se é diferente do processo atual
                if (pagesInMemory[i].frequency < frequency || 
                (pagesInMemory[i].frequency == frequency && pagesInMemory[i].process < id) || 
                (pagesInMemory[i].frequency == frequency && pagesInMemory[i].process == id && pagesInMemory[i].page < page)) {
                    // Verifica se a frequência é menor que a frequência atual e se for verifica qual processo tem menor id
                    frequency = pagesInMemory[i].frequency;
                    page = pagesInMemory[i].page;
                    id = pagesInMemory[i].process;
                    position = i;
                }
            }
        }
    }else{// Se não tiver, ele troca apenas dentro do processo
        for(int i = 0; i < tamanhoMemoria/tamanhoPagina; i++){
            if(pagesInMemory[i].process == idProcesso) {
                if (pagesInMemory[i].frequency < frequency || (pagesInMemory[i].frequency == frequency && pagesInMemory[i].page < page)) {
                    frequency = pagesInMemory[i].frequency;
                    page = pagesInMemory[i].page;
                    position = i;
                    }
                }
        }
    }

    
    return position; 
}

int findPageToReplace(Memory *pagesInMemory, int idProcesso){ // Função de troca de página no escopo Local
    int frequency = INT_MAX;
    int page = INT_MAX;
    int position = -1;
    for(int i = 0; i < tamanhoMemoria/tamanhoPagina; i++){
        if(pagesInMemory[i].process == idProcesso) {
            if (pagesInMemory[i].frequency < frequency || (pagesInMemory[i].frequency == frequency && pagesInMemory[i].page < page)) {
                // Verifica se a frequência é menor que a frequência e caso seja igual, pega a página de menor id
                frequency = pagesInMemory[i].frequency;
                page = pagesInMemory[i].page;
                position = i;
            }
        }
    }


    return position;
}


void NUF(DadosProcessos *listaP, int posicao, int tamanhoMemoria){ // Função do algoritmo NUF 

    for(int i = 0; i < clockCPU * acessoPorCiclo; i++) { // Loop que vai acessar x páginas
        int k = i;
        if (listaP[posicao].sequencia[i] == 0) {// Vai para a página que ainda não foi acessada na sequência de acessos
            k += (clockCPU * acessoPorCiclo) * listaP[posicao].vezesAcessado;
        }

        int posicaoMemoria = 0;
        while (posicaoMemoria < tamanhoMemoria / tamanhoPagina) { // While que percorre a memória
            // Verifica se tem espaço livre na memória para adicionar uma página
            if (pagesInMemory[posicaoMemoria].process == -1 && listaP[posicao].espacoMemoria > 0 && listaP[posicao].sequencia[k] != 0) {
                pagesInMemory[posicaoMemoria].process = listaP[posicao].id;
                pagesInMemory[posicaoMemoria].page = listaP[posicao].sequencia[k];
                printf("Página %d adicionada na posição %d\n\n", listaP[posicao].sequencia[k], posicaoMemoria);
                listaP[posicao].sequencia[k] = 0;
                listaP[posicao].espacoMemoria -= 1;
                pagesInMemory[posicaoMemoria].frequency = 1;
                break;
            }
            // Se a página já está na memória, incrementa a frequência de acessos
            else if (listaP[posicao].id == pagesInMemory[posicaoMemoria].process &&
                     listaP[posicao].sequencia[k] == pagesInMemory[posicaoMemoria].page) {
                pagesInMemory[posicaoMemoria].frequency += 1;
                listaP[posicao].sequencia[k] = 0;
                break;
            }
            posicaoMemoria++;
        }

        // Condicional que verifica que não há mais espaço livre na memória mas tem páginas para ser acessada ainda, e chama o algoritmo de substituição
        if ((posicaoMemoria >= tamanhoMemoria / tamanhoPagina || listaP[posicao].espacoMemoria <= 0) && listaP[posicao].sequencia[k] != 0) {
            if(strcmp(politicaDeMemoria, "local") == 0){ // Escopo local
                
                int posicaoTroca = findPageToReplace(pagesInMemory, listaP[posicao].id);

                if (posicaoTroca != -1) {
                trocasPaginas++;
                pagesInMemory[posicaoTroca].process = listaP[posicao].id;
                pagesInMemory[posicaoTroca].page = listaP[posicao].sequencia[k];
                pagesInMemory[posicaoTroca].frequency = 1;
                printf("Página %d adicionada na posição %d\n\n", listaP[posicao].sequencia[k], posicaoTroca);
                listaP[posicao].sequencia[k] = 0;
            }
                
            }else{ // Escopo local
                int posicaoTroca = findGlobalPageToReplace(pagesInMemory, posicao, listaP[posicao].id);

                if (posicaoTroca != -1) {
                trocasPaginas++;
                if(listaP[posicao].id != pagesInMemory[posicaoTroca].process){
                    listaP[posicao].espacoMemoria += 1;
                }
                pagesInMemory[posicaoTroca].process = listaP[posicao].id;
                pagesInMemory[posicaoTroca].page = listaP[posicao].sequencia[k];
                pagesInMemory[posicaoTroca].frequency = 1;
                printf("Página %d adicionada na posição %d\n\n", listaP[posicao].sequencia[k], posicaoTroca);
                listaP[posicao].sequencia[k] = 0;
            }
            }

        }


            
            
//------------------------------------------ Printa a memória ----------------------------------------------------------
            printf("|--------------------- Memória ---------------------|\n\n");
            for(int i = 0; i < tamanhoMemoria/tamanhoPagina; i++){
                if(pagesInMemory[i].process != -1){
                    printf("Posição: %d Processo: %d | Página: %d | Frequência: %d \n",i, pagesInMemory[i].process, pagesInMemory[i].page, pagesInMemory[i].frequency);
                }
            }
            printf("\n");
            printf("|---------------------------------------------------|\n\n");
            sleep(1);
        }
        listaP[posicao].vezesAcessado++;
        }

    




//------------------------------------------- LEITURA DO ARQUIVO DE ENTRADA E MANIPULACAO --------------------------------------------------------------------------------------------
// Função para ler os processos do arquivo
int read_process(const char *nome_arquivo, DadosProcessos *listaP) {
    FILE *file = fopen(nome_arquivo, "r");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    int i = 0;
    int lineCount = 0;
    char line[MAX_LINE_LENGTH];
    int controlador = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        // Remover o caractere de nova linha (\n), se existir
        line[strcspn(line, "\n")] = 0;

        if (controlador == 0) {
            // Ler as informações de configuração
            strcpy(algDeEscalonamento, strtok(line, "|"));
            clockCPU = atoi(strtok(NULL, "|"));
            strcpy(politicaDeMemoria, strtok(NULL, "|"));
            tamanhoMemoria = atoi(strtok(NULL, "|"));
            tamanhoPagina = atoi(strtok(NULL, "|"));
            percentualAlocacao = atoi(strtok(NULL, "|"));
            acessoPorCiclo = atoi(strtok(NULL, "|"));

            acessosNaMemoria = clockCPU * acessoPorCiclo;
            controlador++;

            pagesInMemory = realloc(pagesInMemory, tamanhoMemoria/tamanhoPagina * sizeof(Memory));

            for (int i = 0; i < tamanhoMemoria/tamanhoPagina; i++){
                pagesInMemory[i].process = -1;
                pagesInMemory[i].page = 0;
                pagesInMemory[i].frequency = 0;
                printf("Processo: %d | Página: %d | Frequência: %d \n", pagesInMemory[i].process, pagesInMemory[i].page, pagesInMemory[i].frequency);
            }

            int tamanhoUsado = tamanhoMemoria * (percentualAlocacao / 100);
            int *temp = realloc(primaryMemory,( (tamanhoMemoria) * sizeof(int)));
            primaryMemory = temp;
            printf("Memoria Principal com capacidade de %d bytes\n",tamanhoMemoria);
        } else {
            // Processar informações de cada processo
            strcpy(listaP[i].nome_processo, strtok(line, "|"));
            listaP[i].id = atoi(strtok(NULL, "|"));
            listaP[i].tempo_execucao = atoi(strtok(NULL, "|"));
            listaP[i].prioridade = atoi(strtok(NULL, "|"));
            listaP[i].qtdMemoria = atoi(strtok(NULL, "|"));
            listaP[i].estadoSequencia = 0;
            listaP[i].trocasDePaginas = 0;
            listaP[i].espacoMemoria = listaP[i].qtdMemoria / tamanhoPagina * 75/100;

            // Ler e armazenar a sequência
            char *sequencia_str = strtok(NULL, "|");
            char *token = strtok(sequencia_str, " ");
            listaP[i].tamanho_sequencia = 0;

            while (token != NULL) {
                listaP[i].sequencia[listaP[i].tamanho_sequencia++] = atoi(token);
                token = strtok(NULL, " ");
            }

            i++;
            lineCount++;
        }
    }

    fclose(file);
    return lineCount;
}

void show_process(DadosProcessos *listaP, int numeroProcessos) {
    for (int j = 0; j < numeroProcessos; j++) {
        printf("\nProcesso: %s\n", listaP[j].nome_processo);
        printf("ID: %d\n", listaP[j].id);
        printf("Tempo de Execução: %d\n", listaP[j].tempo_execucao);
        printf("Prioridade: %d\n", listaP[j].prioridade);
        printf("Qtd Memória: %d\n", listaP[j].qtdMemoria);
        printf("Sequência Acessos: ");
        for (int k = 0; k < listaP[j].tamanho_sequencia; k++) {
            printf("%d ", listaP[j].sequencia[k]);
        }
        printf("\n");
    }
}

//------------------------------------------- ESCALONADOR DE PROCESSO POR PRIORIDADE ---------------------------------------------------------------------------------------------
pthread_mutex_t mutex_prioridade;
//Função para criar um arquivo txt onde estarão armazenados o Id e a Latência de cada processo.
void criando_arquivo(){
    int i = 0;
    FILE* arquivo_3 = fopen("SaidaPrioridade.txt", "w");
    fprintf(arquivo_3, "ID | LATÊNCIA\n");

    while (i < iterador){
        if (listaP[i].id >= 0){
            fprintf(arquivo_3, "%d | ", listaP[i].id);
            fprintf(arquivo_3, "%d\n", listaP[i].latencia);
        }
        i++;
    }
}

//Função que recebe a lista de processos e executa-os. Durante o procedimento de executar um novo processo ele "trava" a função recebe_novos_processos usando um mutex.
void *executando_processos(void* arg){
    int latencia = 0, id_processo_anterior;
    int cpuClock = *(int*)arg;

    while(true){
        int maior_prioridade = 0, j = 0, k = 0, posicao = 0,  clock = *(int*)arg;;
        while(j < iterador){
            if (maior_prioridade < listaP[j].prioridade && listaP[j].tempo_execucao > 0){
                maior_prioridade = listaP[j].prioridade;
                posicao = j;
            }
            j++;     
        }

        if (maior_prioridade > 0){
            pthread_mutex_lock(&mutex_prioridade);

            if (listaP[posicao].tempo_execucao - clock >= 0){
                listaP[posicao].tempo_execucao = listaP[posicao].tempo_execucao - clock;
            }
            else{
                clock = listaP[posicao].tempo_execucao;
                listaP[posicao].tempo_execucao = 0;
            }

            while(k < iterador){
                if (listaP[k].prioridade > 0){
                listaP[k].latencia += clock;
                }
                k++;
            }
         
            if(listaP[posicao].tempo_execucao <= 0){ //Processo encerrou
                listaP[posicao].prioridade = 0;          
            }

            printf("Posição: %d\nId: %d\nTempo de execucao: %d\nPrioridade: %d\nLatencia: %d\nqtdMemoria: %d\n", 
            posicao,
            listaP[posicao].id, 
            listaP[posicao].tempo_execucao, 
            listaP[posicao].prioridade, 
            listaP[posicao].latencia,
            listaP[posicao].qtdMemoria);

            // Aplicacao do algoritmo de gerenciamento de memoria NUF
            NUF(listaP, posicao, tamanhoMemoria);
            
            if(listaP[posicao].tempo_execucao <= 0){ //Processo encerrou
                for (int i = 0; i < tamanhoMemoria/tamanhoPagina; i++){
                    if (listaP[posicao].id == pagesInMemory[i].process){
                        pagesInMemory[i].process = -1;
                        pagesInMemory[i].page = 0;
                        pagesInMemory[i].frequency = 0;
                    }
                }
                printf("Trocas de páginas: %d", trocasPaginas);
            }
            pthread_mutex_unlock(&mutex_prioridade); 

            printf("\n");
            sleep(1);
        }
        else{
            if ( listaP[iterador-1].id == -1 ){
                //printf("Thread executar encerrou \n");
                break;
            }
            printf("Todos os processos foram executados. Deseja encerrar? S \n");
            sleep(3);
        }
    }

}

//Função que recebe novos processos e armazena numa lista. Durante o procedimento de adicionar um novo processo ele "trava" a função executando_processos usando um mutex.
void *recebe_novos_processos(void* arg){
    char resposta, linha[50];
    int teste = 0;
    
    iterador = *(int*)arg;
    printf("Caso deseja inserir um novo processo siga o padrão: \nnome do processo|Id|Tempo de execução|Prioridade|QtdMemoria|Seq de Acessos\n");

    while (true){
        fgets(linha, sizeof(linha), stdin);

        pthread_mutex_lock(&mutex_prioridade);  

        iterador++;

        listaP = realloc(listaP, iterador * sizeof(DadosProcessos));
        
        char nome[50];
        int id, tempo, prioridade, qtdMemoria;
        char* sequencia_str;
        
        // Tenta ler os 5 campos principais
        int result = sscanf(linha, "%[^|]|%d|%d|%d|%d|%m[^\n]", nome, &id, &tempo, &prioridade, &qtdMemoria, &sequencia_str); 
        
        if (result == 6) {  // Certifique-se de que 6 campos foram lidos corretamente
            strcpy(listaP[iterador - 1].nome_processo, nome);
            listaP[iterador - 1].id = id;
            listaP[iterador - 1].tempo_execucao = tempo;
            listaP[iterador - 1].prioridade = prioridade;
            listaP[iterador - 1].qtdMemoria = qtdMemoria;
            listaP[iterador - 1].latencia = 0;
            listaP[iterador - 1].tamanho_sequencia = 0;
            listaP[iterador - 1].espacoMemoria = listaP[iterador -1].qtdMemoria / tamanhoPagina * 75/100;
            listaP[iterador - 1].vezesAcessado = 0;

            // Lê a sequência de acessos se existir
            if (sequencia_str != NULL) {
                char *token = strtok(sequencia_str, " ");
                while (token != NULL) {
                    listaP[iterador - 1].sequencia[listaP[iterador - 1].tamanho_sequencia++] = atoi(token);
                    token = strtok(NULL, " ");
                }
                free(sequencia_str);  // Libera a memória alocada pelo sscanf
            }

            // Exibe o processo adicionado
            printf("Novo processo adicionado: %s\n", listaP[iterador - 1].nome_processo);
            printf("Id: %d \n", listaP[iterador - 1].id);
            printf("Clock: %d \n", listaP[iterador - 1].tempo_execucao);
            printf("Prioridade: %d \n", listaP[iterador - 1].prioridade);
            printf("QtdMemoria: %d \n", listaP[iterador - 1].qtdMemoria);
            printf("Sequência de Acessos: ");
            for (int i = 0; i < listaP[iterador - 1].tamanho_sequencia; i++) {
                printf("%d ", listaP[iterador - 1].sequencia[i]);
            }
            printf("\n\n");
        }
        else {
            int result = sscanf(linha, "%s", nome);

            if(result == 1 && strcmp(nome, "s") == 0){
                listaP[iterador - 1].id = -1;
                //printf("Thread Adiconar encerrou \n");
                break;
            }
        }
        pthread_mutex_unlock(&mutex_prioridade);
    }
}

// Função para ler o arquivo e preencher os valores dos algoritmos
void lerArquivoEAtualizar(const char *algoritmo_atual, int trocas_pagina_atual) {
    FILE *arquivo = fopen("resultados.txt", "r+");
    char linha[MAX_LINE_LENGTH];
    int otimo = -1, fifo = -1, nuf = -1, mru = -1;
    char *token;

    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo.\n");
        return;
    }

    // Ler a linha do arquivo que contém os resultados
    if (fgets(linha, MAX_LINE_LENGTH, arquivo) != NULL) {
        // Quebrar a linha em tokens para identificar os valores de cada algoritmo
        token = strtok(linha, "|");

        while (token != NULL) {
            if (strstr(token, "OTIMO:") != NULL) {
                sscanf(token, "OTIMO: %d", &otimo);
            } else if (strstr(token, "FIFO:") != NULL) {
                sscanf(token, "FIFO: %d", &fifo);
            } else if (strstr(token, "NUF:") != NULL) {
                sscanf(token, "NUF: %d", &nuf);
            } else if (strstr(token, "MRU:") != NULL) {
                sscanf(token, "MRU: %d", &mru);
            }
            token = strtok(NULL, "|");
        }

        // Atualizar o valor do algoritmo atual, sem modificar os outros
        if (strcmp(algoritmo_atual, "FIFO") == 0) {
            fifo = trocas_pagina_atual;
        } else if (strcmp(algoritmo_atual, "NUF") == 0) {
            nuf = trocas_pagina_atual;
        } else if (strcmp(algoritmo_atual, "MRU") == 0) {
            mru = trocas_pagina_atual;
        } else if (strcmp(algoritmo_atual, "OTIMO") == 0) {
            otimo = trocas_pagina_atual;
        }
    }

    // Reescrever os valores atualizados na primeira linha do arquivo
    rewind(arquivo);
    fprintf(arquivo, "OTIMO: %d |FIFO: %d |NUF: %d |MRU: %d\n", 
            otimo != -1 ? otimo : -1, 
            fifo != -1 ? fifo : -1, 
            nuf != -1 ? nuf : -1, 
            mru != -1 ? mru : -1);

    // Comparar os valores para encontrar o algoritmo mais próximo de OTIMO
    int dif_fifo = (fifo != -1) ? abs(fifo - otimo) : -1;
    int dif_nuf = (nuf != -1) ? abs(nuf - otimo) : -1;
    int dif_mru = (mru != -1) ? abs(mru - otimo) : -1;

    int menor_diferenca = -1;
    const char *algoritmo_mais_proximo = NULL;

    // Definir o algoritmo mais próximo e verificar empates
    if (dif_fifo != -1) {
        menor_diferenca = dif_fifo;
        algoritmo_mais_proximo = "FIFO";
    }
    if (dif_nuf != -1 && (menor_diferenca == -1 || dif_nuf < menor_diferenca)) {
        menor_diferenca = dif_nuf;
        algoritmo_mais_proximo = "NUF";
    } else if (dif_nuf == menor_diferenca) {
        algoritmo_mais_proximo = "EMPATE";
    }
    if (dif_mru != -1 && (menor_diferenca == -1 || dif_mru < menor_diferenca)) {
        menor_diferenca = dif_mru;
        algoritmo_mais_proximo = "MRU";
    } else if (dif_mru == menor_diferenca && strcmp(algoritmo_mais_proximo, "EMPATE") != 0) {
        algoritmo_mais_proximo = "EMPATE";
    }

    // Escrever a segunda linha no arquivo com o resultado
    fprintf(arquivo, "Algoritmo mais próximo do OTIMO: %s\n", algoritmo_mais_proximo);

    fclose(arquivo);

    printf("Arquivo atualizado com sucesso.\n");
}



int main() {
    // Alocar memória para a lista de processos
    listaP = (DadosProcessos *)malloc(MAX_PROCESSES * sizeof(DadosProcessos));
    if (listaP == NULL) {
        perror("Erro ao alocar memória para a lista de processos");
        return EXIT_FAILURE;
    }
    // Chamar a função para ler processos do arquivo
    int numeroProcessos = read_process("entradaMemoria.txt", listaP);
    if (numeroProcessos == -1) {
        free(listaP);
        return EXIT_FAILURE;
    }

    // char algoritmo_atual[] = "NUF";

    // Imprime os valores dos processos
    show_process(listaP, numeroProcessos);

    printf("\nAlgoritmo NUF\n");
    iterador = numeroProcessos;

    pthread_t executando_processo, lendo_novo_processo;
    pthread_mutex_init(&mutex_prioridade, NULL);

    pthread_create(&lendo_novo_processo, NULL, &recebe_novos_processos, &iterador);
    pthread_create(&executando_processo, NULL, &executando_processos, &clockCPU);

    pthread_join(executando_processo, NULL);
    pthread_cancel(lendo_novo_processo);

    lerArquivoEAtualizar(algoritmo_atual, trocasPaginas);

    pthread_mutex_destroy(&mutex_prioridade);

    free(pagesInMemory);
    free(listaP);
    free(oldestProcess);

    return 0;
}
