#include "maze.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// importing libin103
#include "generic_queue.h"
#include "generic_stack.h"
#include "generic_heap.h"

// time settings
#ifdef _WIN32
  #include <windows.h>
  static double get_time_ms(void) { return (double)timeGetTime(); }
#else
  #include <time.h>
  static double get_time_ms(void) {
      struct timespec t;
      clock_gettime(CLOCK_MONOTONIC, &t);
      return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
  }
#endif

// -----------------------------------------------------------------------------------
//  Node — one cell in the search, with its position, parent, and cost
//  Parent (not children) because path reconstruction walks BACKWARD:
//  goal -> parent -> parent -> ... -> start.
//  ----------------------------------------------------------------------------------*/

typedef struct Node {
    int   row, col;
    int   parent_row, parent_col;
    float cost;    // real cost from start to here
    float score;   // cost + heuristic for A*, same as cost for Dijkstra
} Node;

// --------------------------- libin103 callback functions -----------------------------------------
// generic structure in libin103 needs :
//   build, destroy, compare: (used by heap for min/max)
//------------------------------------------------------------

static void *node_build(const void *data) {
    Node *copy = malloc(sizeof(Node));
    *copy = *(const Node *)data;
    return copy;
}

static void node_destroy(void *data) {
    free(data);
}

//for the heap, return 1 to send fist variable to the root and -1 for the other case
static int node_compare(const void *a, const void *b) {
    float fa = ((const Node *)a)->score;
    float fb = ((const Node *)b)->score;
    if (fa < fb) return  1;
    if (fa > fb) return -1;
    return 0;
}

//--------------------------------------------------------------------------------------

struct SearchCtx {
    SearchAlgo algo;
    Grid      *grid;
    bool       done, found;//in hope for newer version where the user will be able to draw the maze himself
                            // done and found will be different when the user draw a maze where no path exist
                            // not sure if this version will be available before the deadline

    int   parent_row[ROWS][COLS];
    int   parent_col[ROWS][COLS];
    bool  visited[ROWS][COLS]; //different from vis, set to true when elmt enqueued, the other for dequeued
    float cost[ROWS][COLS];

    // bidirectional Dijkstra only
    int   back_parent_row[ROWS][COLS];
    int   back_parent_col[ROWS][COLS];
    bool  back_visited[ROWS][COLS];
    float back_cost[ROWS][COLS];
    int   meet_row, meet_col;

    generic_queue_t queue;       // BFS
    generic_stack_t stack;       // DFS
    generic_heap_t  heap;        // Dijkstra / A*
    generic_heap_t  back_heap;   // bidirectional Dijkstra backward

    SearchResult result; //Report data
    double       start_time_ms;
};

// directions
static const int DELTA_ROW[4] = {-1, 1,  0, 0};
static const int DELTA_COL[4] = { 0, 0, -1, 1};

static float manhattan(int row, int col, int goal_row, int goal_col) {
    return (float)(abs(row - goal_row) + abs(col - goal_col));
}

static float edge_cost(SearchAlgo algo, CellType cell_type) {
    if (algo == ALGO_BFS || algo == ALGO_DFS) return 1.0f;
    int penalty = cell_penalty(cell_type);
    return 1.0f + (penalty > 0 ? (float)penalty / 15.0f : 0.0f);
}


static int bomb_effect(Grid *grid, int row, int col) {
    int dr[] = {-1,-1,-1, 0, 0, 1, 1, 1};
    int dc[] = {-1, 0, 1,-1, 1,-1, 0, 1};
    int opened = 0;

    for (int d = 0; d < 8; d++) {
        int wr = row + dr[d], wc = col + dc[d];
        if (wr <= 0 || wr >= ROWS-1 || wc <= 0 || wc >= COLS-1) continue; //not destroy an edge
        if (grid->cells[wr][wc] != CELL_WALL) continue; //destroy only walls ofc

        grid->cells[wr][wc] = CELL_OPEN;
        grid->vis[wr][wc]   = VIS_EXPLODED;
        if (grid->crater_count < MAX_CRATERS) {
            grid->craters[grid->crater_count].r = wr;
            grid->craters[grid->crater_count].c = wc;
            grid->crater_count++;
        }
        opened++;
    }
    return opened;
}

// ----------------------- path tracing -------------------------------------------
static void trace_path(SearchCtx *ctx) {
    Grid *grid = ctx->grid;
    int row = grid->endR, col = grid->endC;
    int length = 0, penalties = 0, bonuses = 0,
        traps_hit = 0, bonuses_hit = 0, walls_opened = 0;
    ctx->result.event_count = 0;

    while (!(row == grid->startR && col == grid->startC)) {
        if (row < 0) break; //* Should never trigger
        CellType cell = grid->cells[row][col];

        if (cell != CELL_END && cell != CELL_START) {
            int delta = cell_penalty(cell);
            if (delta > 0) { penalties += delta; traps_hit++; }
            else if (delta < 0) { bonuses += (-delta); bonuses_hit++; }

            if (cell == CELL_BONUS_BOMB) {
                int dr[] = {-1,-1,-1, 0, 0, 1, 1, 1};
                int dc[] = {-1, 0, 1,-1, 1,-1, 0, 1};
                int bomb_walls = 0;
                for (int d = 0; d < 8; d++) {
                    int wr = row+dr[d], wc = col+dc[d];
                    for (int k = 0; k < grid->crater_count; k++)
                        if (grid->craters[k].r == wr && grid->craters[k].c == wc)
                            bomb_walls++;
                }
                walls_opened += bomb_walls;
                if (ctx->result.event_count < MAX_EVENTS) {  //should be always true
                    TileEvent ev = {cell, row, col, bomb_walls};
                    ctx->result.events[ctx->result.event_count++] = ev;
                }
            } else if (cell_is_special(cell) && ctx->result.event_count < MAX_EVENTS) {
                TileEvent ev = {cell, row, col, delta};
                ctx->result.events[ctx->result.event_count++] = ev;
            }
        }

        grid->vis[row][col] = VIS_PATH;
        int pr = ctx->parent_row[row][col];
        int pc = ctx->parent_col[row][col];
        row = pr; col = pc; length++;
    }

    grid->vis[grid->startR][grid->startC] = VIS_NONE; //? removing the yellow overlay
    grid->vis[grid->endR  ][grid->endC  ] = VIS_NONE;

    ctx->result.pathLen     = length;
    ctx->result.penalties   = penalties;
    ctx->result.bonuses     = bonuses;
    ctx->result.traps_hit   = traps_hit;
    ctx->result.bonuses_hit = bonuses_hit;
    ctx->result.walls_opened= walls_opened;
}

//for biDir Djiks
static void trace_bidir_path(SearchCtx *ctx) {
    Grid *grid = ctx->grid;
    int mr = ctx->meet_row, mc = ctx->meet_col;
    int length = 0, penalties = 0, bonuses = 0,
        traps_hit = 0, bonuses_hit = 0, walls_opened = 0;
    ctx->result.event_count = 0;

    // forward half: meeting point -> start
    int row = mr, col = mc;
    while (!(row == grid->startR && col == grid->startC)) {
        if (row < 0) break;
        CellType cell = grid->cells[row][col];
        if (cell != CELL_START && cell != CELL_END) {
            int delta = cell_penalty(cell);
            if (delta > 0) { penalties += delta; traps_hit++; }
            else if (delta < 0) { bonuses += (-delta); bonuses_hit++; }

            if (cell == CELL_BONUS_BOMB) {
                int dr[] = {-1,-1,-1, 0, 0, 1, 1, 1};
                int dc[] = {-1, 0, 1,-1, 1,-1, 0, 1};
                int bomb_walls = 0;
                for (int d = 0; d < 8; d++) {
                    int wr = row+dr[d], wc = col+dc[d];
                    for (int k = 0; k < grid->crater_count; k++)
                        if (grid->craters[k].r == wr && grid->craters[k].c == wc)
                            bomb_walls++;
                }
                walls_opened += bomb_walls;
                if (ctx->result.event_count < MAX_EVENTS) {
                    TileEvent ev = {cell, row, col, bomb_walls};
                    ctx->result.events[ctx->result.event_count++] = ev;
                }
            } else if (cell_is_special(cell) && ctx->result.event_count < MAX_EVENTS) {
                TileEvent ev = {cell, row, col, delta};
                ctx->result.events[ctx->result.event_count++] = ev;
            }
        }
        grid->vis[row][col] = VIS_PATH;
        int pr = ctx->parent_row[row][col];
        int pc = ctx->parent_col[row][col];
        row = pr; col = pc; length++;
    }

    // backward half: meeting point -> end
    row = mr; col = mc;
    while (!(row == grid->endR && col == grid->endC)) {
        if (row < 0) break;
        CellType cell = grid->cells[row][col];
        if (cell != CELL_START && cell != CELL_END) {
            int delta = cell_penalty(cell);
            if (delta > 0) { penalties += delta; traps_hit++; }
            else if (delta < 0) { bonuses += (-delta); bonuses_hit++; }

            if (cell == CELL_BONUS_BOMB) {
                int dr[] = {-1,-1,-1, 0, 0, 1, 1, 1};
                int dc[] = {-1, 0, 1,-1, 1,-1, 0, 1};
                int bomb_walls = 0;
                for (int d = 0; d < 8; d++) {
                    int wr = row+dr[d], wc = col+dc[d];
                    for (int k = 0; k < grid->crater_count; k++)
                        if (grid->craters[k].r == wr && grid->craters[k].c == wc)
                            bomb_walls++;
                }
                walls_opened += bomb_walls;
                if (ctx->result.event_count < MAX_EVENTS) {
                    TileEvent ev = {cell, row, col, bomb_walls};
                    ctx->result.events[ctx->result.event_count++] = ev;
                }
            } else if (cell_is_special(cell) && ctx->result.event_count < MAX_EVENTS) {
                TileEvent ev = {cell, row, col, delta};
                ctx->result.events[ctx->result.event_count++] = ev;
            }
        }
        grid->vis[row][col] = VIS_PATH;
        int pr = ctx->back_parent_row[row][col];
        int pc = ctx->back_parent_col[row][col];
        row = pr; col = pc; length++;
    }

    grid->vis[grid->startR][grid->startC] = VIS_NONE;
    grid->vis[grid->endR  ][grid->endC  ] = VIS_NONE;

    ctx->result.pathLen     = length;
    ctx->result.penalties   = penalties;
    ctx->result.bonuses     = bonuses;
    ctx->result.traps_hit   = traps_hit;
    ctx->result.bonuses_hit = bonuses_hit;
    ctx->result.walls_opened= walls_opened; 
}

//TODO:---------------------------------------------------------------------------------------------
// ═══════════════════════════════════════════════════════════════════════════════
// search_create
// ═══════════════════════════════════════════════════════════════════════════════
SearchCtx *search_create(SearchAlgo algo, Grid *grid) {
    SearchCtx *ctx = calloc(1, sizeof(SearchCtx));
    ctx->algo     = algo;
    ctx->grid     = grid;
    ctx->meet_row = ctx->meet_col = -1;

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            ctx->parent_row[r][c] = ctx->parent_col[r][c] = -1;
            ctx->back_parent_row[r][c] = ctx->back_parent_col[r][c] = -1;
            ctx->cost[r][c] = ctx->back_cost[r][c] = 1e30f;
        }
    ctx->cost[grid->startR][grid->startC] = 0.0f;

    Node start = {
        grid->startR, grid->startC, -1, -1,
        0.0f,
        manhattan(grid->startR, grid->startC, grid->endR, grid->endC)
    };

    switch (algo) {
        case ALGO_BFS:
            generic_queue_init(&ctx->queue, node_compare, node_build, node_destroy);
            generic_queue_enqueue(&ctx->queue, &start);
            ctx->visited[grid->startR][grid->startC] = true;
            break;

        case ALGO_DFS:
            generic_stack_init(&ctx->stack, node_compare, node_build, node_destroy);
            generic_stack_push(&ctx->stack, &start);
            break;

        case ALGO_BIDIR_DIJKSTRA:
            generic_heap_init(&ctx->heap, node_compare, node_build, node_destroy);
            generic_heap_insert(&ctx->heap, &start);
            ctx->back_cost[grid->endR][grid->endC] = 0.0f;
            Node back_start = {grid->endR, grid->endC, -1, -1, 0.0f, 0.0f};
            generic_heap_init(&ctx->back_heap, node_compare, node_build, node_destroy);
            generic_heap_insert(&ctx->back_heap, &back_start);
            break;

        default:  // Dijkstra and A*
            generic_heap_init(&ctx->heap, node_compare, node_build, node_destroy);
            generic_heap_insert(&ctx->heap, &start);
            break;
    }

    ctx->start_time_ms = get_time_ms();
    return ctx;
}

//--------------------------------------------------------------------------------------------
// search_step — advance one node, return false when done
//---------------------------------------------------------------------------------------------
bool search_step(SearchCtx *ctx) {
    if (ctx->done) return false;
    Grid *grid = ctx->grid;

    //  Bidirectional Dijkstra 
    if (ctx->algo == ALGO_BIDIR_DIJKSTRA) {

        if (generic_heap_size(&ctx->heap) > 0) {
            void *raw = NULL;
            generic_heap_extract(&ctx->heap, &raw);
            Node current = *(Node *)raw;    // to activate vs code auto comp
            node_destroy(raw);

            if (!ctx->visited[current.row][current.col] 
                    || current.cost < ctx->cost[current.row][current.col]) {
                ctx->visited[current.row][current.col] = true;
                ctx->cost[current.row][current.col] = current.cost;
                ctx->parent_row[current.row][current.col] = current.parent_row;
                ctx->parent_col[current.row][current.col] = current.parent_col;
                ctx->result.visited++;

                if (grid->cells[current.row][current.col] == CELL_BONUS_BOMB)
                    ctx->result.walls_opened += bomb_effect(grid, current.row, current.col);
                if (grid->vis[current.row][current.col] == VIS_NONE)
                    grid->vis[current.row][current.col] = VIS_VISITED;

                if (ctx->back_visited[current.row][current.col]) {
                    ctx->meet_row = current.row; ctx->meet_col = current.col;
                    ctx->done = ctx->found = true;
                    trace_bidir_path(ctx);
                    goto finish;
                }
                for (int d = 0; d < 4; d++) {
                    int nr = current.row + DELTA_ROW[d];
                    int nc = current.col + DELTA_COL[d];
                    if (nr < 0||nr >= ROWS||nc < 0||nc >= COLS) continue;
                    if (grid->cells[nr][nc] == CELL_WALL) continue;
                    if (ctx->visited[nr][nc]) continue;
                    float new_cost = ctx->cost[current.row][current.col]
                                   + edge_cost(ctx->algo, grid->cells[nr][nc]);
                    if (new_cost >= ctx->cost[nr][nc]) continue;
                    ctx->cost[nr][nc] = new_cost;
                    Node nb = {nr, nc, current.row, current.col, new_cost, new_cost};
                    generic_heap_insert(&ctx->heap, &nb);
                    if (grid->vis[nr][nc] == VIS_NONE) grid->vis[nr][nc] = VIS_FRONTIER;
                }
            }
        }

        if (generic_heap_size(&ctx->back_heap) > 0) {
            void *raw = NULL;
            generic_heap_extract(&ctx->back_heap, &raw);
            Node current = *(Node *)raw;
            node_destroy(raw);
            if (!ctx->back_visited[current.row][current.col]
                    || current.cost < ctx->back_cost[current.row][current.col]) {
                ctx->back_visited[current.row][current.col] = true;
                ctx->back_cost[current.row][current.col] = current.cost;
                ctx->back_parent_row[current.row][current.col] = current.parent_row;
                ctx->back_parent_col[current.row][current.col] = current.parent_col;
                ctx->result.visited++;

                if (grid->vis[current.row][current.col] == VIS_NONE)
                    grid->vis[current.row][current.col] = VIS_FRONTIER;

                if (ctx->visited[current.row][current.col]) {
                    ctx->meet_row = current.row; ctx->meet_col = current.col;
                    ctx->done = ctx->found = true;
                    trace_bidir_path(ctx);
                    goto finish;
                }
                for (int d = 0; d < 4; d++) {
                    int nr = current.row + DELTA_ROW[d];
                    int nc = current.col + DELTA_COL[d];
                    if (nr < 0||nr >= ROWS||nc < 0||nc >= COLS) continue;
                    if (grid->cells[nr][nc] == CELL_WALL) continue;
                    if (ctx->back_visited[nr][nc]) continue;
                    float new_cost = ctx->back_cost[current.row][current.col]
                                   + edge_cost(ctx->algo, grid->cells[nr][nc]);
                    if (new_cost >= ctx->back_cost[nr][nc]) continue;
                    ctx->back_cost[nr][nc] = new_cost;
                    Node nb = {nr, nc, current.row, current.col, new_cost, new_cost};
                    generic_heap_insert(&ctx->back_heap, &nb);
                }
            }
        }

        if (!generic_heap_size(&ctx->heap) && !generic_heap_size(&ctx->back_heap))
            ctx->done = true;
        goto finish;
    }

    //  BFS / DFS / Dijkstra / A* 
    {
        Node current;
        void *raw = NULL;

        if (ctx->algo == ALGO_BFS) {
            if (!generic_queue_size(&ctx->queue)) { ctx->done = true; goto finish; }
            generic_queue_dequeue(&ctx->queue, &raw);
            current = *(Node *)raw;
            node_destroy(raw);

        } else if (ctx->algo == ALGO_DFS) {
            if (!generic_stack_size(&ctx->stack)) { ctx->done = true; goto finish; }
            generic_stack_pop(&ctx->stack, &raw);
            current = *(Node *)raw;
            node_destroy(raw);
            if (ctx->visited[current.row][current.col]) return true;
            ctx->visited[current.row][current.col] = true;

        } else {
            if (!generic_heap_size(&ctx->heap)) { ctx->done = true; goto finish; }
            generic_heap_extract(&ctx->heap, &raw);
            current = *(Node *)raw;
            node_destroy(raw);
            if (ctx->visited[current.row][current.col] && current.cost >= ctx->cost[current.row][current.col])
                return true;
            ctx->visited[current.row][current.col] = true;
            ctx->cost[current.row][current.col] = current.cost;
        }

        ctx->parent_row[current.row][current.col] = current.parent_row;
        ctx->parent_col[current.row][current.col] = current.parent_col;
        ctx->result.visited++;

        CellType cell = grid->cells[current.row][current.col];
        if (cell == CELL_BONUS_BOMB)
            ctx->result.walls_opened += bomb_effect(grid, current.row, current.col);
        if (cell != CELL_START && cell != CELL_END)
            grid->vis[current.row][current.col] = VIS_VISITED;

        if (current.row == grid->endR && current.col == grid->endC) {
            ctx->done = ctx->found = true;
            trace_path(ctx);
            goto finish;
        }
        for (int d = 0; d < 4; d++) {
            int nr = current.row + DELTA_ROW[d];
            int nc = current.col + DELTA_COL[d];
            if (nr < 0||nr >= ROWS||nc < 0||nc >= COLS) continue;
            if (grid->cells[nr][nc] == CELL_WALL) continue;

            float new_cost = ctx->cost[current.row][current.col]
                        + edge_cost(ctx->algo, grid->cells[nr][nc]);

            if (ctx->algo != ALGO_BFS && ctx->algo != ALGO_DFS
                    && new_cost >= ctx->cost[nr][nc]) continue;

            if (ctx->visited[nr][nc]
                    && new_cost >= ctx->cost[nr][nc]) continue;

            ctx->cost[nr][nc] = new_cost;

            float new_score = new_cost;
            if (ctx->algo == ALGO_ASTAR)
                new_score += manhattan(nr, nc, grid->endR, grid->endC);

            Node nb = {nr, nc, current.row, current.col, new_cost, new_score};

            if (ctx->algo == ALGO_BFS) {
                if (!ctx->visited[nr][nc]) {
                    ctx->visited[nr][nc] = true;
                    generic_queue_enqueue(&ctx->queue, &nb);
                    if (grid->vis[nr][nc] == VIS_NONE) grid->vis[nr][nc] = VIS_FRONTIER;
                }
            } else if (ctx->algo == ALGO_DFS) {
                generic_stack_push(&ctx->stack, &nb);
                if (grid->vis[nr][nc] == VIS_NONE) grid->vis[nr][nc] = VIS_FRONTIER;
            } else {
                generic_heap_insert(&ctx->heap, &nb);
                if (grid->vis[nr][nc] == VIS_NONE) grid->vis[nr][nc] = VIS_FRONTIER;
            }
        }
    }

finish:
    if (ctx->done) {
        ctx->result.timeMs = get_time_ms() - ctx->start_time_ms;
        ctx->result.final_score =
            ctx->result.pathLen   * 5 +
            ctx->result.penalties * 3 -
            ctx->result.bonuses   * 2 +
            (int)(ctx->result.timeMs * 0.5);
    }
    return !ctx->done;
}

// ----------------------------------------------------------------------------------
// Cleanup
// -----------------------------------------------------------------------
void search_destroy(SearchCtx *ctx) {
    if (!ctx) return;
    if (ctx->algo == ALGO_BFS)
        generic_queue_destroy(&ctx->queue);
    else if (ctx->algo == ALGO_DFS)
        generic_stack_destroy(&ctx->stack);
    else {
        generic_heap_destroy(&ctx->heap);
        if (ctx->algo == ALGO_BIDIR_DIJKSTRA)
            generic_heap_destroy(&ctx->back_heap);
    }
    free(ctx);
}

SearchResult search_get_result(SearchCtx *ctx) { return ctx->result; }
