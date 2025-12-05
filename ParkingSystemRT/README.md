ParkRT — Estacionamento Concorrente (A3)
Objetivo

Projeto demonstrativo para a disciplina Sistemas Eletrônicos de Tempo-Real, implementando um estacionamento controlado por mecanismos de concorrência e IPC:

Filas de mensagens POSIX (mq_send/mq_receive) para pedidos dos carros

Memória compartilhada POSIX (shm_open + mmap) para vagas e valor arrecadado

Mutex + Condvar (pshared) para sincronização entre processos

Threads POSIX para liberação automática das vagas

Modelo Produtor–Consumidor entre clientes e controller

Interface ncurses atualizada em tempo real (display)

Custo por vaga: R$10

Tempo de permanência: 15s por carro

O projeto simula vários carros chegando, pedindo vaga, estacionando, liberando vaga e atualizando o painel.

Arquivos

src/controller.c — núcleo do sistema; recebe pedidos, atribui vagas, libera vagas

src/client.c — representa um carro que solicita uma vaga

src/display.c — painel ncurses que mostra vagas e total arrecadado

src/common.h — definições de IPC e estruturas compartilhadas

src/sim_clients.sh — script para gerar múltiplos clientes automaticamente

Makefile

Requisitos

Linux / WSL (Ubuntu)

gcc, librt, pthreads, ncurses

compilar com: -lrt -pthread -lncurses

Compilar
make

Executar
1. Iniciar o controller
./controller

2. Executar o display (painel ncurses)
./display

3. Criar carros (clientes)
./client


ou usar o script automático:

./sim_clients.sh