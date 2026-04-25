#include "maze.h"
#include <stdlib.h>

/*
 * traps.c
 * Scatters trap and bonus tiles on open corridor cells AFTER maze generation.
 * Writes to cells[][] only — the permanent layer.
 * Never touches vis[][].
 *
 * Tile effects:
 *   CELL_TRAP_SPIKE  +80  skull      heavy penalty
 *   CELL_TRAP_FREEZE +50  ice        medium penalty
 *   CELL_TRAP_TELE   +65  vortex     medium-heavy penalty
 *   CELL_BONUS_BOMB   0   explosion  no score effect — opens one adjacent wall during search
 *   CELL_BONUS_SPEED -30  lightning  bonus
 *   CELL_BONUS_COIN  -15  coin       small bonus
 *
 * Score formula (lower = better):
 *   final_score = pathLen*5 + penalties*3 - bonuses*2
 */

static const CellType TYPES[] = {
    CELL_TRAP_SPIKE,  CELL_TRAP_SPIKE,  CELL_TRAP_SPIKE,  CELL_TRAP_SPIKE,
    CELL_TRAP_FREEZE, CELL_TRAP_FREEZE, CELL_TRAP_FREEZE,
    CELL_TRAP_TELE,   CELL_TRAP_TELE,   CELL_TRAP_TELE,
    CELL_BONUS_BOMB,  CELL_BONUS_BOMB,  CELL_BONUS_BOMB,
    CELL_BONUS_SPEED, CELL_BONUS_SPEED, CELL_BONUS_SPEED, CELL_BONUS_SPEED,
    CELL_BONUS_COIN,  CELL_BONUS_COIN,  CELL_BONUS_COIN,
    CELL_BONUS_COIN,  CELL_BONUS_COIN,
};
#define N_TYPES (int)(sizeof(TYPES)/sizeof(TYPES[0]))

void traps_place(Grid *g) {
    typedef struct{int r,c;}Pos;
    Pos *pool = malloc(ROWS*COLS*sizeof(Pos));
    int  ps   = 0;

    for (int r=1;r<ROWS-1;r++)
        for (int c=1;c<COLS-1;c++)
            if (g->cells[r][c]==CELL_OPEN)
                pool[ps++]=(Pos){r,c};

    // Fisher-Yates shuffle
    for (int i=ps-1;i>0;i--) {
        int j=rand()%(i+1);
        Pos t=pool[i]; pool[i]=pool[j]; pool[j]=t;
    }

    int place = ps < N_TYPES ? ps : N_TYPES;
    for (int i=0;i<place;i++)
        g->cells[pool[i].r][pool[i].c]=TYPES[i];

    free(pool);
}

// positive = penalty, negative = bonus, 0 = neutral
int cell_penalty(CellType t) {
    switch (t) {
        case CELL_TRAP_SPIKE:  return  PEN_SPIKE;
        case CELL_TRAP_FREEZE: return  PEN_FREEZE;
        case CELL_TRAP_TELE:   return  PEN_TELE;
        case CELL_BONUS_BOMB:  return  0;      // structural effect only — no score bonus
        case CELL_BONUS_SPEED: return -BON_SPEED;
        case CELL_BONUS_COIN:  return -BON_COIN;
        default:               return  0;
    }
}

bool cell_is_special(CellType t) {
    return t >= CELL_TRAP_SPIKE && t <= CELL_BONUS_COIN;
}
