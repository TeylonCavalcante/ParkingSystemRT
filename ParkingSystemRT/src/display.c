/*
 * display.c
 *
 * Run: ./build/display
 */

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "common.h"

static ParkState *state = NULL;
static int shm_fd = -1;

static void draw_screen(void) {
    clear();
    mvprintw(0, 2, "=== ParkingSystemRT - Painel ===");

    int free_count = 0;

    for (int i = 0; i < MAX_SPOTS; ++i) {
        int y = 2 + i;
        if (state->spots[i] == 0) {
            mvprintw(y, 4, "[%02d] . LIVRE", i+1);
            free_count++;
        } else {
            mvprintw(y, 4, "[%02d] X OCUPADA", i+1);
        }
    }

    mvprintw(2 + MAX_SPOTS + 1, 4, "Vagas livres: %d / %d", free_count, MAX_SPOTS);
    mvprintw(2 + MAX_SPOTS + 2, 4, "Total arrecadado: R$ %d", state->total_revenue);

    mvprintw(2 + MAX_SPOTS + 4, 2, "Pressione 'q' para sair.");
    refresh();
}


int main(void) {
    // open shared memory
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open display");
        fprintf(stderr, "Certifique-se de que o controller esteja rodando.\n");
        return 1;
    }
    state = mmap(NULL, sizeof(ParkState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (state == MAP_FAILED) {
        perror("mmap display");
        return 1;
    }

    // ncurses init
    initscr();
    noecho();
    cbreak();
    nodelay(stdscr, TRUE); // make getch non-blocking
    keypad(stdscr, TRUE);

    // initial draw
    draw_screen();

    // wait loop: use cond to be signalled by controller when state changes
    int ch = 0;
    while (1) {
        // check keyboard
        ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        // Wait on cond (with mutex) but to avoid blocking keyboard handling, use timed wait.
        // We'll do a timed wait of 1 second.
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        pthread_mutex_lock(&state->mutex);
        int rc = pthread_cond_timedwait(&state->cond, &state->mutex, &ts);
        // either signalled (rc==0) or timed out (rc==ETIMEDOUT)
        // redraw in both cases to reflect current state
        draw_screen();
        pthread_mutex_unlock(&state->mutex);
    }

    // end ncurses
    endwin();
    munmap(state, sizeof(ParkState));
    close(shm_fd);
    return 0;
}
