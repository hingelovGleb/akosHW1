// judge.c
// Судья: расставляет флоты, спрашивает игроков, куда стрелять, применяет
// выстрел и сообщает результат. Всё печатаем через dprintf на экран (fd 1)
// и в журнал.
#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int s_log = -1;
static int s_quiet = 0;  // 1 - в серии боёв отдельные ходы не печатаем

static void Say(const char* fmt, ...) {
    if (s_quiet) return;
    va_list ap;
    va_start(ap, fmt);
    vdprintf(1, fmt, ap);
    va_end(ap);
    if (s_log >= 0) {
        va_start(ap, fmt);
        vdprintf(s_log, fmt, ap);
        va_end(ap);
    }
}

static void Sleep(int ms) {
    struct timespec t = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&t, NULL);  // Ctrl+C прервёт ожидание
}

// Оба поля рядом, корабли видны (. вода, o промах, X попал, # корабль)
static void PrintBoards(const Board* b, const char names[2][32]) {
    int n = b->size;
    Say("\n    %-*s    %s\n   ", n * 2 + 2, names[0], names[1]);
    for (int s = 0; s < 2; s++) {
        for (int x = 0; x < n; x++) Say("%c ", 'A' + x);
        Say("     ");
    }
    for (int y = 0; y < n; y++) {
        Say("\n%2d ", y + 1);
        for (int s = 0; s < 2; s++) {
            for (int x = 0; x < n; x++) {
                const Board* bd = &b[s];
                char c = bd->cell[y][x] ? (bd->shot[y][x] ? 'X' : '#') : (bd->shot[y][x] ? 'o' : '.');
                Say("%c ", c);
            }
            Say("   ");
        }
    }
    Say("\n");
}

typedef struct { int winner, moves, quit; } Outcome;  // winner -1 - никто

// Один бой. first - кто ходит первым, seed - для расстановки и игроков.
// Возвращает код завершения (0, 3, 4 или 128+сигнал)
static int Fight(const Config* c, unsigned seed, int first, Outcome* out) {
    char names[2][32];
    snprintf(names[0], 32, "Нельсон(%s)", c->strat[0]);
    snprintf(names[1], 32, "Ушаков(%s)", c->strat[1]);
    Board bd[2];
    Player pl[2];
    for (int i = 0; i < 2; i++) {
        uint64_t rng = ((uint64_t)seed * 31 + 7 + i * 1000) * 0x9E3779B97F4A7C15ull;
        if (BoardPlace(&bd[i], c->size, c->touch, &rng) != 0) {
            Say("Не получилось расставить флот для %s\n", names[i]);
            return 3;
        }
        PlayerInit(&pl[i], c, i, seed);
    }

    int shots[2] = {0, 0}, hits[2] = {0, 0}, sunk[2] = {0, 0};
    int cur = first, moves = 0;
    *out = (Outcome){-1, 0, 0};
    while (!g_stop) {
        if (c->max_moves && moves >= c->max_moves) break;
        int x = 0, y = 0;
        if (PlayerChoose(&pl[cur], &x, &y) != 0) {  // человек вышел
            out->quit = 1;
            break;
        }
        Result r = BoardShoot(&bd[1 - cur], x, y);
        if (r.kind == REPEAT) {
            Say("Ошибка: %s выстрелил второй раз в %c%d\n", names[cur], 'A' + x, y + 1);
            return 4;
        }
        const char* err = BoardCheck(&bd[1 - cur]);
        if (err) {
            Say("Судья нашёл ошибку: %s\n", err);
            return 4;
        }
        moves++;
        shots[cur]++;
        if (r.kind != MISS) hits[cur]++;
        if (r.kind == SUNK) sunk[cur]++;
        PlayerOnResult(&pl[cur], x, y, &r);
        Say("Ход %3d. %-18s -> %c%-2d %s\n", moves, names[cur], 'A' + x, y + 1,
            r.kind == MISS ? "мимо" : r.kind == HIT ? "ПОПАЛ" : "УБИЛ");
        if (bd[1 - cur].alive == 0) {
            out->winner = cur;
            break;
        }
        if (!(r.kind != MISS && c->again)) cur = 1 - cur;
        if (c->delay) Sleep(c->delay);
    }
    out->moves = moves;

    PrintBoards(bd, names);
    if (g_stop) Say("\nБой прерван сигналом %d после %d ходов\n", (int)g_stop, moves);
    else if (out->quit) Say("\nИгрок вышел из боя после %d ходов\n", moves);
    else if (out->winner >= 0) Say("\nПобедил %s за %d ходов\n", names[out->winner], moves);
    else Say("\nДостигнут лимит ходов (%d), ничья\n", moves);
    for (int i = 0; i < 2; i++)
        Say("%-18s выстрелов %3d, попаданий %3d (%.0f%%), убито кораблей %d\n", names[i], shots[i], hits[i],
            shots[i] ? 100.0 * hits[i] / shots[i] : 0.0, sunk[i]);
    return g_stop ? 128 + g_stop : 0;
}

int Play(const Config* c) {
    if (strcmp(c->log, "none") != 0) {
        s_log = open(c->log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (s_log < 0) dprintf(2, "не удалось открыть журнал %s\n", c->log);
    }
    Say("Морской бой %dx%d, seed=%u, касание=%s, после попадания %s, боёв: %d\n", c->size, c->size, c->seed,
        c->touch ? "можно" : "нельзя", c->again ? "ходит ещё раз" : "ход переходит", c->games);

    Outcome o;
    int code = 0;
    if (c->games == 1) {
        code = Fight(c, c->seed, 0, &o);
    } else {
        // Серия боёв: ходы не печатаем, считаем итоги. Первый ходящий меняется
        int wins[2] = {0, 0}, draws = 0, played = 0;
        long total = 0;
        int min = 1 << 30, max = 0;
        s_quiet = 1;
        for (int i = 0; i < c->games && !g_stop && code == 0; i++) {
            code = Fight(c, c->seed + i, i % 2, &o);
            if (code != 0) break;
            played++;
            if (o.winner >= 0) wins[o.winner]++;
            else draws++;
            total += o.moves;
            if (o.moves < min) min = o.moves;
            if (o.moves > max) max = o.moves;
        }
        s_quiet = 0;
        if (g_stop) code = 128 + g_stop;
        Say("\nСыграно боёв: %d\n", played);
        Say("Победы: Нельсон(%s) %d, Ушаков(%s) %d, ничьих %d\n", c->strat[0], wins[0], c->strat[1], wins[1], draws);
        if (played) Say("Ходов за бой: в среднем %.1f, минимум %d, максимум %d\n", (double)total / played, min, max);
    }
    if (s_log >= 0) close(s_log);
    return code;
}
