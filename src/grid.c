#include "maze.h"
#include <string.h>

void grid_init(Grid *g) {
    memset(g->cells, WALL, sizeof(g->cells));
    memset(g->vis,   0,    sizeof(g->vis));
    g->startR = g->startC = -1;
    g->endR   = g->endC   = -1;
}

void grid_clear_search(Grid *g) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            CellState s = g->vis[r][c];
            if (s == STATE_VISITED || s == STATE_FRONTIER || s == STATE_PATH)
                g->vis[r][c] = (g->cells[r][c] == OPEN) ? STATE_OPEN : STATE_WALL;
        }
    // restore start/end markers
    if (g->startR >= 0) g->vis[g->startR][g->startC] = STATE_START;
    if (g->endR   >= 0) g->vis[g->endR  ][g->endC  ] = STATE_END;
}
