/*
 * controller.c
 * Gerenciador do estacionamento
 *
 * Recebe pedidos de clientes via message queue MQ_REQUEST_NAME.
 * Mantém o estado das vagas em SHM_NAME (ParkState).
 * Quando ocupa uma vaga, cria uma thread detached que dorme PARK_TIME_SECONDS
 * e então libera a vaga (e sinaliza a condvar).
 *
 * Envia resposta para o cliente via fila de resposta cujo nome é informado
 * na mensagem do cliente (exemplo: "/park_resp_<pid>").
 *
 * Compile: make
 * Run: ./build/controller
 * 
 */

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>

#include "common.h"

static ParkState *state = NULL;
static int shm_fd = -1;
static mqd_t mq_req = (mqd_t)-1;
static volatile sig_atomic_t running = 1;

/* cleanup function */
static void cleanup(void) {
    if (mq_req != (mqd_t)-1) {
        mq_close(mq_req);
        mq_unlink(MQ_REQUEST_NAME);
    }
    if (state) {
        // destroy mutex/cond (best effort)
        pthread_mutex_destroy(&state->mutex);
        pthread_cond_destroy(&state->cond);
        munmap(state, sizeof(ParkState));
        state = NULL;
    }
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
        shm_fd = -1;
    }
}

/* signal handler for graceful stop */
static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

/* detached thread: frees a spot after PARK_TIME_SECONDS */
void *release_thread(void *arg) {
    int idx = *(int*)arg;
    free(arg);

    sleep(PARK_TIME_SECONDS);

    pthread_mutex_lock(&state->mutex);
    if (state->spots[idx] == 1) {
        state->spots[idx] = 0;                     // libera vaga
        // notify display (cond)
        pthread_cond_broadcast(&state->cond);
        printf("[CONTROLLER] Vaga %d liberada (tempo esgotou)\n", idx+1);
    }
    pthread_mutex_unlock(&state->mutex);

    return NULL;
}

/* Try to assign a free spot; returns index (0..MAX_SPOTS-1) or -1 if none */
int assign_spot_and_launch_release(void) {
    int found = -1;
    pthread_mutex_lock(&state->mutex);
    for (int i = 0; i < MAX_SPOTS; ++i) {
        if (state->spots[i] == 0) {
            state->spots[i] = 1;
            state->total_revenue += PRICE_PER_SPOT;
            found = i;
            // signal display/others
            pthread_cond_broadcast(&state->cond);
            break;
        }
    }
    pthread_mutex_unlock(&state->mutex);

    if (found >= 0) {
        // spawn detached thread to release after PARK_TIME_SECONDS
        pthread_t th;
        int *arg = malloc(sizeof(int));
        if (!arg) {
            perror("malloc");
            return found; // still assigned, but no release thread -> leak (unlikely)
        }
        *arg = found;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&th, &attr, release_thread, arg) != 0) {
            perror("pthread_create");
            free(arg);
        }
        pthread_attr_destroy(&attr);
    }
    return found;
}

/* process one request message (expecting "REQ:<resp_mq_name>") */
void process_request(char *msg) {
    // parse
    // format expected: "REQ:<respname>"
    if (strncmp(msg, "REQ:", 4) != 0) {
        fprintf(stderr, "[CONTROLLER] mensagem invalida: %s\n", msg);
        return;
    }
    char resp_name[RESP_NAME_LEN];
    strncpy(resp_name, msg + 4, RESP_NAME_LEN-1);
    resp_name[RESP_NAME_LEN-1] = '\0';

    // assign spot
    int spot = assign_spot_and_launch_release();

    // send response to client
    mqd_t mq_resp = mq_open(resp_name, O_WRONLY);
    if (mq_resp == (mqd_t)-1) {
        fprintf(stderr, "[CONTROLLER] nao conseguiu abrir fila de resposta %s: %s\n",
                resp_name, strerror(errno));
        return;
    }

    char out[MQ_MSG_SIZE];
    if (spot >= 0) {
        snprintf(out, sizeof(out), "OK:%d:%d", spot, state->total_revenue); // spot idx and total
        printf("[CONTROLLER] Cliente recebeu vaga %d. Total = R$%d\n", spot+1, state->total_revenue);
    } else {
        snprintf(out, sizeof(out), "FULL:%d", state->total_revenue);
        printf("[CONTROLLER] Cliente informado: ESTACIONAMENTO CHEIO. Total = R$%d\n", state->total_revenue);
    }

    mq_send(mq_resp, out, strlen(out)+1, 0);
    mq_close(mq_resp);
}

int main(void) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Clean possible leftovers
    mq_unlink(MQ_REQUEST_NAME);
    shm_unlink(SHM_NAME);

    // Create and map shared memory
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    if (ftruncate(shm_fd, sizeof(ParkState)) == -1) {
        perror("ftruncate");
        shm_unlink(SHM_NAME);
        exit(1);
    }
    state = mmap(NULL, sizeof(ParkState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (state == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_NAME);
        exit(1);
    }

    // Initialize structure in shared memory
    // mutex and cond need pshared attributes
    pthread_mutexattr_t mattr;
    pthread_condattr_t cattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    // initialize fields
    memset(state, 0, sizeof(ParkState));
    if (pthread_mutex_init(&state->mutex, &mattr) != 0) {
        perror("pthread_mutex_init");
        cleanup();
        exit(1);
    }
    if (pthread_cond_init(&state->cond, &cattr) != 0) {
        perror("pthread_cond_init");
        cleanup();
        exit(1);
    }
    state->total_revenue = 0;
    for (int i=0;i<MAX_SPOTS;i++) state->spots[i]=0;

    pthread_mutexattr_destroy(&mattr);
    pthread_condattr_destroy(&cattr);

    // Create request message queue
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MQ_MSG_SIZE;
    attr.mq_curmsgs = 0;

    mq_req = mq_open(MQ_REQUEST_NAME, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq_req == (mqd_t)-1) {
        perror("mq_open req");
        cleanup();
        exit(1);
    }

    printf("Aguardando pedidos (Ctrl+C para encerrar)...\n");

    // main loop
    char buf[MQ_MSG_SIZE];
    while (running) {
        ssize_t n = mq_receive(mq_req, buf, sizeof(buf), NULL);
        if (n >= 0) {
            buf[n] = '\0';
            process_request(buf);
        } else {
            if (errno == EINTR) continue;
            perror("mq_receive");
            break;
        }
    }

    printf("[CONTROLLER] Encerrando...\n");
    cleanup();
    return 0;
}
