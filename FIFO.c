#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <semaphore.h>

#define MAX_LINE_LENGTH 256
#define MAX_PROCESSES 100
#define MAX_DEVICES 50
#define MAX_SEQ_LENGTH 100


// Dados dos processos
typedef struct{
    char nome_processo[50];
    int id,
        prioridade,
        tempo_execucao,
        qtdMemoria,
        sequencia[MAX_SEQ_LENGTH],
        tamanho_sequencia,
        chanceRequisitarES,
        estadoSequencia,
        latencia,
        trocasDePaginas,
        moldurasUsadas,
        tempo_pronto,
        tempo_bloqueado,
        status; // Status do processos (0 para pronto e 1 para bloqueado)
} DadosProcessos;

// Dados ficarao dentro de cada posicao da lista de espera por dispositivos
typedef struct{
    int idProcesso,
        tempoES,
        tempoRestante,
        status;
} ProcessoEmEspera;

// Dados dos dispositivos
typedef struct{
    int idDispositivo,
        numUsosSimultaneos,     
        tempoOperacao,
        numOfUses,
        tamanhoLista; // numero de dispositivos que esta sendo usado no momento
    ProcessoEmEspera *listaEspera;
    int *listaEmUso;  // armazena os Ids dos processo que estao usando o dispositivo
} DadosDispositivos;


pthread_mutex_t mutex_devices;  // mutex para lista de dados dos dispositivos
pthread_mutex_t mutex_clock;    // mutex para o clock da CPU
sem_t semaphoreClock;

DadosProcessos *listaP = NULL; // Ponteiro para a lista de processos PRONTOS
DadosDispositivos *listaD = NULL; // Ponteiro para a lista de dispositivos

// Variaveis globais
char algDeEscalonamento[50];
char politicaDeMemoria[50];
int clockCPU,
    clockAtualCPU = 0,
    tamanhoMemoria, 
    tamanhoPagina, 
    percentualAlocacao, 
    acessoPorCiclo, 
    numDispositivosES,          
    acessosNaMemoria,
    moldurasTotais,
    numProcessos = 0;


//------------------------------------------- Algortimo de Gerenciador de E/S --------------------------------------------------------------------------------------------

// Função que sorteia se o processo vai fazer E/S
int sorteia_numero(int porcentagem){

    int numeroAleatorio = rand() % 100;
    // printf("Número sorteado %d", numeroAleatorio);
    // printf("Numero sorteado: %d\n", numeroAleatorio);
    if (porcentagem > numeroAleatorio) {
        return 1;
    } else {
        return 0;
    }
}

// Função que sorteia dispositivo
int sorteia_dispositivo(){
    int dispositivo = rand() % numDispositivosES;
    return dispositivo;
}

// Função que gerencia o dispositivo que vai fazer E/S
void gerencia_es( int id){ // recebe id do processo
    int device = sorteia_dispositivo();
    listaD[device].listaEspera = realloc(listaD[device].listaEspera, (listaD[device].tamanhoLista + 1) * sizeof(ProcessoEmEspera));
    if (listaD[device].listaEspera == NULL) {
        printf("Erro ao alocar memória.\n");
        exit(1);
    }

    ProcessoEmEspera processoEmEspera;
    processoEmEspera.idProcesso = id;                              // id do processo
    // processoEmEspera.tempoES = tempoES;                            // Tempo em que fara E/S
    processoEmEspera.tempoRestante = listaD[device].tempoOperacao; // Tempo total que usara
    processoEmEspera.status = 0;                                   // Em espera pelo dispositivo

    listaD[device].listaEspera[listaD[device].tamanhoLista] = processoEmEspera;
    //printf("Adicionado o processo (%d) na posicao (%d)\n",id, listaD[device].tamanhoLista);
    listaD[device].tamanhoLista++;
    return;
    printf("\n");
}

void imprimi_processos_executando(int posicao){
    printf("\n--- PROCESSO EM EXECUÇÃO ---\n");
    printf("Id: %d; ", listaP[posicao].id);
    printf("Tempo de restante: %d; ",(listaP[posicao].tempo_execucao > 0) ? listaP[posicao].tempo_execucao : 0 ); 
    printf("Latencia: %d\n", listaP[posicao].latencia);
}

void imprimi_processos_prontos(int posicao){
    printf("--- PROCESSOS PRONTOS --- \n");
    int k = 0;
    while(k < numProcessos){
        if(listaP[k].status == 0 && listaP[k].tempo_execucao > 0 && k != posicao){
            printf("Id: %d; ", listaP[k].id);
            printf("Tempo de restante: %d;\n", listaP[k].tempo_execucao); 
        }
        k++;     
    }
}

void imprimi_processos_bloqueados(){
    printf("--- PROCESSOS BLOQUEADOS --- \n");
    int k = 0;
    while(k < numProcessos){
        if(listaP[k].status == 1){
            // Para todos deve ser informado o tempo de CPU restante para sua conclusão.
            printf("Id: %d; ", listaP[k].id);
            printf("Tempo de restante: %d; ", listaP[k].tempo_execucao); 
            printf("Latencia: %d; ", listaP[k].latencia); 
            // usar mutex para ver qual disp esta usando ou esperando para usar 
            pthread_mutex_lock(&mutex_devices);
            for (int i = 0; i < numDispositivosES; i++){
                for (int j = 0; j < listaD[i].tamanhoLista; j++){
                    if (listaD[i].listaEspera[j].idProcesso == listaP[k].id){
                        printf("%s dispositivo %d;  \n",
                            (listaD[i].listaEspera[j].status == 0) ? "Esperando" : "Usando",
                            listaD[i].idDispositivo); 
                    }   
                }
            }
            pthread_mutex_unlock(&mutex_devices);
            printf("\n");
        }
        k++;     
    }
}

void remove_position(int device, int pos) {
    for (int i = pos; i < listaD[device].tamanhoLista - 1; i++) {
        listaD[device].listaEspera[i] = listaD[device].listaEspera[i + 1];
    }

    // diminui o tamnho da lista de espera
    listaD[device].tamanhoLista--;
    if (listaD[device].tamanhoLista > 0) {
        listaD[device].listaEspera = realloc(listaD[device].listaEspera, listaD[device].tamanhoLista * sizeof(ProcessoEmEspera));
        if (listaD[device].listaEspera == NULL) {
            printf("Erro ao realocar memória.\n");
            exit(1);
        }
    }
}
//------------------------------------------- Algortimo de MEMORIA FIFO --------------------------------------------------------------------------------------------
typedef struct {
    int id,
        page,
        localPosition,
        globalPosition;
} Memory;

Memory *primaryMemory = NULL; // Ponteiro para a lista de processos
int nOfPagesInMemory = 0;
int trocasDePaginas = 0;

// Imprime o conteudo da memoria principal
void print_primary_memory() {
    // printf("|----------------------------------------------------------------|\n");
    // printf("|--------------------------- Memória Pricipal -------------------|\n");
    // for (int i = 0; i < moldurasTotais; i++){
    //     printf("Posicao: %d | ID: %d | Page: %d | localPosition %d | globalPosition %d\n",i,primaryMemory[i].id, primaryMemory[i].page, primaryMemory[i].localPosition, primaryMemory[i].globalPosition);
    // }
}

void initMemory(){
    printf("\n--- Memoria iniciada ---\n");
    for (int i = 0; i < moldurasTotais; i++){
        primaryMemory[i].id = -1;
        primaryMemory[i].page = -1;
        primaryMemory[i].localPosition = -1;
        primaryMemory[i].globalPosition = -1;
    }
    print_primary_memory();
}

int pageInMemory(int id, int page){
    for (int i = 0; i < moldurasTotais; i++){
        if(primaryMemory[i].id == id && primaryMemory[i].page == page ){
            return 1;
        }
    }
    return 0;
}

int processInMemory(int id){
    for (int i = 0; i < moldurasTotais; i++){
        if(primaryMemory[i].id == id){
            return 1;
        }
    }
    return 0;
}

int LocalPriority(int id){  // Percorre a memoria e calcula quantos locais ja tem aquele processo
    int count = 1;
    for (int i = 0; i < moldurasTotais; i++){
        if(primaryMemory[i].id == id){
            count++;
        }
    }
    // printf("Prioridade do processo: %d \n",count);
    return count;
}

int freePosition(){  // Percorre a memoria e calcula quantos locais ja tem aquele processo
    for (int position = 0; position < moldurasTotais; position++){
        if(primaryMemory[position].id == -1){
            // printf("free: %d \n", position);
            return position;
        }
    }
    return -1;
}

void reordenaLocalPrioridade(int id){
    for (int i = 0; i < moldurasTotais; i++){
        if(primaryMemory[i].id == id){
            primaryMemory[i].localPosition--;
        }
    }
}

void reordenaGlobalPrioridade(int posicao){
    for (int i = 0; i < moldurasTotais; i++){
        if(primaryMemory[i].id != -1 && primaryMemory[i].globalPosition >= posicao){
            primaryMemory[i].globalPosition--;
        }
    }
}

void addLocalMemory(int id, int page, int posicao){
    int pos = LocalPriority(id); // Calcula a prioridade do novo processo

    reordenaLocalPrioridade(id); // Diminui uma prioridade de todos 
    pos--;
    
    reordenaGlobalPrioridade(posicao); // Diminui uma prioridade de todos 

    int oldpage = primaryMemory[posicao].page;
    primaryMemory[posicao].page = page;
    primaryMemory[posicao].localPosition = pos;
    primaryMemory[posicao].globalPosition = nOfPagesInMemory;
    printf("Substituido página %d por pagina %d do processo %d na posição %d da memória\n", oldpage, page, id, posicao);
}

void libera_memoria(int id){
    for (int i = 0; i < moldurasTotais; i++){
        if(primaryMemory[i].id == id){
            primaryMemory[i].id = -1;
            primaryMemory[i].page = -1;
            primaryMemory[i].localPosition = -1;
            primaryMemory[i].globalPosition = -1;
            nOfPagesInMemory--;
        }
    }
}

int addPageIntoMemory(int id, int page){
    int freeP = freePosition();
    int pos = LocalPriority(id); 
    nOfPagesInMemory++;

    printf("Adicionando página %d do processo %d na posição %d da memória\n", page, id, freeP);
    primaryMemory[freeP].id = id;
    primaryMemory[freeP].page = page;
    primaryMemory[freeP].localPosition = pos;
    primaryMemory[freeP].globalPosition = nOfPagesInMemory;
    return freeP; //retorna a posicao alocada
}

int findOldestLocal(int id) {
    int current = -1;
    int smallest = 0;
    int key = 0;
    for (int i = 0; i < moldurasTotais; i++){
        if(primaryMemory[i].id == id){
            current = primaryMemory[i].localPosition;
            if(!key){
                key = 1;
                smallest = primaryMemory[i].localPosition;
            }else{
                if(current < smallest){
                    smallest = current;
                }   
            }
        }
    }
    for (int i = 0; i < moldurasTotais; i++){
        if(primaryMemory[i].id == id && primaryMemory[i].localPosition == smallest){
            return i; // retorna a posicao desse com menor prioridade (mais velho)
        }
    }

    return -1;
}

int findOldestGlobal() {
    int current = -1;
    int smallest = primaryMemory[0].globalPosition;
    int pos = 0;
    for (int i = 1; i < moldurasTotais; i++){
        current = primaryMemory[i].globalPosition;
        if(current < smallest){
            smallest = current;
            pos = i;
        }
    }

    return pos;
}

void addGlobalMemory(int id, int page, int pos,int posicaoP){
    int posicaoOldProcess;
    int newPosition = 1;
    for (int i = 0; i < moldurasTotais; i++){
        if(i == pos){
            posicaoOldProcess = i;
        }
        if(id == primaryMemory[i].id){
            newPosition++;
        }
    }
    int oldId = primaryMemory[posicaoOldProcess].id;
    for (int i = 0; i < MAX_PROCESSES; i++){
        if(oldId == listaP[i].id){
            listaP[i].moldurasUsadas--; //Libera uma moldura do processo que estava naquela posicao
            // printf("id %d mold usadas %d\n",listaP[i].id, listaP[i].moldurasUsadas); //Libera uma moldura do processo que estava naquela posicao
        }
    }

    primaryMemory[pos].id = id;
    primaryMemory[pos].page = page;
    primaryMemory[pos].localPosition = newPosition;
    primaryMemory[pos].globalPosition = nOfPagesInMemory;
    listaP[posicaoP].moldurasUsadas++;

    for (int i = 0; i < moldurasTotais; i++){
        if(i != pos){
            primaryMemory[i].globalPosition--;
        }
        if (oldId == primaryMemory[i].id){
            primaryMemory[i].localPosition--;
        }
    }
}

void politicaLocal(int id, int page){
    int pos = findOldestLocal(id);
    // printf("pos oldest local %d \n",pos);
    addLocalMemory(id,page,pos);
}

void politicaGlobal(int id, int page,int posicaoP){
    int pos = findOldestGlobal(id);
    addGlobalMemory(id,page,pos,posicaoP);
}


void FIFO(DadosProcessos *listaP, int posicao) {
    int id = listaP[posicao].id;
    int qtdMemoria = listaP[posicao].qtdMemoria;
    int moldurasDisponiveis = (((qtdMemoria / tamanhoPagina) * percentualAlocacao) / 100);
    if (moldurasDisponiveis > moldurasTotais){
        moldurasDisponiveis = moldurasTotais;
    }
    // Caso nao esteja na memoria, o processo sera adicionado
    printf("\nSequencia de acessos a memoria: ");
    for(int i = listaP[posicao].estadoSequencia; i < (listaP[posicao].estadoSequencia + acessosNaMemoria); i++){
        int page = listaP[posicao].sequencia[i];
        if(page != -1 && pageInMemory(id,page)){
            printf("%d ",page);
        } 
        else if(page != -1 && page!= 0 && !pageInMemory(id,page)){ // Pagina nao esta na memoria
            printf("\nPagina %d nao esta na memoria\n",page);
            if(!processInMemory(id)){
                if (freePosition() != -1){
                    int posicaoAlocada = addPageIntoMemory(id,page);
                    listaP[posicao].moldurasUsadas++;
                    print_primary_memory();
                }else{
                    printf("Não é possível armazenar Memoria cheia\n");
                }
            }
            else{ //Processo ja esta na memoria
                if(listaP[posicao].moldurasUsadas < moldurasDisponiveis && freePosition() != -1){// Aloca em um novo espaco
                    int posicaoAlocada = addPageIntoMemory(id,page);
                    listaP[posicao].moldurasUsadas++;
                }
                else{  // Memoria cheia e precisa de troca de pagina
                    if (strcmp(politicaDeMemoria, "local") == 0) {
                        if(!processInMemory(id) && freePosition() == -1){
                            printf("Não é possível armazenar\n");
                        }else{
                            politicaLocal(id,page);
                            listaP[posicao].trocasDePaginas++;
                        }
                    }else{
                        if(listaP[posicao].moldurasUsadas >= moldurasDisponiveis){ //se torna local
                            politicaLocal(id,page); //Deve SER LOCAL AGORA
                            listaP[posicao].trocasDePaginas++;
                        }else{ 
                            politicaGlobal(id,page,posicao);
                            listaP[posicao].trocasDePaginas++;
                        }
                    }
                }
                print_primary_memory();
            }
        }
    }
    listaP[posicao].estadoSequencia += acessosNaMemoria;
    printf("\n");
}

//------------------------------------------- LEITURA DO ARQUIVO DE ENTRADA E MANIPULACAO --------------------------------------------------------------------------------------------
// Função para ler os processos do arquivo
int read_process(const char *nome_arquivo, DadosProcessos *listaP, DadosDispositivos *listaD) {
    FILE *file = fopen(nome_arquivo, "r");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    int i = 0;
    int j = 0;
    int lineCount = 0;
    char line[MAX_LINE_LENGTH];
    int controlador = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        // Remover o caractere de nova linha (\n), se existir
        line[strcspn(line, "\n")] = 0;

        if (controlador == 0) {
            // Ler as informações de configuração
            //Variaveis globais
            strcpy(algDeEscalonamento, strtok(line, "|"));
            clockCPU = atoi(strtok(NULL, "|"));
            strcpy(politicaDeMemoria, strtok(NULL, "|"));
            tamanhoMemoria = atoi(strtok(NULL, "|"));
            tamanhoPagina = atoi(strtok(NULL, "|"));
            percentualAlocacao = atoi(strtok(NULL, "|"));
            acessoPorCiclo = atoi(strtok(NULL, "|"));
            numDispositivosES = atoi(strtok(NULL, "|"));
            
            acessosNaMemoria = clockCPU * acessoPorCiclo;
            moldurasTotais = tamanhoMemoria / tamanhoPagina;
            controlador++;

            int tamanhoUsado = tamanhoMemoria * (percentualAlocacao / 100);
            // Alocar memória para a lista de processos
            primaryMemory = (Memory *)malloc(moldurasTotais * sizeof(Memory));

            printf("Memoria Principal com capacidade de %d bytes\n",tamanhoMemoria);
        } else if (controlador <= numDispositivosES) {
            // Processar informações de cada dispositivo
            char *nome_dispositivo = strtok(line, "|");
            char *id_str = strchr(nome_dispositivo, '-') + 1; 

            if (id_str != NULL) {
                listaD[j].idDispositivo = atoi(id_str); 
            }
            listaD[j].numUsosSimultaneos = atoi(strtok(NULL, "|"));
            listaD[j].tempoOperacao = atoi(strtok(NULL, "|"));
            listaD[j].numOfUses = 0;
            listaD[j].listaEspera = NULL; 
            listaD[j].listaEmUso = (int*)malloc(listaD[j].numUsosSimultaneos*sizeof(int)); 
            for (int k = 0; k < listaD[j].numUsosSimultaneos; k++) {
                listaD[k].listaEmUso[k] = 0;
            }
            listaD[j].tamanhoLista = 0;

            j++;
            controlador++;
        } else {
            // Processar informações de cada processo
            strcpy(listaP[i].nome_processo, strtok(line, "|"));
            listaP[i].id = atoi(strtok(NULL, "|"));
            listaP[i].tempo_execucao = atoi(strtok(NULL, "|"));
            listaP[i].prioridade = atoi(strtok(NULL, "|"));
            listaP[i].qtdMemoria = atoi(strtok(NULL, "|"));

            listaP[i].estadoSequencia = 0;
            listaP[i].trocasDePaginas = 0;
            listaP[i].moldurasUsadas = 0;
            listaP[i].status = 0;
            listaP[i].tempo_bloqueado = 0;
            listaP[i].tempo_pronto = 0;

            // Ler e armazenar a sequência
            char *sequencia_str = strtok(NULL, "|");
            char *ultimo_valor_str = strtok(NULL, "|");
            char *token = strtok(sequencia_str, " ");
            listaP[i].tamanho_sequencia = 0;

            while (token != NULL) {
                listaP[i].sequencia[listaP[i].tamanho_sequencia++] = atoi(token);
                token = strtok(NULL, " ");
            }
            
            if (ultimo_valor_str != NULL) {
                listaP[i].chanceRequisitarES  = atoi(ultimo_valor_str);
            }

            i++;
            lineCount++;
        }
    }

    fclose(file);
    return lineCount;
}

// Função para printar a lista de processos
void show_process(DadosProcessos *listaP, int numeroProcessos) {
    for (int j = 0; j < numeroProcessos; j++) {
        printf("\nProcesso: %s\n", listaP[j].nome_processo);
        printf("ID: %d\n", listaP[j].id);
        printf("Tempo de Execucao: %d\n", listaP[j].tempo_execucao);
        printf("Prioridade: %d\n", listaP[j].prioridade);
        printf("Qtd Memoria: %d\n", listaP[j].qtdMemoria);
        printf("Sequencia Acessos: ");
        for (int k = 0; k < listaP[j].tamanho_sequencia; k++) {
            printf("%d ", listaP[j].sequencia[k]);
        }
        printf("\n");
        printf("Chance de ES: %d\n", listaP[j].chanceRequisitarES);
        printf("\n");
    }
}

// Função para printar a lista de dispositivos
void show_devices(DadosDispositivos *listaD, int numeroDispositivos) {
    for (int j = 0; j < numeroDispositivos; j++) {
        printf("ID: %d\n", listaD[j].idDispositivo);
        printf("Numero de usos simultaneos: %d\n", listaD[j].numUsosSimultaneos);
        printf("Tempo operacao: %d\n", listaD[j].tempoOperacao);
        printf("\n");
    }
}

//------------------------------------------- ESCALONADOR DE PROCESSO POR PRIORIDADE ---------------------------------------------------------------------------------------------
pthread_mutex_t mutex_prioridade;
//Função para criar um arquivo txt onde estarão armazenados o Id e a Latência de cada processo.
void criando_arquivo(){
    int i = 0;
    FILE *arquivo_3 = fopen("SaidaPrioridade.txt", "w");
    fprintf(arquivo_3, "ID | TEMPO EXECUTANDO | TEMPO BLOQUEADO | TEMPO PRONTO\n");

    while (i < numProcessos){
        if (listaP[i].id >= 0){
            fprintf(arquivo_3, "%d  | ", listaP[i].id);
            fprintf(arquivo_3, "        %d        | ", listaP[i].latencia);
            fprintf(arquivo_3, "        %d       | ", listaP[i].tempo_bloqueado);
            fprintf(arquivo_3, "    %d\n", listaP[i].tempo_pronto);
        }
        i++;
    }
    fclose(arquivo_3);
}

int usoDeDispositivo(int device, int idProcess){ // Posicao da listaD em que esta o dispositivo e id do processo a verificar
    // printf("Lista em uso do dispositivo %d\n",device);
    for(int i = 0; i < listaD[device].numUsosSimultaneos; i++){
        // printf("%d ",listaD[device].listaEmUso[i]);
        if(listaD[device].listaEmUso[i] == idProcess){
            return 1; // Processo ja esta usando o dispositivo
        }
    }
    // printf("\n");
    return 0;
}

void processos_bloqueados(int clockCPU){

    //printf("\n-- Thread Bloqueados -- \n");
    for (int process =0; process < numProcessos; process++){
        if(listaP[process].status == 1){
            for (int device =0; device < numDispositivosES; device++){  //percorre a lista de espera do dispositivo por prioridade de entrada
                for (int process = 0; process < listaD[device].tamanhoLista; process++){

                    if(listaD[device].listaEspera[process].tempoRestante > 0){ //Vini modificou o clock que era >=
                        // Checa se o dispositivo ainda aceita mais processos simultaneos
                        if(usoDeDispositivo(device,listaD[device].listaEspera[process].idProcesso)){   //Se o processo ja esta usando o dispositivo pode fazer E/S
                            listaD[device].listaEspera[process].tempoRestante -= clockCPU;
                            listaD[device].listaEspera[process].status = 1; // esta usando o dispositivo
                            // printf("Processo %d ja esta na lista E/S pelo dispositivo %d\n",listaD[device].listaEspera[process].idProcesso,device);
                            // printf("Processo %d realizou E/S\n",listaD[device].listaEspera[process].idProcesso);

                        }
                        else{ // Else para n guardar o mesmo processo em uso
                            if(listaD[device].numOfUses < listaD[device].numUsosSimultaneos){ 
                                printf("Processo %d NAO estava na lista E/S pelo dispositivo %d\n",listaD[device].listaEspera[process].idProcesso,device);
                                
                                listaD[device].listaEmUso[listaD[device].numOfUses] = listaD[device].listaEspera[process].idProcesso; // Guarda idProcess
                                listaD[device].numOfUses++; // criar lista com dispositivos em uso
                                printf("Numero de dispositivos em uso (%d) e lista em uso: ",listaD[device].numOfUses);
                                for(int m = 0; m < listaD[device].numUsosSimultaneos; m++){
                                    printf("%d",listaD[device].listaEmUso[m]);
                                }
                                printf("\n");

                                // printf("Processo %d esta usando o dispositivo %d\n",listaD[device].listaEspera[process].idProcesso,device);
                                listaD[device].listaEspera[process].tempoRestante -= clockCPU;
                                listaD[device].listaEspera[process].status = 1; // esta usando o dispositivo
                                printf("Processo %d realizou E/S\n",listaD[device].listaEspera[process].idProcesso);
                                
                            }
                        }
                    }
                    if(listaD[device].listaEspera[process].tempoRestante <= 0){ // Processo esta pronto
                        printf("Processo %d pronto\n",listaD[device].listaEspera[process].idProcesso);
                        listaD[device].numOfUses--; // Libera o uso do processo no dispositivo 

                        for(int m = 0; m < listaD[device].numUsosSimultaneos-1; m++){
                            listaD[device].listaEmUso[m] = listaD[device].listaEmUso[m+1];
                        }
                        printf("Lista atualizada de espera: ");
                        for(int m = 0; m < listaD[device].numUsosSimultaneos; m++){
                            printf("%d",listaD[device].listaEmUso[m]);
                        }
                        printf("\n");

                        
                        for (int posicao = 0; posicao <= numProcessos; posicao++){
                            if(listaP[posicao].id == listaD[device].listaEspera[process].idProcesso){
                                listaP[posicao].status = 0; // processo pronto
                            }
                        }
                        
                        printf("arroz\n");
                        remove_position(device, process); // process == posicao do processo na lista de espera
                        
                    }
                }
            }
        }
    }
    // sleep(rand() % 3);
}

//Função que recebe a lista de processos e executa-os. Durante o procedimento de executar um novo processo ele "trava" a função recebe_novos_processos usando um mutex.
void *executando_processos(void* arg){
    int latencia = 0, 
        id_processo_anterior = -1,
        id_processo_atual;

    while(true){
        int maior_prioridade = 0, 
            clockCPU = *(int*)arg,
            posicao = 0, 
            j = 0,
            k = 0;
        
        while(j < numProcessos){
            if(listaP[j].tempo_execucao <= 0 && listaP[j].prioridade > 0){
                // listaP[j].latencia += listaP[j].tempo_bloqueado;
                listaP[j].prioridade = 0;
            }
            // Percorre a lista de processos e encontra o processo com maior prioridade e armazena a posicao
            if (maior_prioridade < listaP[j].prioridade && listaP[j].tempo_execucao > 0 && listaP[j].status == 0){
                maior_prioridade = listaP[j].prioridade;
                posicao = j;
            }
            j++;     
        }

        id_processo_atual = listaP[posicao].id;
        processos_bloqueados(clockCPU);

        if(listaP[posicao].status == 0 && listaP[posicao].tempo_execucao > 0){
            // Ira realizar E/S
            int sort = sorteia_numero(listaP[posicao].chanceRequisitarES);
            if (sort){
                printf("Processo %d realizar E/S\n",listaP[posicao].id);
                int tempoES = (listaP[posicao].tempo_execucao < clockCPU) ? rand() % listaP[posicao].tempo_execucao : rand() % clockCPU; // tempo em que fara E/S

                gerencia_es(listaP[posicao].id); // Entrada e saída!!!!!!!!!!!!


                int k = 0;
                while(k < numProcessos){
                    if (listaP[k].prioridade > 0 && listaP[k].tempo_execucao > 0){
                        listaP[k].latencia += (listaP[k].tempo_execucao < tempoES ) ? listaP[k].tempo_execucao : tempoES;
                    }
                    if( k != posicao && listaP[k].status == 0 && listaP[k].prioridade > 0 && listaP[k].tempo_execucao > 0){
                        listaP[k].tempo_pronto += (listaP[k].tempo_execucao < tempoES ) ? listaP[k].tempo_execucao : tempoES;
                    }
                    if( k != posicao && listaP[k].status == 1 && listaP[k].prioridade > 0 && listaP[k].tempo_execucao > 0){
                        listaP[k].tempo_bloqueado += (listaP[k].tempo_execucao < tempoES ) ? listaP[k].tempo_execucao : tempoES;
                    }
                    k++;
                }
                listaP[posicao].tempo_execucao -= tempoES;
                //Imprime todo o processo que esta Executando
                imprimi_processos_executando(posicao);
                    
                //Imprime todos os processos que estao PRONTOS
                imprimi_processos_prontos(posicao);

                //Imprime todos os processos que estao BLOQUEADOS
                imprimi_processos_bloqueados();
                listaP[posicao].status = 1; //processo bloqueado

            }
            else{

                int k = 0;
                while(k < numProcessos){
                    if (listaP[k].prioridade > 0 && listaP[k].tempo_execucao > 0){
                        listaP[k].latencia += (listaP[k].tempo_execucao < clockCPU ) ? listaP[k].tempo_execucao : clockCPU;
                    }
                    if( k != posicao && listaP[k].status == 0 && listaP[k].prioridade > 0 && listaP[k].tempo_execucao > 0){
                        listaP[k].tempo_pronto += (listaP[k].tempo_execucao < clockCPU ) ? listaP[k].tempo_execucao : clockCPU;
                    }
                    if( k != posicao && listaP[k].status == 1 && listaP[k].prioridade > 0 && listaP[k].tempo_execucao > 0){
                        listaP[k].tempo_bloqueado += (listaP[k].tempo_execucao < clockCPU ) ? listaP[k].tempo_execucao : clockCPU;
                    }
                    k++;
                }
                listaP[posicao].tempo_execucao -= clockCPU;
                    //Imprime todo o processo que esta Executando
                imprimi_processos_executando(posicao);
                    
                //Imprime todos os processos que estao PRONTOS
                imprimi_processos_prontos(posicao);

                //Imprime todos os processos que estao BLOQUEADOS
                imprimi_processos_bloqueados();
            }

            printf("\n ----- DISPOSITIVOS -----\n"); 
            for (int i = 0; i < numDispositivosES; i++)
            {
                printf("Dispositivo: %d, Estado: %s\n", i, (listaD[i].tamanhoLista == 0) ? "Disponível para uso" : "Ocupado"); 
                for (int j = 0; j < listaD[i].tamanhoLista; j++)
                {
                    // printf("status do processo %d\n", listaD[i].listaEspera[j].status); 
                    printf("Id processo: %d, Estado: %s dispositivo %d\n", listaD[i].listaEspera[j].idProcesso,(listaD[i].listaEspera[j].status == 0) ? "(esperando)" : "(usando)", i);
                }
                
            }

            //FIFO(listaP, posicao); // Aplicacao do algoritmo de gerenciamento de memoria FIFO

            // if (listaP[posicao].prioridade == 0){
            //     // libera_memoria(listaP[posicao].id);
            //     printf("Trocas no processo %d : %d \n",listaP[posicao].id, listaP[posicao].trocasDePaginas);
            //     trocasDePaginas += listaP[posicao].trocasDePaginas;
            // }

        if ( listaP[numProcessos-1].id == -1 ){
            //printf("Thread executar encerrou \n");
            break;
        }
        sleep(1);
    }
    if(listaP[posicao].prioridade == 0){
        printf("--- CPU VAZIA ---\n");
        criando_arquivo();
        sleep(5);
    }
}
}

//Função que recebe novos processos e armazena numa lista. Durante o procedimento de adicionar um novo processo ele "trava" a função executando_processos usando um mutex.
void *recebe_novos_processos(void* arg){
    char resposta, linha[50];
    int teste = 0;
    
    numProcessos = *(int*)arg;
    printf("Caso deseja inserir um novo processo siga o padrão: \nnome do processo|Id|Tempo de execução|Prioridade|QtdMemoria|Seq de Acessos\n");

    while (true){
        fgets(linha, sizeof(linha), stdin);

        pthread_mutex_lock(&mutex_prioridade);  

        numProcessos++;

        listaP = realloc(listaP, numProcessos * sizeof(DadosProcessos));
        
        char nome[50];
        int id, tempo, prioridade, qtdMemoria;
        char* sequencia_str;
        
        // Tenta ler os 5 campos principais
        int result = sscanf(linha, "processo-%[^|]|%d|%d|%d|%d|%[^\n]", nome, &id, &tempo, &prioridade, &qtdMemoria, sequencia_str); 
        printf("Result: %d \n", result);
        
        if (result == 6) {  // Certifique-se de que 6 campos foram lidos corretamente
            strcpy(listaP[numProcessos - 1].nome_processo, nome);
            listaP[numProcessos - 1].id = id;
            listaP[numProcessos - 1].tempo_execucao = tempo;
            listaP[numProcessos - 1].prioridade = prioridade;
            listaP[numProcessos - 1].qtdMemoria = qtdMemoria;
            listaP[numProcessos - 1].latencia = 0;
            listaP[numProcessos - 1].tamanho_sequencia = 0;

            // Lê a sequência de acessos se existir
            if (sequencia_str != NULL) {
                char *token = strtok(sequencia_str, " ");
                while (token != NULL) {
                    listaP[numProcessos - 1].sequencia[listaP[numProcessos - 1].tamanho_sequencia++] = atoi(token);
                    token = strtok(NULL, " ");
                }
                free(sequencia_str);  // Libera a memória alocada pelo sscanf
            }

            // Exibe o processo adicionado
            printf("Novo processo adicionado: %s\n", listaP[numProcessos - 1].nome_processo);
            printf("Id: %d \n", listaP[numProcessos - 1].id);
            printf("Clock: %d \n", listaP[numProcessos - 1].tempo_execucao);
            printf("Prioridade: %d \n", listaP[numProcessos - 1].prioridade);
            printf("QtdMemoria: %d \n", listaP[numProcessos - 1].qtdMemoria);
            printf("Sequência de Acessos: ");
            for (int i = 0; i < listaP[numProcessos - 1].tamanho_sequencia; i++) {
                printf("%d ", listaP[numProcessos - 1].sequencia[i]);
            }
            printf("\n\n");
        }
        else {
            int result = sscanf(linha, "%s", nome);

            if(result == 1 && strcmp(nome, "s") == 0){
                listaP[numProcessos - 1].id = -1;
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
    srand(time(NULL));
    if (sem_init(&semaphoreClock, 0, 1) != 0) {
            perror("Semaphore initialization failed");
            return 1;
    }

    // Alocar memória para a lista de processos
    listaP = (DadosProcessos *)malloc(MAX_PROCESSES * sizeof(DadosProcessos));
    if (listaP == NULL) {
        perror("Erro ao alocar memória para a lista de processos");
        return EXIT_FAILURE;
    }
    // Alocar memória para a lista de dispositivos
    listaD = (DadosDispositivos *)malloc(MAX_PROCESSES * sizeof(DadosDispositivos));
    if (listaD == NULL) {
        perror("Erro ao alocar memória para a lista de dispositivos");
        return EXIT_FAILURE;
    }

    // Chamar a função para ler processos do arquivo
    int numeroProcessos = read_process("entrada_ES.txt", listaP, listaD);
    if (numeroProcessos == -1) {
        free(listaP);
        return EXIT_FAILURE;
    }

    char algoritmo_atual[] = "FIFO";
    show_devices(listaD, numDispositivosES); // Imprime os valores dos dispositivos
    show_process(listaP, numeroProcessos);   // Imprime os valores dos processos

    numProcessos = numeroProcessos; // Variavel para armazenar o valor de processos
    initMemory();

    pthread_t executando_processo, lendo_novo_processo;

    // pthread_create(&lendo_novo_processo, NULL, &recebe_novos_processos, &numProcessos);
    pthread_create(&executando_processo, NULL, &executando_processos, &clockCPU);
    
    pthread_join(executando_processo, NULL);
    
    // pthread_cancel(lendo_novo_processo);

    // Saida de troca de paginas
    // lerArquivoEAtualizar(algoritmo_atual,trocasDePaginas);

    free(listaP);
    free(listaD);

    return 0;
}