#include "maze.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── helpers ──────────────────────────────────────────────────────────────── */
static unsigned int rng_state;
static unsigned int rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void shuffle_dirs(int dirs[4]) {
    for (int i = 3; i > 0; i--) {
        int j = rng() % (i + 1);
        int tmp = dirs[i]; dirs[i] = dirs[j]; dirs[j] = tmp;
    }
}

// carve between two cells (r1,c1) and (r2,c2) — also opens the wall between
static void carve(Grid *g, int r1, int c1, int r2, int c2) {
    g->cells[r1][c1] = OPEN;
    g->cells[(r1+r2)/2][(c1+c2)/2] = OPEN;
    g->cells[r2][c2] = OPEN;
    g->vis[r1][c1] = STATE_OPEN;
    g->vis[(r1+r2)/2][(c1+c2)/2] = STATE_OPEN;
    g->vis[r2][c2] = STATE_OPEN;
}

//Recursive Backtracker (DFS) 
static void rb_visit(Grid *g, int r, int c) {
    int dr[] = {-2,  2,  0,  0};
    int dc[] = { 0,  0, -2,  2};
    int dirs[4] = {0,1,2,3};
    shuffle_dirs(dirs);
    for (int i = 0; i < 4; i++) {
        int nr = r + dr[dirs[i]];
        int nc = c + dc[dirs[i]];
        if (nr > 0 && nr < ROWS-1 && nc > 0 && nc < COLS-1
                && g->cells[nr][nc] == WALL) {
            carve(g, r, c, nr, nc);
            rb_visit(g, nr, nc);
        }
    }
}

void gen_recursive_backtrack(Grid *g, unsigned int seed) {
    rng_state = seed ? seed : (unsigned int)time(NULL);
    grid_init(g);
    rb_visit(g, 1, 1);
}

/* ── 2. Prim's (randomised) ──────────────────────────────────────────────── */
#define MAX_FRONTIER (ROWS * COLS)
typedef struct { int r, c; } Cell;

void gen_prims(Grid *g, unsigned int seed) {
    rng_state = seed ? seed : (unsigned int)time(NULL);
    grid_init(g);

    Cell *frontier = malloc(MAX_FRONTIER * sizeof(Cell));
    int   fsize = 0;

    // start at (1,1)
    g->cells[1][1] = OPEN;
    g->vis[1][1]   = STATE_OPEN;

    int dr[] = {-2, 2, 0, 0};
    int dc[] = {0, 0, -2, 2};

#define ADD_FRONTIER(rr,cc) \
    if ((rr)>0&&(rr)<ROWS-1&&(cc)>0&&(cc)<COLS-1&&g->cells[(rr)][(cc)]==WALL)\
        frontier[fsize++] = (Cell){(rr),(cc)};

    ADD_FRONTIER(1,3) ADD_FRONTIER(3,1)

    while (fsize > 0) {
        int idx = rng() % fsize;
        Cell cur = frontier[idx];
        frontier[idx] = frontier[--fsize];

        if (g->cells[cur.r][cur.c] == OPEN) continue;

        // collect open neighbours
        Cell nbrs[4]; int nn = 0;
        for (int d = 0; d < 4; d++) {
            int nr = cur.r + dr[d], nc = cur.c + dc[d];
            if (nr>0&&nr<ROWS-1&&nc>0&&nc<COLS-1&&g->cells[nr][nc]==OPEN)
                nbrs[nn++] = (Cell){nr,nc};
        }
        if (nn == 0) continue;
        Cell chosen = nbrs[rng() % nn];
        carve(g, cur.r, cur.c, chosen.r, chosen.c);

        for (int d = 0; d < 4; d++) {
            ADD_FRONTIER(cur.r+dr[d], cur.c+dc[d])
        }
    }
    free(frontier);
}

/* ── 3. Kruskal's (randomised) ───────────────────────────────────────────── */
static int parent[ROWS*COLS];
static int find(int x) { return parent[x]==x ? x : (parent[x]=find(parent[x])); }
static void unite(int a, int b) { parent[find(a)] = find(b); }

void gen_kruskals(Grid *g, unsigned int seed) {
    rng_state = seed ? seed : (unsigned int)time(NULL);
    grid_init(g);

    // open all odd cells
    for (int r = 1; r < ROWS; r+=2)
        for (int c = 1; c < COLS; c+=2) {
            g->cells[r][c] = OPEN;
            g->vis[r][c]   = STATE_OPEN;
            parent[r*COLS+c] = r*COLS+c;
        }

    // collect all walls between odd cells
    typedef struct { int r1,c1,r2,c2; } Wall;
    Wall *walls = malloc(2*ROWS*COLS*sizeof(Wall));
    int wcount = 0;
    for (int r = 1; r < ROWS; r+=2)
        for (int c = 1; c < COLS; c+=2) {
            if (r+2 < ROWS) walls[wcount++] = (Wall){r,c,r+2,c};
            if (c+2 < COLS) walls[wcount++] = (Wall){r,c,r,c+2};
        }

    // shuffle walls
    for (int i = wcount-1; i > 0; i--) {
        int j = rng() % (i+1);
        Wall tmp = walls[i]; walls[i] = walls[j]; walls[j] = tmp;
    }

    for (int i = 0; i < wcount; i++) {
        Wall w = walls[i];
        if (find(w.r1*COLS+w.c1) != find(w.r2*COLS+w.c2)) {
            unite(w.r1*COLS+w.c1, w.r2*COLS+w.c2);
            carve(g, w.r1, w.c1, w.r2, w.c2);
        }
    }
    free(walls);
}

/* ── 4. Aldous-Broder ────────────────────────────────────────────────────── */
void gen_aldous_broder(Grid *g, unsigned int seed) {
    rng_state = seed ? seed : (unsigned int)time(NULL);
    grid_init(g);

    int total = ((ROWS-1)/2) * ((COLS-1)/2);
    int visited = 1;
    int r = 1, c = 1;
    g->cells[r][c] = OPEN;
    g->vis[r][c]   = STATE_OPEN;

    int dr[] = {-2,2,0,0};
    int dc[] = {0,0,-2,2};

    while (visited < total) {
        int d = rng() % 4;
        int nr = r+dr[d], nc = c+dc[d];
        if (nr<=0||nr>=ROWS-1||nc<=0||nc>=COLS-1) continue;
        if (g->cells[nr][nc] == WALL) {
            carve(g, r, c, nr, nc);
            visited++;
        }
        r = nr; c = nc;
    }
}

/* ── 5. Braided maze (cyclic — multiple correct paths) ───────────────────────
 *
 *  Strategy:
 *   a) Build a perfect maze first (Recursive Backtracker → unique-path tree).
 *   b) Collect every wall that still separates two OPEN cells.
 *   c) Randomly remove ~40 % of those walls.
 *
 *  This punches "shortcuts" (back-edges) into the spanning tree, creating
 *  cycles so that almost any two cells are connected by several distinct
 *  routes.  The more walls removed, the more open and loopy the maze becomes.
 * ──────────────────────────────────────────────────────────────────────────── */
#define BRAID_RATIO 0.40f   /* fraction of extra walls to remove (0–1) */

void gen_braided(Grid *g, unsigned int seed) {
    /* ── step 1 : perfect maze via recursive backtracker ── */
    rng_state = seed ? seed : (unsigned int)time(NULL);
    gen_recursive_backtrack(g, rng_state);   /* re-uses our rng_state */

    /* ── step 2 : collect all walls that divide two open cells ──
     *
     *  A "removable" wall is any cell that is currently WALL and has
     *  exactly two OPEN neighbours on opposite sides (horizontal pair
     *  or vertical pair).  Removing it merges two corridors.
     *
     *  We scan every interior cell.  Walls on the border (row 0, ROWS-1,
     *  col 0, COLS-1) are never touched so the outer boundary stays solid.
     */
    typedef struct { int r, c; } WPos;
    WPos *candidates = malloc(ROWS * COLS * sizeof(WPos));
    int   ncand = 0;

    for (int r = 1; r < ROWS - 1; r++) {
        for (int c = 1; c < COLS - 1; c++) {
            if (g->cells[r][c] != WALL) continue;

            /* horizontal pair: left and right neighbours both open */
            int horiz = (g->cells[r][c-1] == OPEN && g->cells[r][c+1] == OPEN);
            /* vertical pair: top and bottom neighbours both open */
            int vert  = (g->cells[r-1][c] == OPEN && g->cells[r+1][c] == OPEN);

            if (horiz || vert)
                candidates[ncand++] = (WPos){r, c};
        }
    }

    /* ── step 3 : shuffle candidates then remove the first BRAID_RATIO of them ── */
    for (int i = ncand - 1; i > 0; i--) {
        int j = rng() % (i + 1);
        WPos tmp = candidates[i]; candidates[i] = candidates[j]; candidates[j] = tmp;
    }

    int to_remove = (int)(ncand * BRAID_RATIO);
    for (int i = 0; i < to_remove; i++) {
        int r = candidates[i].r;
        int c = candidates[i].c;
        g->cells[r][c] = OPEN;
        g->vis[r][c]   = STATE_OPEN;
    }

    free(candidates);
}
