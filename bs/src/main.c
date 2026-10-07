// main.c
// Запуск: ./bin/battleship [-c файл.cfg] [--ключ=значение ...]
// Ключи: size, touch, again, max_moves, delay, seed, a, b, log
// Коды завершения: 0 ок, 1 плохие параметры, 2 не открылся файл настроек,
// 3 не расставился флот, 4 судья нашёл нарушение, 128+N прервали сигналом N
#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void OnSignal(int sig) { g_stop = sig; }

static int Set(Config* c, const char* k, const char* v) {
    if (!strcmp(k, "size")) c->size = atoi(v);
    else if (!strcmp(k, "touch")) c->touch = atoi(v);
    else if (!strcmp(k, "again")) c->again = atoi(v);
    else if (!strcmp(k, "max_moves")) c->max_moves = atoi(v);
    else if (!strcmp(k, "delay")) c->delay = atoi(v);
    else if (!strcmp(k, "seed")) c->seed = (unsigned)strtoul(v, NULL, 10);
    else if (!strcmp(k, "a") || !strcmp(k, "b")) snprintf(c->strat[k[0] - 'a'], 8, "%s", v);
    else if (!strcmp(k, "log")) snprintf(c->log, sizeof(c->log), "%s", v);
    else return -1;
    return 0;
}

// Файл настроек: строки вида ключ=значение, # начинает комментарий
static int LoadFile(Config* c, const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char buf[2048];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n < 0) return -1;
    buf[n] = 0;
    char* save;
    for (char* line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char* hash = strchr(line, '#');
        if (hash) *hash = 0;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        if (Set(c, line, eq + 1) != 0) return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    Config c = {10, 0, 1, 0, 100, (unsigned)getpid(), {"hunt", "random"}, "battleship.log"};
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            if (LoadFile(&c, argv[++i]) != 0) {
                dprintf(2, "не получилось прочитать файл настроек %s\n", argv[i]);
                return 2;
            }
        } else if (!strncmp(argv[i], "--", 2) && strchr(argv[i], '=')) {
            char* arg = strdup(argv[i] + 2);
            char* eq = strchr(arg, '=');
            *eq = 0;
            int bad = Set(&c, arg, eq + 1);
            free(arg);
            if (bad) {
                dprintf(2, "неизвестный параметр: %s\n", argv[i]);
                return 1;
            }
        } else {
            dprintf(2, "Запуск: %s [-c файл] [--size=10 --touch=0 --again=1 --seed=N --a=hunt --b=random ...]\n", argv[0]);
            return 1;
        }
    }
    if (c.size < 8 || c.size > MAXN || (c.touch | 1) != 1 || (c.again | 1) != 1 || c.delay < 0 || c.max_moves < 0 ||
        (strcmp(c.strat[0], "random") && strcmp(c.strat[0], "hunt")) ||
        (strcmp(c.strat[1], "random") && strcmp(c.strat[1], "hunt"))) {
        dprintf(2, "неправильные значения параметров (size 8..%d, touch/again 0 или 1, a/b random или hunt)\n", MAXN);
        return 1;
    }

    // без SA_RESTART, чтобы sleep прерывался сразу
    struct sigaction sa = {0};
    sa.sa_handler = OnSignal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    return Play(&c);
}
