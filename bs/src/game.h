// game.h
// Общие типы для всех файлов: настройки, поле, игрок, результат выстрела.
#ifndef GAME_H
#define GAME_H

#include <signal.h>
#include <stdint.h>

#define MAXN 16      // максимальный размер поля
#define MAXSHIPS 16  // кораблей в флоте (классический флот - 10)

typedef enum { MISS, HIT, SUNK, REPEAT } Kind;

// Что судья сообщает игроку после выстрела. Если корабль убит,
// то говорим ещё где он стоял, чтобы игрок мог пометить клетки вокруг.
typedef struct {
    Kind kind;
    int len, x0, y0, dx, dy;
} Result;

typedef struct {
    int size;       // размер поля (8..16)
    int touch;      // 1 - корабли могут касаться друг друга, 0 - нет
    int again;      // 1 - после попадания игрок ходит ещё раз
    int max_moves;  // лимит выстрелов на двоих (0 - без лимита)
    int delay;      // пауза между ходами, мс
    int games;      // сколько боёв сыграть подряд (больше 1 - серия)
    unsigned seed;  // с одним seed бой повторяется
    char strat[2][8];   // random, hunt или human
    char log[64];   // имя файла журнала ("none" - не писать)
} Config;

typedef struct { int len, hits, x0, y0, dx, dy; } Ship;

// Поле есть только у судьи, игроки его не видят
typedef struct {
    int size;
    int cell[MAXN][MAXN];  // 0 - вода, иначе номер корабля + 1
    int shot[MAXN][MAXN];  // 1 - сюда уже стреляли
    Ship ships[MAXSHIPS];
    int n_ships, alive;
} Board;

typedef struct {
    int size, touch;
    char strat[8];                  // random, hunt или human
    int known[MAXN][MAXN];          // 0 не знаем, 1 промах, 2 попал, 3 убитый корабль
    uint64_t rng;
} Player;

extern volatile sig_atomic_t g_stop;  // ставится в обработчике сигнала

uint32_t Rand(uint64_t* s, uint32_t n);  // число от 0 до n-1

int BoardPlace(Board* b, int size, int touch, uint64_t* rng);  // 0 - успех
Result BoardShoot(Board* b, int x, int y);
const char* BoardCheck(const Board* b);  // NULL если всё правильно

void PlayerInit(Player* p, const Config* c, int id, unsigned seed);
int PlayerChoose(Player* p, int* x, int* y);  // 0 - выбрал, -1 - человек вышел
void PlayerOnResult(Player* p, int x, int y, const Result* r);

int Play(const Config* c);  // главный цикл боя, возвращает код завершения

#endif
