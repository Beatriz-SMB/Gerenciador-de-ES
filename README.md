
# Gerenciador de Memória

O objetivo deste trabalho é comparar o desempenho de diferentes algoritmos de substituição de página em termos de número de trocas de página, utilizando como referência o algoritmo ótimo. Serão implementados e analisados os seguintes algoritmos:


## 1. Algoritmo Ótimo
O algoritmo de substituição de páginas ótimo busca minimizar o número de faltas de página, substituindo a página que será acessada mais tarde no futuro em comparação com as outras páginas que já estão na memória. Ele faz isso com base no conhecimento dos acessos futuros, algo que, na prática, é impossível de se prever com precisão, o que torna esse algoritmo teórico.



## 2. Menos Recentemente Usada (MRU)

O algoritmo de substituição de páginas "Menos Recentemente Usada" é uma técnica de gerenciamento de memória que prioriza a permanência na memória das páginas que foram acessadas mais recentemente. Quando ocorre uma falta de página e é necessário substituir uma página na memória, o MRU identifica a página que não foi utilizada por mais tempo e a substitui.


## 3. Não usada frequentemente (NUF) 

Este 



## 4. FIFO

Este 


## Dependências

- Compilador GCC (ou qualquer outro compilador C compatível)


## Instalação

Para executar o projeto basta rodar os seguintes comandos via terminal:

```bash
  gcc geradorEntrada.c -o geradorEntrada
  ./geradorEntrada
```
```bash
  gcc algoritmo.c -o algoritmo
  ./algoritmo
```

## Autores
- [@Beatriz-SMB](https://github.com/Beatriz-SMB)
- [@Knoshz](https://github.com/Knoshz)
- [@RyanBLeon28](https://github.com/RyanBLeon28)
- [@Vinimborges](https://github.com/Vinimborges)
