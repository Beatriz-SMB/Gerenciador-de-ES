#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define TAM_LINHA 500
#define PROCESSOS 100
#define TAM_SEQUENCIA 100
#define MAX_LINE_LENGTH 256

// Dados dos processos
typedef struct{
    char nome_processo[50];
    int id,
        tempo_execucao,
        prioridade,
        qtd_memoria,
        sequencia[TAM_SEQUENCIA],
        tamanho_sequencia,
        qtd_paginas_memoria,
        contando,
        latencia;
} DadosProcessos;

//Dados das páginas
typedef struct{
    int numero_pagina,  
        id_processo,   
        ultimo_acesso;
} Paginas;


pthread_mutex_t mutex_PR;

char alg_escalonamento[50];
char politica_memoria[50];
int clock_cpu, tam_memoria, tam_pagina, percentual_alocacao, acesso_por_ciclo;
int iterador = 0, tempo_atual = 0, molduras_usadas = 0, trocas_de_paginas = 0;
int qtd_processos;


int * molduras_memoria; //Ponteiro para a quantidade de molduras que a memória possui.
DadosProcessos *lista_processos; //Ponteiro para a lista de processos.
Paginas *paginas_na_memoria; //Ponteiro para uma lista, que armazena as páginas e seus dados.

//------------------------------------------- LEITURA DO ARQUIVO DE ENTRADA E MANIPULACAO --------------------------------------------------------------------------------------------

//Função para ler os processos do arquivo.
int recebe_processos(const char *nome_arquivo, DadosProcessos *lista_processos) {
    FILE *file = fopen(nome_arquivo, "r");

    int numero_processos = 0, controlador = 0, i = 0;
    char line[TAM_LINHA];
  
    while (fgets(line, sizeof(line), file) != NULL) {

        if (controlador == 0) {
            // Ler as informações de configuração
            strcpy(alg_escalonamento, strtok(line, "|"));
            clock_cpu = atoi(strtok(NULL, "|"));
            strcpy(politica_memoria, strtok(NULL, "|"));
            tam_memoria = atoi(strtok(NULL, "|"));
            tam_pagina = atoi(strtok(NULL, "|"));
            percentual_alocacao = atoi(strtok(NULL, "|"));
            acesso_por_ciclo = atoi(strtok(NULL, "|"));
            controlador++;

            printf("Memoria Principal com capacidade de %d bytes\n",tam_memoria);

        } else {
            // Processar informações de cada processo
            strcpy(lista_processos[i].nome_processo, strtok(line, "|"));
            lista_processos[i].id = atoi(strtok(NULL, "|"));
            lista_processos[i].tempo_execucao = atoi(strtok(NULL, "|"));
            lista_processos[i].prioridade = atoi(strtok(NULL, "|"));
            lista_processos[i].qtd_memoria = atoi(strtok(NULL, "|"));
            lista_processos[i].latencia = 0;
            lista_processos[i].contando = 0;
            lista_processos[i].qtd_paginas_memoria = 0;
            

            // Ler e armazenar a sequência
            char *sequencia_str = strtok(NULL, "|");
            char *token = strtok(sequencia_str, " ");
            lista_processos[i].tamanho_sequencia = 0;

            while (token != NULL) {
                lista_processos[i].sequencia[lista_processos[i].tamanho_sequencia++] = atoi(token);
                token = strtok(NULL, " ");
            }

            i++;
            numero_processos++;
        }
    }

    fclose(file);
    return numero_processos;
}
//Função para imprimir os processos.
void imprimir_processos(DadosProcessos *lista_processos, int numeroProcessos) {

    for (int j = 0; j < numeroProcessos; j++) {
        printf("\nProcesso: %s\n", lista_processos[j].nome_processo);
        printf("ID: %d\n", lista_processos[j].id);
        printf("Tempo de Execução: %d\n", lista_processos[j].tempo_execucao);
        printf("Prioridade: %d\n", lista_processos[j].prioridade);
        printf("Qtd Memória: %d\n", lista_processos[j].qtd_memoria);
        printf("Sequência Acessos: ");
        for (int k = 0; k < lista_processos[j].tamanho_sequencia; k++) {
            printf("%d ", lista_processos[j].sequencia[k]);
        }
        printf("\n");
    }
}
//Função para criar um arquivo txt onde estarão armazenados o Id e a Latência de cada processo.
void criando_arquivo(){
    int i = 0;
    FILE* arquivo_3 = fopen("SaidaPrioridade.txt", "w");
    fprintf(arquivo_3, "ID | LATÊNCIA\n");

    while (i < iterador){
        if (lista_processos[i].id >= 0){
            fprintf(arquivo_3, "%d | ", lista_processos[i].id);
            fprintf(arquivo_3, "%d\n", lista_processos[i].latencia);
        }
        i++;
    }
}

//------------------------------------------- LEITURA DO ARQUIVO DE ENTRADA E MANIPULACAO --------------------------------------------------------------------------------------------

//------------------------------------------- GERENCIADOR DE MEMÓRIA E CÓDIGOS ASSOCIADOS -------------------------------------------------------------------------------------------

//Função para inicializar a página.
void inicializar_pagina(Paginas *pagina) {
    pagina -> numero_pagina = -1; 
    pagina -> id_processo = -1;    
    pagina -> ultimo_acesso = 0;   
}
//Função para liberar a memória ocupada pelo um processo que já foi encerrado.
void libera_memoria(int posicao){
    for (int i = 0; i < *molduras_memoria; i++){
        if(lista_processos[posicao].id == paginas_na_memoria[i].id_processo){
            paginas_na_memoria[i].id_processo = -1;
            paginas_na_memoria[i].numero_pagina = -1;
            paginas_na_memoria[i].ultimo_acesso = 0;
            molduras_usadas--;

        }
    }
    molduras_usadas = 0;
}
//Função para imprimir as páginas que estão na memória.
void imprimir_paginas(){
    for (int i = 0; i < *molduras_memoria; i++){
        printf("\nid do processo: %d\n", paginas_na_memoria[i].id_processo);
        printf("numero da pagina: %d\n", paginas_na_memoria[i].numero_pagina);
        printf("ultimo acesso: %d\n", paginas_na_memoria[i].ultimo_acesso);

    }

}
//Função que implementa o gerenciador de memória pelo algoritmo do Menos Recentemente Usado (MRU)
void MRU (int posicao, int clock_cpu){
    int num_paginas_recebidas = clock_cpu * acesso_por_ciclo;
    int qtd_paginas_permitida = (lista_processos[posicao].qtd_memoria * 0.75) / tam_pagina;
    printf("\n%d\n", tempo_atual);
      
    if(strcmp(politica_memoria, "local") == 0){ //Aplica o algorimto para o caso local.
        if(molduras_usadas <= *molduras_memoria){ //Verifica se tem espaço na memória.
        
            for (int i = 0; i < num_paginas_recebidas; i++){ //Acha, na memória, as posições onde as páginas do processo vão ser alocadas.
                int flag_subs = 0, acesso = 10000, menos_acessada = -1;
                for (int j = 0; j < *molduras_memoria; j++){
                    
                    if(paginas_na_memoria[j].id_processo == -1 && lista_processos[posicao].qtd_paginas_memoria < qtd_paginas_permitida){ //Verifica se a posição está vazia e se o processo pode adicionar mais páginas na memória 

                        paginas_na_memoria[j].id_processo = lista_processos[posicao].id;
                        paginas_na_memoria[j].numero_pagina = lista_processos[posicao].sequencia[i + lista_processos[posicao].contando] ;
                        paginas_na_memoria[j].ultimo_acesso = tempo_atual + (i / 2);
                        lista_processos[posicao].qtd_paginas_memoria++;
                        flag_subs = 0;
                        molduras_usadas++;
                        break;
                       
                    }else if (lista_processos[posicao].id == paginas_na_memoria[j].id_processo && lista_processos[posicao].sequencia[i + lista_processos[posicao].contando] == paginas_na_memoria[j].numero_pagina){ //Atualiza a página, caso ela já esteja na memória.
                        paginas_na_memoria[j].ultimo_acesso = tempo_atual + (i / 2);
                        flag_subs = 0;
                        break;
                        
                    }else if (paginas_na_memoria[j].ultimo_acesso <= acesso && paginas_na_memoria[j].id_processo == lista_processos[posicao].id){ //Caso nem uma das outras opções ocorra, o programa procura pela a página menos recentemente usada para ser substituida.
                        acesso = paginas_na_memoria[j].ultimo_acesso;
                        menos_acessada = j;
                        flag_subs = 1;  
                    }

                }
                
                if (flag_subs == 1){ //Troca a página que ficou mais tempo sem ser acessada.
                    paginas_na_memoria[menos_acessada].id_processo = lista_processos[posicao].id;
                    paginas_na_memoria[menos_acessada].numero_pagina = lista_processos[posicao].sequencia[i + lista_processos[posicao].contando] ;
                    paginas_na_memoria[menos_acessada].ultimo_acesso = tempo_atual + (i / 2);
                    trocas_de_paginas++;
                    
                }
          
            }

            lista_processos[posicao].contando += num_paginas_recebidas;  
            imprimir_paginas();
        }

    } else{ //Aplica o algoritmo para o caso global.

        
        for (int i = 0; i < num_paginas_recebidas; i++){ //Acha, na memória, as posições onde as páginas do processo vão ser alocadas.
            int flag_subs = 0, acesso = 10000, menos_acessada = -1;
            for (int j = 0; j < *molduras_memoria; j++){
                
                if(paginas_na_memoria[j].id_processo == -1 && lista_processos[posicao].qtd_paginas_memoria < qtd_paginas_permitida){ //Verifica se a posição está vazia e se o processo pode adicionar mais páginas na memória 

                    paginas_na_memoria[j].id_processo = lista_processos[posicao].id;
                    paginas_na_memoria[j].numero_pagina = lista_processos[posicao].sequencia[i + lista_processos[posicao].contando] ;
                    paginas_na_memoria[j].ultimo_acesso = tempo_atual + (i / 2);
                    lista_processos[posicao].qtd_paginas_memoria++;
                    flag_subs = 0;
                    molduras_usadas++;
                    break;
                    
                }else if (lista_processos[posicao].id == paginas_na_memoria[j].id_processo && lista_processos[posicao].sequencia[i + lista_processos[posicao].contando] == paginas_na_memoria[j].numero_pagina){ //Atualiza a página, caso ela já esteja na memória.
                    paginas_na_memoria[j].ultimo_acesso = tempo_atual + (i / 2);
                    flag_subs = 0;
                    break;
                    
                }else if (paginas_na_memoria[j].ultimo_acesso <= acesso && (lista_processos[posicao].qtd_paginas_memoria < qtd_paginas_permitida || paginas_na_memoria[j].id_processo == lista_processos[posicao].id)){ //Caso nem uma das outras opções ocorra, o programa procura pela a página menos recentemente usada para ser substituida.
                    acesso = paginas_na_memoria[j].ultimo_acesso;
                    menos_acessada = j;
                    flag_subs = 1;  
                }

            }

            if (flag_subs == 1){ //Troca a página que ficou mais tempo sem ser acessada.
                int id_processo_trocado = paginas_na_memoria[menos_acessada].id_processo;

                for(int k = 0; k < qtd_processos; k++){

                    if(lista_processos[k].id == id_processo_trocado){
                        lista_processos[k].qtd_paginas_memoria--;
                    }


                }
                lista_processos[posicao].qtd_paginas_memoria++;
                paginas_na_memoria[menos_acessada].id_processo = lista_processos[posicao].id;
                paginas_na_memoria[menos_acessada].numero_pagina = lista_processos[posicao].sequencia[i + lista_processos[posicao].contando] ;
                paginas_na_memoria[menos_acessada].ultimo_acesso = tempo_atual + (i / 2);
                trocas_de_paginas++;
                
            }
          
        }

        lista_processos[posicao].contando += num_paginas_recebidas;  
        imprimir_paginas();
    }

}

//------------------------------------------- GERENCIADOR DE MEMÓRIA E CÓDIGOS ASSOCIADOS -------------------------------------------------------------------------------------------

//Função que recebe a lista de processos e executa-os. Durante o procedimento de executar um novo processo ele "trava" a função recebe_novos_processos usando um mutex.
void *executando_processos(void* arg){
    int latencia = 0;
    int clock_cpu = *(int*)arg;

    while(true){
        int maior_prioridade = 0, j = 0, k = 0, posicao = 0;
        while(j < iterador){
            if (maior_prioridade < lista_processos[j].prioridade && lista_processos[j].tempo_execucao > 0){
                maior_prioridade = lista_processos[j].prioridade;
                posicao = j;
            }
            j++;     
        }

       

        if (maior_prioridade > 0){
            pthread_mutex_lock(&mutex_PR);

            if (lista_processos[posicao].tempo_execucao - clock_cpu >= 0){
                lista_processos[posicao].tempo_execucao = lista_processos[posicao].tempo_execucao - clock_cpu;
            }
            else{
                clock_cpu = lista_processos[posicao].tempo_execucao;
                lista_processos[posicao].tempo_execucao = 0;
            }

            while(k < iterador){
                if (lista_processos[k].prioridade > 0){
                lista_processos[k].latencia += clock_cpu;
                }
                k++;
            }
         
            if(lista_processos[posicao].tempo_execucao <= 0){ //Processo encerrou
                lista_processos[posicao].prioridade = 0;          
            }

            MRU(posicao, clock_cpu);
            tempo_atual += clock_cpu;

            if (lista_processos[posicao].prioridade == 0){
                libera_memoria(posicao);
                tempo_atual = 0;
            }

            pthread_mutex_unlock(&mutex_PR); 

            printf("\n");
            sleep(1);
            printf("\nNúmero de troca de páginas: %d\n", trocas_de_paginas);
        }
        else{
            if ( lista_processos[iterador-1].id == -1 ){
                //printf("Thread executar encerrou \n");
                break;
            }
            printf("Todos os processos foram executados. Deseja encerrar?\n");
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

        pthread_mutex_lock(&mutex_PR);  

        iterador++;

        lista_processos = realloc(lista_processos, iterador * sizeof(DadosProcessos));
        
        char nome[50];
        int id, tempo, prioridade, qtd_memoria;
        char* sequencia_str;
        
        // Tenta ler os 5 campos principais
        int result = sscanf(linha, "%[^|]|%d|%d|%d|%d|%m[^\n]", nome, &id, &tempo, &prioridade, &qtd_memoria, &sequencia_str); 
        
        if (result == 6) {  // Certifique-se de que 6 campos foram lidos corretamente
            strcpy(lista_processos[iterador - 1].nome_processo, nome);
            lista_processos[iterador - 1].id = id;
            lista_processos[iterador - 1].tempo_execucao = tempo;
            lista_processos[iterador - 1].prioridade = prioridade;
            lista_processos[iterador - 1].qtd_memoria = qtd_memoria;
            lista_processos[iterador - 1].latencia = 0;
            lista_processos[iterador - 1].tamanho_sequencia = 0;

            // Lê a sequência de acessos se existir
            if (sequencia_str != NULL) {
                char *token = strtok(sequencia_str, " ");
                while (token != NULL) {
                    lista_processos[iterador - 1].sequencia[lista_processos[iterador - 1].tamanho_sequencia++] = atoi(token);
                    token = strtok(NULL, " ");
                }
                free(sequencia_str);  // Libera a memória alocada pelo sscanf
            }

            // Exibe o processo adicionado
            printf("Novo processo adicionado: %s\n", lista_processos[iterador - 1].nome_processo);
            printf("Id: %d \n", lista_processos[iterador - 1].id);
            printf("Clock: %d \n", lista_processos[iterador - 1].tempo_execucao);
            printf("Prioridade: %d \n", lista_processos[iterador - 1].prioridade);
            printf("QtdMemoria: %d \n", lista_processos[iterador - 1].qtd_memoria);
            printf("Sequência de Acessos: ");
            for (int i = 0; i < lista_processos[iterador - 1].tamanho_sequencia; i++) {
                printf("%d ", lista_processos[iterador - 1].sequencia[i]);
            }
            printf("\n\n");
        }
        else {
            int result = sscanf(linha, "%s", nome);

            if(result == 1 && strcmp(nome, "s") == 0){
                lista_processos[iterador - 1].id = -1;
                //printf("Thread Adiconar encerrou \n");
                break;
            }
        }
        pthread_mutex_unlock(&mutex_PR);
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

//Função Main que inicializa o programa.
int main() {
    // Alocar memória para a lista de processos
    lista_processos = (DadosProcessos *)malloc(PROCESSOS * sizeof(DadosProcessos));
    molduras_memoria = (int *)malloc(sizeof(int));

    // Chamar a função para ler processos do arquivo
    int numero_processos = recebe_processos("entradaMemoria.txt", lista_processos);
    qtd_processos = numero_processos;

    *molduras_memoria = tam_memoria / tam_pagina;
    paginas_na_memoria =  (Paginas *)malloc(*molduras_memoria * sizeof(Paginas));
    


    // Imprime os valores dos processos
    imprimir_processos(lista_processos, numero_processos);

    char algoritmo_atual[] = "MRU";

    printf("\nAlgoritmo MRU\n");
    iterador = numero_processos;

    for (int i = 0; i < *molduras_memoria; i++) {
        inicializar_pagina(&paginas_na_memoria[i]);
    }   

 

    pthread_t executando_processo, lendo_novo_processo;
    pthread_mutex_init(&mutex_PR, NULL);

    pthread_create(&lendo_novo_processo, NULL, &recebe_novos_processos, &iterador);
    pthread_create(&executando_processo, NULL, &executando_processos, &clock_cpu);

    pthread_join(executando_processo, NULL);
    pthread_cancel(lendo_novo_processo);

     // Saida de troca de paginas
    lerArquivoEAtualizar(algoritmo_atual,trocas_de_paginas);

    pthread_mutex_destroy(&mutex_PR);

    free(lista_processos);
    free(paginas_na_memoria);
    free(molduras_memoria);

   

    return 0;
}