#ifndef PARKRT_COMMON_H
#define PARKRT_COMMON_H

#include <pthread.h>
#include <mqueue.h>
#include <stdbool.h>

#define MAX_SPOTS 10
#define MAX_QUEUE 20       // tamanho máximo da fila FIFO

#define PARK_TIME_SECONDS 15
#define PRICE_PER_SPOT    10

// simbologia (display usa estes valores)
#define SPOT_FREE     0   // LIVRE
#define SPOT_OCCUPIED 1   // OCUPADO

// nomes de IPC
#define MQ_REQUEST_NAME "/park_mq"
#define SHM_NAME        "/park_shm"

// MQ
#define MQ_MSG_SIZE    128
#define RESP_NAME_LEN   64

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;

    int spots[MAX_SPOTS];       // 0 = livre, 1 = ocupado
    int total_revenue;

    // FILA DE ESPERA (FIFO)
    int waiting_queue[MAX_QUEUE];
    int queue_size;

} ParkState;

#endif
