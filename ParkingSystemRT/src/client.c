/*
 * client.c
 * Simula um carro chegando: cria uma fila de resposta única, envia
 * "REQ:<resp_mq_name>" para MQ_REQUEST_NAME e espera resposta.
 *
 * Uso: ./build/client
 */

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mqueue.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "common.h"

int main(void) {
    pid_t pid = getpid();
    char resp_mq_name[RESP_NAME_LEN];
    snprintf(resp_mq_name, sizeof(resp_mq_name), "/park_resp_%d", (int)pid);

    // create response queue (client reads from it)
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 4;
    attr.mq_msgsize = MQ_MSG_SIZE;
    attr.mq_curmsgs = 0;

    mqd_t mq_resp = mq_open(resp_mq_name, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq_resp == (mqd_t)-1) {
        perror("mq_open resp");
        exit(1);
    }

    // open request queue and send request with our resp name
    mqd_t mq_req = mq_open(MQ_REQUEST_NAME, O_WRONLY);
    if (mq_req == (mqd_t)-1) {
        perror("mq_open req");
        mq_unlink(resp_mq_name);
        exit(1);
    }

    char msg[MQ_MSG_SIZE];
    snprintf(msg, sizeof(msg), "REQ:%s", resp_mq_name);
    if (mq_send(mq_req, msg, strlen(msg)+1, 0) == -1) {
        perror("mq_send");
        mq_close(mq_req);
        mq_unlink(resp_mq_name);
        exit(1);
    }
    mq_close(mq_req);

    // wait response (blocking)
    char buf[MQ_MSG_SIZE];
    ssize_t n = mq_receive(mq_resp, buf, sizeof(buf), NULL);
    if (n >= 0) {
        buf[n] = '\0';
        // expected formats:
        // "OK:<spotidx>:<total>" or "FULL:<total>"
        if (strncmp(buf, "OK:", 3) == 0) {
            int spot, total;
            if (sscanf(buf+3, "%d:%d", &spot, &total) >= 1) {
                printf("[CLIENT %d] Estacionado na vaga %d. Total arrecadado agora: R$%d\n",
                       (int)pid, spot+1, total);
            } else {
                printf("[CLIENT %d] Resposta inesperada: %s\n", (int)pid, buf);
            }
        } else if (strncmp(buf, "FULL:", 5) == 0) {
            int total;
            if (sscanf(buf+5, "%d", &total) >= 1) {
                printf("[CLIENT %d] Estacionamento cheio. Total arrecadado: R$%d\n",
                       (int)pid, total);
            } else {
                printf("[CLIENT %d] Resposta inesperada: %s\n", (int)pid, buf);
            }
        } else {
            printf("[CLIENT %d] Mensagem: %s\n", (int)pid, buf);
        }
    } else {
        perror("mq_receive resp");
    }

    mq_close(mq_resp);
    mq_unlink(resp_mq_name);
    return 0;
}
