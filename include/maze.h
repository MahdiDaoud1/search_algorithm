#ifndef MAZE_H
#define MAZE_H

#include <stdbool.h>

// ─── Grid dimensions ─────────────────────────────────────────────────────────
#define COLS        31        // must be odd
#define ROWS        31        // must be odd
#define CELL_SIZE   24
#define WALL_THICK  2

// ─── Window ──────────────────────────────────────────────────────────────────
#define SIDEBAR_W   300
#define WIN_W       (COLS * CELL_SIZE + SIDEBAR_W)
#define WIN_H       (ROWS * CELL_SIZE + 50)
#define GRID_OFF_X  0
#define GRID_OFF_Y  50

// ─── Trap score constants ─────────────────────────────────────────────────────
#define PEN_SPIKE   80
#define PEN_FREEZE  50
#define PEN_TELE    65
#define BON_BOMB    40    // unused in score — bomb effect is structural only
#define BON_SPEED   30
#define BON_COIN    15

// ─── CellType — permanent logical state of each cell ─────────────────────────
// Written by generators and traps_place. Never modified during search.
// CELL_WALL=0 so grid_init memset(0) works correctly.
typedef enum {
    CELL_WALL        = 0,
    CELL_OPEN        = 1,
    CELL_START       = 2,
    CELL_END         = 3,
    CELL_TRAP_SPIKE  = 4,   // penalty +80
    CELL_TRAP_FREEZE = 5,   // penalty +50
    CELL_TRAP_TELE   = 6,   // penalty +65
    CELL_BONUS_BOMB  = 7,   // bonus  -40  + opens adjacent wall during search
    CELL_BONUS_SPEED = 8,   // bonus  -30
    CELL_BONUS_COIN  = 9,   // bonus  -15
} CellType;

// ─── VisState — search overlay, reset between searches ───────────────────────
typedef enum {
    VIS_NONE      = 0,   // show base CellType
    VIS_VISITED   = 1,   // algorithm fully processed
    VIS_FRONTIER  = 2,   // in queue/stack, not yet processed
    VIS_PATH      = 3,   // final solution path
    VIS_EXPLODED  = 4,   // wall opened by bomb — restore to CELL_WALL on full reset
} VisState;

// ─── Search algorithms ────────────────────────────────────────────────────────
typedef enum {
    ALGO_BFS = 0,
    ALGO_DFS,
    ALGO_DIJKSTRA,
    ALGO_ASTAR,
    ALGO_COUNT
} SearchAlgo;

// ─── Maze generators ──────────────────────────────────────────────────────────
typedef enum {
    GEN_RECURSIVE_BACKTRACK = 0,
    GEN_PRIMS,
    GEN_KRUSKALS,
    GEN_ALDOUS_BRODER,
    GEN_BRAIDED,
    GEN_WILSONS,
    GEN_COUNT
} GenAlgo;

// ─── App phases ───────────────────────────────────────────────────────────────
typedef enum {
    PHASE_MENU = 0,
    PHASE_PICK_START,
    PHASE_PICK_END,
    PHASE_SEARCHING,
    PHASE_DONE,
} AppPhase;

// ─── Grid ─────────────────────────────────────────────────────────────────────
#define MAX_CRATERS 16   // max bombs that can fire in one search run

typedef struct { int r, c; } CraterPos;

typedef struct {
    CellType  cells[ROWS][COLS];      // permanent — generators + traps write here
    VisState  vis[ROWS][COLS];        // search overlay — cleared between searches
    CraterPos craters[MAX_CRATERS];   // positions of bomb-opened walls
    int       crater_count;           // how many craters exist right now
    int startR, startC;
    int endR,   endC;
} Grid;

// ─── Per-tile event log ───────────────────────────────────────────────────────
#define MAX_EVENTS 64

typedef struct {
    CellType type;
    int      r, c;
    int      delta;   // positive = penalty, negative = bonus
} TileEvent;

// ─── Search result ────────────────────────────────────────────────────────────
typedef struct {
    int    pathLen;
    int    visited;
    double timeMs;
    int    penalties;
    int    bonuses;
    int    traps_hit;
    int    bonuses_hit;
    int    walls_opened;
    int    final_score;
    // per-tile event log for the end popup
    TileEvent events[MAX_EVENTS];
    int       event_count;
} SearchResult;

// ─── Prototypes ───────────────────────────────────────────────────────────────

/* grid.c */
void grid_init(Grid *g);
void grid_clear_search(Grid *g);
void grid_full_reset(Grid *g);   // restore maze to post-generation state (undo bombs, clear start/end)

/* generators.c */
void gen_recursive_backtrack(Grid *g);
void gen_prims(Grid *g);
void gen_kruskals(Grid *g);
void gen_aldous_broder(Grid *g);
void gen_braided(Grid *g);
void gen_wilsons(Grid *g);

/* traps.c */
void traps_place(Grid *g);
int  cell_penalty(CellType t);   // >0 penalty, <0 bonus, 0 neutral
bool cell_is_special(CellType t);

/* search.c */
typedef struct SearchCtx SearchCtx;
SearchCtx   *search_create(SearchAlgo algo, Grid *g);
bool         search_step(SearchCtx *ctx);
void         search_destroy(SearchCtx *ctx);
SearchResult search_get_result(SearchCtx *ctx);

/* render.c */
void render_init(void);
void render_free(void);
void render_grid(const Grid *g);
void render_sidebar(AppPhase phase, GenAlgo gen, SearchAlgo algo,
                    const SearchResult *res, bool searching);
void render_topbar(AppPhase phase);
void render_report_popup(const SearchResult *res, SearchAlgo algo,
                         bool *show);   // sets *show=false when closed

/* ui.c */
bool button(int x, int y, int w, int h, const char *label, bool active);

#endif
