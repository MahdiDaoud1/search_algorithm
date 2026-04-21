#ifndef MAZE_H
#define MAZE_H

#include <stdbool.h>
#include <stddef.h>

// ─── Grid dimensions ───────────────────────────────────────────────────────
#define COLS        31       
#define ROWS        31        
#define CELL_SIZE   22
#define WALL_THICK  2

// ─── Window / UI ────────────────────────────────────────────────────────────
#define SIDEBAR_W   260
#define WIN_W       (COLS * CELL_SIZE + SIDEBAR_W)
#define WIN_H       (ROWS * CELL_SIZE + 60)
#define GRID_OFF_X  0
#define GRID_OFF_Y  30

// ─── Cell flags ─────────────────────────────────────────────────────────────
#define WALL   0
#define OPEN   1

// ─── Cell states for rendering ───────────────────────────────────────────────
typedef enum {
    STATE_WALL    = 0,
    STATE_OPEN    = 1,
    STATE_START   = 2,
    STATE_END     = 3,
    STATE_VISITED = 4,
    STATE_FRONTIER= 5,
    STATE_PATH    = 6,
} CellState;

// ─── Search algorithms ───────────────────────────────────────────────────────
typedef enum {
    ALGO_BFS = 0,
    ALGO_DFS,
    ALGO_DIJKSTRA,
    ALGO_ASTAR,
    ALGO_COUNT
} SearchAlgo;

// ─── Maze generators ────────────────────────────────────────────────────────
typedef enum {
    GEN_RECURSIVE_BACKTRACK = 0,
    GEN_PRIMS,
    GEN_KRUSKALS,
    GEN_ALDOUS_BRODER,
    GEN_BRAIDED,          // cyclic maze — multiple correct paths
    GEN_COUNT
} GenAlgo;

// ─── App state machine ───────────────────────────────────────────────────────
typedef enum {
    PHASE_MENU = 0,
    PHASE_PICK_START,
    PHASE_PICK_END,
    PHASE_SEARCHING,
    PHASE_DONE,
} AppPhase;

// ─── Grid ────────────────────────────────────────────────────────────────────
typedef struct {
    int cells[ROWS][COLS];   // WALL / OPEN
    CellState vis[ROWS][COLS];
    int startR, startC;
    int endR,   endC;
} Grid;

// ─── Search result ───────────────────────────────────────────────────────────
typedef struct {
    int pathLen;
    int visited;
    double timeMs;
} SearchResult;

// ─── Function prototypes ─────────────────────────────────────────────────────

/* grid.c */
void grid_init(Grid *g);
void grid_clear_search(Grid *g);

/* generators.c */
void gen_recursive_backtrack(Grid *g, unsigned int seed);
void gen_prims(Grid *g, unsigned int seed);
void gen_kruskals(Grid *g, unsigned int seed);
void gen_aldous_broder(Grid *g, unsigned int seed);
void gen_braided(Grid *g, unsigned int seed);       /* cyclic — no unique path */

/* search.c  — step-by-step iterators */
typedef struct SearchCtx SearchCtx;
SearchCtx *search_create(SearchAlgo algo, Grid *g);
bool       search_step(SearchCtx *ctx);   /* false = finished */
void       search_destroy(SearchCtx *ctx);
SearchResult search_get_result(SearchCtx *ctx);

/* render.c */
void render_grid(const Grid *g);
void render_sidebar(AppPhase phase, GenAlgo gen, SearchAlgo algo,
                    const SearchResult *res, bool searching);
void render_topbar(AppPhase phase);

/* ui.c */
bool button(int x, int y, int w, int h, const char *label, bool active);

#endif /* MAZE_H */
