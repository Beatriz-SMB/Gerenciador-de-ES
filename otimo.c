#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 256
#define MAX_PROCESSES 100
#define MAX_SEQ_LENGTH 100

int iterador = 0;

// Dados dos processos
typedef struct {
    char nome_processo[50];
    int id,
        prioridade,
        tempo_execucao,
        qtdMemoria,
        estadoSequencia,
        latencia,
        trocasDePaginas;
    int capacity;
    int *pageSequence; 
    int pageAmount;
    int positionCounter;
} DadosProcessos;

char algDeEscalonamento[50];
char politicaDeMemoria[50];
int clockCPU, tamanhoMemoria, tamanhoPagina, percentualAlocacao, acessoPorCiclo, acessosNaMemoria;
int memorySize = 0;
int memory_count = 0;
char method[50];
int changeCounter = 0;
// int *primaryMemory;

typedef struct st_memory{
    int processId;
    int page;
}st_memory;

DadosProcessos *listaP = NULL; // Ponteiro para a lista de processos
st_memory *memory = NULL;

//------------------------------------------- Algortimo de MEMORIA ÓTIMO --------------------------------------------------------------------------------------------
void read_sequence(DadosProcessos *p, char *str) {

    char *temp = strdup(str);
    char *token = strtok(temp, " ");
    p -> pageAmount = 0;
    
    while (token != NULL) {
        p -> pageAmount++;
        token = strtok(NULL, " ");
    }
    free(temp);

    p -> pageSequence = (int *)malloc(p -> pageAmount * sizeof(int));

    token = strtok(str, " ");
    int i = 0;
    while (token != NULL) {
        p -> pageSequence[i++] = atoi(token);
        token = strtok(NULL, " ");
    }
}

void remove_page(DadosProcessos *p) {
    if (p->pageAmount == 0) {
        printf("Nao ha paginas para remover.\n");
        return;
    }

    for (int i = 0; i < p->pageAmount - 1; i++) {
        p->pageSequence[i] = p->pageSequence[i + 1];
    }

    p->pageAmount--;

    p->pageSequence = (int *)realloc(p->pageSequence, p->pageAmount * sizeof(int));
}

int isPageInMemory(int id, int page) {
    for (int i = 0; i < memory_count; i++) {
        if (memory[i].page == page && memory[i].processId == id) {
            return 1;  
        }
    }
    return 0;
}

int findGreatReplacementLocal(int index, int process_id) {
    int replace_index = -1; 
    int farthest = 1;

    for (int i = 0; i < memory_count; i++) {
        if (memory[i].processId == process_id) {
            int j;
            for (j = 1; j < listaP[index].pageAmount; j++) {
                if (memory[i].page == listaP[index].pageSequence[j]) {
                    if (j > farthest) {
                        farthest = j;  
                        replace_index = i;
                    }
                    break;
                }
            }
            if (j == listaP[index].pageAmount) {
                return i;
            }
        }
    }

    return (replace_index == -1) ? 0 : replace_index;
}

int findGreatReplacementGlobal(int index, int process_id) {
    int replace_index = -1; 
    int farthest = 1;

    for (int i = 0; i < memory_count; i++) {
        int process_id = memory[i].processId; 
        int j;

        for (j = 1; j < listaP[index].pageAmount; j++) {
            if (memory[i].page == listaP[index].pageSequence[j]) {
                if (j > farthest) {
                    farthest = j;  
                    replace_index = i;
                }
                break;
            }
        }

        if (j == listaP[index].pageAmount) {
            return i;
        }
    }

    return (replace_index == -1) ? 0 : replace_index;
}

int insertPageInMemory(int index, int process_id, int page) {

    if (!isPageInMemory(process_id, page)) {
        if (listaP[index].positionCounter < listaP[index].capacity) {
            if (memory_count < memorySize) {
                memory[memory_count].page = page;  
                memory[memory_count].processId = process_id; 
                memory_count++;
                listaP[index].positionCounter++;
            } else {
                if (strcmp(method, "local") == 0) {
                    printf("Metodo local.\n");
                    int replace_index = findGreatReplacementLocal(index, process_id);
                    printf("Pagina %d sera colocana na posicao %d.\n", page, replace_index);
                    memory[replace_index].page = page;
                    changeCounter++;
                } else {
                    printf("Metodo global.\n");
                    int replace_index = findGreatReplacementGlobal(index, process_id);
                    printf("Pagina %d sera colocana na posicao %d.\n", page, replace_index);
                    memory[replace_index].page = page;
                    listaP[index].positionCounter++;
                    changeCounter++;
                } 
            }
            
        } else {
            printf("Metodo local.\n");
            int replace_index = findGreatReplacementLocal(index, process_id);
            printf("Pagina %d sera colocana na posicao %d.\n", page, replace_index);
            memory[replace_index].page = page;
            changeCounter++;
        }
    }

    printf("Acesso a pagina %d -", page);
    printf("\n");
    for (int k = 0; k < memory_count; k++) {
        printf("%d - p%d,  ", memory[k].page, memory[k].processId); 
    }
    printf("\n");
}

void removePagesFromMemory(int process_id) {
    int i = 0;

    while (i < memory_count) {
        if (memory[i].processId == process_id) {
            printf("Removendo pagina %d do processo %d na posicao %d.\n", memory[i].page, process_id, i);
            
            // Shift para a esquerda: compacta o array de memória
            for (int j = i; j < memory_count - 1; j++) {
                memory[j] = memory[j + 1];
            }
            
            memory_count--; 
        } else {
            i++; 
        }
    }
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
            strcpy(method, strtok(NULL, "|"));
            tamanhoMemoria = atoi(strtok(NULL, "|"));
            tamanhoPagina = atoi(strtok(NULL, "|"));
            percentualAlocacao = atoi(strtok(NULL, "|"));
            acessoPorCiclo = atoi(strtok(NULL, "|"));

            acessosNaMemoria = clockCPU * acessoPorCiclo;
            controlador++;

            memorySize = tamanhoMemoria / tamanhoPagina;
            memory = malloc(memorySize * sizeof(st_memory));

            printf("Memoria Principal com capacidade de %d bytes\n",tamanhoMemoria);
        } else {
            // Processar informações de cada processo
            strcpy(listaP[i].nome_processo, strtok(line, "|"));
            listaP[i].id = atoi(strtok(NULL, "|"));
            listaP[i].tempo_execucao = atoi(strtok(NULL, "|"));
            listaP[i].prioridade = atoi(strtok(NULL, "|"));
            listaP[i].qtdMemoria = atoi(strtok(NULL, "|"));
            listaP[i].capacity = ((listaP[i].qtdMemoria / tamanhoPagina) * percentualAlocacao / 100 );
            listaP[i].positionCounter = 0;

            char *page_sequence_str = strtok(NULL, "|");

            read_sequence(&listaP[i], page_sequence_str);

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
        printf("Tempo de Execucao: %d\n", listaP[j].tempo_execucao);
        printf("Prioridade: %d\n", listaP[j].prioridade);
        printf("Qtd Memoria: %d\n", listaP[j].qtdMemoria);
        printf("Capacidade: %d\n", listaP[j].capacity);
        printf("Sequencia Acessos: ");
        for (int k = 0; k < listaP[j].pageAmount; k++) {
            printf("%d ", listaP[j].pageSequence[k]);
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

        if (maior_prioridade > 0) {
            pthread_mutex_lock(&mutex_prioridade);
            printf("\n");
            printf("Processo de ID: %d\n", listaP[posicao].id);
            printf("\n");
            if (listaP[posicao].tempo_execucao - clock >= 0) {
                for (int j = 0; j < clock; j++) {
                    for (int k = 0; k < acessoPorCiclo; k++) {
                        insertPageInMemory(posicao, listaP[posicao].id, listaP[posicao].pageSequence[0]);
                        remove_page(&listaP[posicao]);
                    }
                }
                listaP[posicao].tempo_execucao = listaP[posicao].tempo_execucao - clock;
            } else {
                for (int j = 0; j < listaP[posicao].tempo_execucao; j++) {
                    for (int k = 0; k < acessoPorCiclo; k++) {
                        insertPageInMemory(posicao, listaP[posicao].id, listaP[posicao].pageSequence[0]);
                        remove_page(&listaP[posicao]);
                    }
                }
                clock = listaP[posicao].tempo_execucao;
                listaP[posicao].tempo_execucao = 0;
            }

            if (listaP[posicao].tempo_execucao == 0) {
                printf("\n");
                removePagesFromMemory(listaP[posicao].id);
            }

            while(k < iterador) {
                if (listaP[k].prioridade > 0) {
                    listaP[k].latencia += clock;
                }
                k++;
            }
         
            if (listaP[posicao].tempo_execucao <= 0) { //Processo encerrou

                listaP[posicao].prioridade = 0;          
            }
            
            pthread_mutex_unlock(&mutex_prioridade); 

            printf("\n");
            sleep(1);
        }
        else{
            if ( listaP[iterador-1].id == -1 ){
                printf("Thread executar encerrou \n");
                break;
            }
            printf("Total de troca de paginas: %d \n", changeCounter);
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
    printf("Caso deseja inserir um novo processo siga o padrao: \nnome do processo|Id|Tempo de execucao|Prioridade|QtdMemoria|Seq de Acessos\n");

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
            listaP[iterador - 1].pageAmount = 0;
            listaP[iterador - 1].capacity = ((qtdMemoria / tamanhoPagina) * percentualAlocacao / 100 );

            char *page_sequence_str = strtok(NULL, "|");

            read_sequence(&listaP[iterador - 1], page_sequence_str);

            // Exibe o processo adicionado
            printf("Novo processo adicionado: %s\n", listaP[iterador - 1].nome_processo);
            printf("Id: %d \n", listaP[iterador - 1].id);
            printf("Clock: %d \n", listaP[iterador - 1].tempo_execucao);
            printf("Prioridade: %d \n", listaP[iterador - 1].prioridade);
            printf("Qtd Memoria: %d \n", listaP[iterador - 1].qtdMemoria);
            printf("Sequencia de Acessos: ");
            for (int i = 0; i < listaP[iterador - 1].pageAmount; i++) {
                printf("%d ", listaP[iterador - 1].pageSequence[i]);
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

    if (listaP == NULL || memory) {
        perror("Erro ao alocar memória");
        return EXIT_FAILURE;
    }

    // Chamar a função para ler processos do arquivo
    int numeroProcessos = read_process("entradaMemoria.txt", listaP);
    if (numeroProcessos == -1) {
        free(listaP);
        return EXIT_FAILURE;
    }
    char algoritmo_atual[] = "OTIMO";

    // Imprime os valores dos processos
    show_process(listaP, numeroProcessos);

    printf("\nAlgoritmo Otimo\n");
    iterador = numeroProcessos;
    

    pthread_t executando_processo, lendo_novo_processo;
    pthread_mutex_init(&mutex_prioridade, NULL);

    pthread_create(&lendo_novo_processo, NULL, &recebe_novos_processos, &iterador);
    pthread_create(&executando_processo, NULL, &executando_processos, &clockCPU);

    pthread_join(executando_processo, NULL);
    pthread_cancel(lendo_novo_processo);

    pthread_mutex_destroy(&mutex_prioridade);

    free(listaP);

    return 0;
}
