#include "maze.h"
#include <string.h>

void grid_init(Grid *g) {
    memset(g->cells,  0, sizeof(g->cells));
    memset(g->vis,    0, sizeof(g->vis));
    g->crater_count = 0;
    g->startR = g->startC = -1;
    g->endR   = g->endC   = -1;
}

/* grid_clear_search — wipe search colours (VISITED/FRONTIER/PATH/EXPLODED).
 * Does NOT restore craters — craters persist until grid_full_reset.
 * cells[][] is never touched here. */
void grid_clear_search(Grid *g) {
    for (int r=0;r<ROWS;r++)
        for (int c=0;c<COLS;c++) {
            VisState v = g->vis[r][c];
            if (v==VIS_VISITED||v==VIS_FRONTIER||v==VIS_PATH)
                g->vis[r][c] = VIS_NONE;
            /* leave VIS_EXPLODED so craters remain visible */
        }
    if (g->startR>=0) g->cells[g->startR][g->startC] = CELL_START;
    if (g->endR  >=0) g->cells[g->endR  ][g->endC  ] = CELL_END;
}

/* grid_full_reset — restore maze to post-generation state.
 * Reads from g->craters[] which is written by bomb_effect and never
 * touched by vis or search code — guaranteed reliable. */
void grid_full_reset(Grid *g) {
    /* restore every bomb-opened wall */
    for (int i=0;i<g->crater_count;i++) {
        int r = g->craters[i].r;
        int c = g->craters[i].c;
        g->cells[r][c] = CELL_WALL;
        g->vis[r][c]   = VIS_NONE;
    }
    g->crater_count = 0;

    /* clear start/end back to open, wipe all vis */
    for (int r=0;r<ROWS;r++)
        for (int c=0;c<COLS;c++) {
            if (g->cells[r][c]==CELL_START || g->cells[r][c]==CELL_END)
                g->cells[r][c] = CELL_OPEN;
            g->vis[r][c] = VIS_NONE;
        }

    g->startR = g->startC = -1;
    g->endR   = g->endC   = -1;
}
