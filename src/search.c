#include "maze.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ── Portable timing ───────────────────────────────────────────────────────────
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

// ═══════════════════════════════════════════════════════════════════════════════
// Node — carries one cell's position, its parent, and its cost
// ═══════════════════════════════════════════════════════════════════════════════
// Each node stores its PARENT (not children) because we reconstruct the path
// by walking backward: goal → parent → parent → ... → start.
// If we stored children we would need to search the whole tree to find the path.
typedef struct Node {
    int   row, col;
    int   parent_row, parent_col;  // where we came from (-1 if start)
    float cost;                    // g: real cost from start to here
    float score;                   // f: cost + heuristic (A* only, = cost for Dijkstra)
} Node;

// ═══════════════════════════════════════════════════════════════════════════════
// Queue — FIFO, used by BFS
// Implemented as a singly-linked list.
// Push adds to the tail, pop removes from the head — both O(1), no reallocation.
// ═══════════════════════════════════════════════════════════════════════════════
typedef struct QueueNode {
    Node            data;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *head;   // front — pop from here
    QueueNode *tail;   // back  — push to here
    int        size;
} Queue;

static Queue *queue_create(void) {
    Queue *q = malloc(sizeof(Queue));
    q->head = q->tail = NULL;
    q->size = 0;
    return q;
}

static void queue_push(Queue *q, Node data) {
    QueueNode *node = malloc(sizeof(QueueNode));
    node->data = data;
    node->next = NULL;
    if (q->tail) q->tail->next = node;
    else         q->head       = node;  // first element: head and tail both point to it
    q->tail = node;
    q->size++;
}

static Node queue_pop(Queue *q) {
    QueueNode *front = q->head;
    Node data = front->data;
    q->head = front->next;
    if (!q->head) q->tail = NULL;  // queue is now empty
    free(front);
    q->size--;
    return data;
}

static int queue_is_empty(Queue *q) { return q->head == NULL; }

static void queue_destroy(Queue *q) {
    while (!queue_is_empty(q)) queue_pop(q);
    free(q);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Stack — LIFO, used by DFS
// Implemented as a singly-linked list.
// Push and pop both operate on the head — O(1), no reallocation.
// ═══════════════════════════════════════════════════════════════════════════════
typedef struct StackNode {
    Node             data;
    struct StackNode *next;
} StackNode;

typedef struct {
    StackNode *top;
    int        size;
} Stack;

static Stack *stack_create(void) {
    Stack *s = malloc(sizeof(Stack));
    s->top  = NULL;
    s->size = 0;
    return s;
}

static void stack_push(Stack *s, Node data) {
    StackNode *node = malloc(sizeof(StackNode));
    node->data = data;
    node->next = s->top;  // new node points to old top
    s->top = node;        // new node becomes the top
    s->size++;
}

static Node stack_pop(Stack *s) {
    StackNode *top = s->top;
    Node data = top->data;
    s->top = top->next;
    free(top);
    s->size--;
    return data;
}

static int stack_is_empty(Stack *s) { return s->top == NULL; }

static void stack_destroy(Stack *s) {
    while (!stack_is_empty(s)) stack_pop(s);
    free(s);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Min-Heap — priority queue ordered by score (lowest first), used by Dijkstra/A*
//
// WHY an array and not a linked list:
// A binary heap relies on the index relationship parent=(i-1)/2, children=2i+1/2i+2.
// These formulas require O(1) access to any index — impossible with a linked list.
// A linked-list heap would need O(n) traversal to find the parent, making
// push O(n) instead of O(log n). So the array is not laziness — it's required.
//
// WHY no realloc:
// The maximum number of cells ever pushed into the heap is bounded by ROWS*COLS.
// We allocate exactly that once and never resize.
// ═══════════════════════════════════════════════════════════════════════════════
#define HEAP_MAX_SIZE (ROWS * COLS)

typedef struct {
    Node data[HEAP_MAX_SIZE];
    int  size;
} MinHeap;

static MinHeap *heap_create(void) {
    MinHeap *h = malloc(sizeof(MinHeap));
    h->size = 0;
    return h;
}

static void heap_push(MinHeap *h, Node node) {
    int i = h->size++;
    h->data[i] = node;
    // bubble up: swap with parent while parent has higher score
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->data[parent].score <= h->data[i].score) break;
        Node tmp        = h->data[parent];
        h->data[parent] = h->data[i];
        h->data[i]      = tmp;
        i = parent;
    }
}

static Node heap_pop(MinHeap *h) {
    Node minimum   = h->data[0];
    h->data[0]     = h->data[--h->size];  // move last element to root
    // sift down: swap with the smaller child until heap property restored
    int i = 0;
    for (;;) {
        int left     = 2 * i + 1;
        int right    = 2 * i + 2;
        int smallest = i;
        if (left  < h->size && h->data[left ].score < h->data[smallest].score) smallest = left;
        if (right < h->size && h->data[right].score < h->data[smallest].score) smallest = right;
        if (smallest == i) break;
        Node tmp          = h->data[smallest];
        h->data[smallest] = h->data[i];
        h->data[i]        = tmp;
        i = smallest;
    }
    return minimum;
}

static void heap_destroy(MinHeap *h) { free(h); }

// ═══════════════════════════════════════════════════════════════════════════════
// SearchCtx — holds the entire state of one in-progress search
// ═══════════════════════════════════════════════════════════════════════════════
struct SearchCtx {
    SearchAlgo algo;
    Grid      *grid;
    bool       done, found;

    // parent map: for each visited cell, where did we come from
    int   parent_row[ROWS][COLS];
    int   parent_col[ROWS][COLS];
    bool  visited[ROWS][COLS];
    float cost[ROWS][COLS];

    // backward search (bidirectional Dijkstra only)
    int   back_parent_row[ROWS][COLS];
    int   back_parent_col[ROWS][COLS];
    bool  back_visited[ROWS][COLS];
    float back_cost[ROWS][COLS];
    int   meet_row, meet_col;

    // containers — only one set is used depending on algo
    Queue   *queue;       // BFS
    Stack   *stack;       // DFS
    MinHeap *heap;        // Dijkstra, A*
    MinHeap *back_heap;   // bidirectional Dijkstra (backward search)

    SearchResult result;
    double       start_time_ms;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════════
static const int DELTA_ROW[4] = {-1, 1,  0, 0};
static const int DELTA_COL[4] = { 0, 0, -1, 1};

static float manhattan(int row, int col, int goal_row, int goal_col) {
    return (float)(abs(row - goal_row) + abs(col - goal_col));
}

// BFS and DFS treat all cells as cost 1 — they are intentionally trap-blind.
// Dijkstra and A* add a penalty weight so they can route around heavy traps.
static float edge_cost(SearchAlgo algo, CellType cell_type) {
    if (algo == ALGO_BFS || algo == ALGO_DFS) return 1.0f;
    int penalty = cell_penalty(cell_type);
    return 1.0f + (penalty > 0 ? (float)penalty / 15.0f : 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Bomb effect — opens all 8 adjacent wall cells
// ═══════════════════════════════════════════════════════════════════════════════
static int bomb_effect(Grid *grid, SearchCtx *ctx, int row, int col) {
    int dr[] = {-1,-1,-1, 0, 0, 1, 1, 1};
    int dc[] = {-1, 0, 1,-1, 1,-1, 0, 1};
    int opened = 0;

    for (int d = 0; d < 8; d++) {
        int wr = row + dr[d];
        int wc = col + dc[d];

        if (wr <= 0 || wr >= ROWS-1 || wc <= 0 || wc >= COLS-1) continue;
        if (grid->cells[wr][wc] != CELL_WALL) continue;

        grid->cells[wr][wc] = CELL_OPEN;
        grid->vis[wr][wc]   = VIS_EXPLODED;

        if (grid->crater_count < MAX_CRATERS) {
            grid->craters[grid->crater_count].r = wr;
            grid->craters[grid->crater_count].c = wc;
            grid->crater_count++;
        }

        // for weighted algorithms: reset cost so the new cell gets re-evaluated
        if (ctx->algo == ALGO_DIJKSTRA || ctx->algo == ALGO_ASTAR ||
            ctx->algo == ALGO_BIDIR_DIJKSTRA) {
            int fr = row + dr[d]*2;
            int fc = col + dc[d]*2;
            if (fr > 0 && fr < ROWS-1 && fc > 0 && fc < COLS-1 && !ctx->visited[fr][fc])
                ctx->cost[fr][fc] = 1e30f;
        }
        opened++;
    }
    return opened;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Path reconstruction — walk parent chain backward from goal to start
// ═══════════════════════════════════════════════════════════════════════════════
static void trace_path(SearchCtx *ctx) {
    Grid *grid = ctx->grid;
    int row = grid->endR, col = grid->endC;
    int length = 0, penalties = 0, bonuses = 0,
        traps_hit = 0, bonuses_hit = 0, walls_opened = 0;
    ctx->result.event_count = 0;

    while (!(row == grid->startR && col == grid->startC)) {
        if (row < 0) break;
        CellType cell = grid->cells[row][col];

        if (cell != CELL_END && cell != CELL_START) {
            int delta = cell_penalty(cell);
            if (delta > 0) { penalties += delta; traps_hit++; }
            else if (delta < 0) { bonuses += (-delta); bonuses_hit++; }

            if (cell == CELL_BONUS_BOMB) {
                // count how many craters this bomb opened
                int dr[] = {-1,-1,-1, 0, 0, 1, 1, 1};
                int dc[] = {-1, 0, 1,-1, 1,-1, 0, 1};
                int bomb_walls = 0;
                for (int d = 0; d < 8; d++) {
                    int wr = row + dr[d], wc = col + dc[d];
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
        row = pr;
        col = pc;
        length++;
    }

    grid->vis[grid->startR][grid->startC] = VIS_NONE;
    grid->vis[grid->endR  ][grid->endC  ] = VIS_NONE;

    ctx->result.pathLen     = length;
    ctx->result.penalties   = penalties;
    ctx->result.bonuses     = bonuses;
    ctx->result.traps_hit   = traps_hit;
    ctx->result.bonuses_hit = bonuses_hit;
    ctx->result.walls_opened= walls_opened;
    // final_score computed after timeMs is recorded in search_step
}

static void trace_bidir_path(SearchCtx *ctx) {
    Grid *grid = ctx->grid;
    int mr = ctx->meet_row, mc = ctx->meet_col;
    int length = 0, penalties = 0, bonuses = 0,
        traps_hit = 0, bonuses_hit = 0, walls_opened = 0;
    ctx->result.event_count = 0;

    // forward half: meeting point → start
    int row = mr, col = mc;
    while (!(row == grid->startR && col == grid->startC)) {
        if (row < 0) break;
        CellType cell = grid->cells[row][col];
        if (cell != CELL_START && cell != CELL_END) {
            int delta = cell_penalty(cell);
            if (delta > 0) { penalties += delta; traps_hit++; }
            else if (delta < 0) { bonuses += (-delta); bonuses_hit++; }
            if (cell_is_special(cell) && ctx->result.event_count < MAX_EVENTS) {
                TileEvent ev = {cell, row, col, delta};
                ctx->result.events[ctx->result.event_count++] = ev;
            }
        }
        grid->vis[row][col] = VIS_PATH;
        int pr = ctx->parent_row[row][col];
        int pc = ctx->parent_col[row][col];
        row = pr; col = pc; length++;
    }

    // backward half: meeting point → end
    row = mr; col = mc;
    while (!(row == grid->endR && col == grid->endC)) {
        if (row < 0) break;
        CellType cell = grid->cells[row][col];
        if (cell != CELL_START && cell != CELL_END) {
            int delta = cell_penalty(cell);
            if (delta > 0) { penalties += delta; traps_hit++; }
            else if (delta < 0) { bonuses += (-delta); bonuses_hit++; }
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

// ═══════════════════════════════════════════════════════════════════════════════
// search_create — allocate and initialise a search context
// ═══════════════════════════════════════════════════════════════════════════════
SearchCtx *search_create(SearchAlgo algo, Grid *grid) {
    SearchCtx *ctx = calloc(1, sizeof(SearchCtx));
    ctx->algo      = algo;
    ctx->grid      = grid;
    ctx->meet_row  = ctx->meet_col = -1;

    // initialise all costs to infinity, parents to -1
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            ctx->parent_row[r][c] = ctx->parent_col[r][c] = -1;
            ctx->back_parent_row[r][c] = ctx->back_parent_col[r][c] = -1;
            ctx->cost[r][c] = ctx->back_cost[r][c] = 1e30f;
        }
    ctx->cost[grid->startR][grid->startC] = 0.0f;

    Node start = {
        .row        = grid->startR,
        .col        = grid->startC,
        .parent_row = -1,
        .parent_col = -1,
        .cost       = 0.0f,
        .score      = manhattan(grid->startR, grid->startC, grid->endR, grid->endC)
    };

    switch (algo) {
        case ALGO_BFS:
            ctx->queue = queue_create();
            queue_push(ctx->queue, start);
            ctx->visited[grid->startR][grid->startC] = true;
            break;
        case ALGO_DFS:
            ctx->stack = stack_create();
            stack_push(ctx->stack, start);
            break;
        case ALGO_BIDIR_DIJKSTRA:
            ctx->heap = heap_create();
            heap_push(ctx->heap, start);
            ctx->back_cost[grid->endR][grid->endC] = 0.0f;
            Node back_start = {
                .row=grid->endR, .col=grid->endC,
                .parent_row=-1,  .parent_col=-1,
                .cost=0.0f,      .score=0.0f
            };
            ctx->back_heap = heap_create();
            heap_push(ctx->back_heap, back_start);
            break;
        default:  // Dijkstra and A*
            ctx->heap = heap_create();
            heap_push(ctx->heap, start);
            break;
    }

    ctx->start_time_ms = get_time_ms();
    return ctx;
}

// ═══════════════════════════════════════════════════════════════════════════════
// search_step — advance the search by exactly one node, return false when done
// ═══════════════════════════════════════════════════════════════════════════════
bool search_step(SearchCtx *ctx) {
    if (ctx->done) return false;
    Grid *grid = ctx->grid;

    // ── Bidirectional Dijkstra ────────────────────────────────────────────────
    if (ctx->algo == ALGO_BIDIR_DIJKSTRA) {

        // advance forward frontier
        if (ctx->heap->size > 0) {
            Node current = heap_pop(ctx->heap);
            if (!ctx->visited[current.row][current.col]) {
                ctx->visited[current.row][current.col] = true;
                ctx->parent_row[current.row][current.col] = current.parent_row;
                ctx->parent_col[current.row][current.col] = current.parent_col;
                ctx->result.visited++;

                if (grid->cells[current.row][current.col] == CELL_BONUS_BOMB)
                    ctx->result.walls_opened += bomb_effect(grid, ctx, current.row, current.col);

                if (grid->vis[current.row][current.col] == VIS_NONE)
                    grid->vis[current.row][current.col] = VIS_VISITED;

                if (ctx->back_visited[current.row][current.col]) {
                    ctx->meet_row = current.row;
                    ctx->meet_col = current.col;
                    ctx->done = ctx->found = true;
                    trace_bidir_path(ctx);
                    goto finish;
                }

                for (int d = 0; d < 4; d++) {
                    int nr = current.row + DELTA_ROW[d];
                    int nc = current.col + DELTA_COL[d];
                    if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
                    if (grid->cells[nr][nc] == CELL_WALL) continue;
                    if (ctx->visited[nr][nc]) continue;
                    float new_cost = ctx->cost[current.row][current.col]
                                   + edge_cost(ctx->algo, grid->cells[nr][nc]);
                    if (new_cost >= ctx->cost[nr][nc]) continue;
                    ctx->cost[nr][nc] = new_cost;
                    Node neighbour = {nr, nc, current.row, current.col, new_cost, new_cost};
                    heap_push(ctx->heap, neighbour);
                    if (grid->vis[nr][nc] == VIS_NONE) grid->vis[nr][nc] = VIS_FRONTIER;
                }
            }
        }

        // advance backward frontier
        if (ctx->back_heap->size > 0) {
            Node current = heap_pop(ctx->back_heap);
            if (!ctx->back_visited[current.row][current.col]) {
                ctx->back_visited[current.row][current.col] = true;
                ctx->back_parent_row[current.row][current.col] = current.parent_row;
                ctx->back_parent_col[current.row][current.col] = current.parent_col;
                ctx->result.visited++;

                if (grid->vis[current.row][current.col] == VIS_NONE)
                    grid->vis[current.row][current.col] = VIS_FRONTIER;

                if (ctx->visited[current.row][current.col]) {
                    ctx->meet_row = current.row;
                    ctx->meet_col = current.col;
                    ctx->done = ctx->found = true;
                    trace_bidir_path(ctx);
                    goto finish;
                }

                for (int d = 0; d < 4; d++) {
                    int nr = current.row + DELTA_ROW[d];
                    int nc = current.col + DELTA_COL[d];
                    if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
                    if (grid->cells[nr][nc] == CELL_WALL) continue;
                    if (ctx->back_visited[nr][nc]) continue;
                    float new_cost = ctx->back_cost[current.row][current.col]
                                   + edge_cost(ctx->algo, grid->cells[nr][nc]);
                    if (new_cost >= ctx->back_cost[nr][nc]) continue;
                    ctx->back_cost[nr][nc] = new_cost;
                    Node neighbour = {nr, nc, current.row, current.col, new_cost, new_cost};
                    heap_push(ctx->back_heap, neighbour);
                }
            }
        }

        if (!ctx->heap->size && !ctx->back_heap->size) ctx->done = true;
        goto finish;
    }

    // ── BFS / DFS / Dijkstra / A* ─────────────────────────────────────────────
    {
        int row, col, parent_row, parent_col;

        if (ctx->algo == ALGO_BFS) {
            if (queue_is_empty(ctx->queue)) { ctx->done = true; goto finish; }
            Node current = queue_pop(ctx->queue);
            row = current.row; col = current.col;
            parent_row = current.parent_row; parent_col = current.parent_col;

        } else if (ctx->algo == ALGO_DFS) {
            if (stack_is_empty(ctx->stack)) { ctx->done = true; goto finish; }
            Node current = stack_pop(ctx->stack);
            if (ctx->visited[current.row][current.col]) return true;
            row = current.row; col = current.col;
            parent_row = current.parent_row; parent_col = current.parent_col;
            ctx->visited[row][col] = true;

        } else {  // Dijkstra / A*
            if (!ctx->heap->size) { ctx->done = true; goto finish; }
            Node current = heap_pop(ctx->heap);
            if (ctx->visited[current.row][current.col]) return true;
            row = current.row; col = current.col;
            parent_row = current.parent_row; parent_col = current.parent_col;
            ctx->visited[row][col] = true;
        }

        ctx->parent_row[row][col] = parent_row;
        ctx->parent_col[row][col] = parent_col;
        ctx->result.visited++;

        CellType cell = grid->cells[row][col];

        // bomb fires before expansion so opened walls are discovered this step
        if (cell == CELL_BONUS_BOMB)
            ctx->result.walls_opened += bomb_effect(grid, ctx, row, col);

        if (cell != CELL_START && cell != CELL_END)
            grid->vis[row][col] = VIS_VISITED;

        if (row == grid->endR && col == grid->endC) {
            ctx->done = ctx->found = true;
            trace_path(ctx);
            goto finish;
        }

        // expand all 4 neighbours
        for (int d = 0; d < 4; d++) {
            int nr = row + DELTA_ROW[d];
            int nc = col + DELTA_COL[d];
            if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
            if (grid->cells[nr][nc] == CELL_WALL) continue;
            if (ctx->visited[nr][nc]) continue;

            float new_cost = ctx->cost[row][col]
                           + edge_cost(ctx->algo, grid->cells[nr][nc]);

            // skip if this route is not an improvement (weighted algorithms only)
            if (ctx->algo != ALGO_BFS && ctx->algo != ALGO_DFS
                    && new_cost >= ctx->cost[nr][nc]) continue;

            ctx->cost[nr][nc] = new_cost;

            float new_score = new_cost;
            if (ctx->algo == ALGO_ASTAR)
                new_score += manhattan(nr, nc, grid->endR, grid->endC);

            Node neighbour = {nr, nc, row, col, new_cost, new_score};

            if (ctx->algo == ALGO_BFS) {
                if (!ctx->visited[nr][nc]) {
                    ctx->visited[nr][nc] = true;
                    queue_push(ctx->queue, neighbour);
                    if (grid->vis[nr][nc] == VIS_NONE) grid->vis[nr][nc] = VIS_FRONTIER;
                }
            } else if (ctx->algo == ALGO_DFS) {
                stack_push(ctx->stack, neighbour);
                if (grid->vis[nr][nc] == VIS_NONE) grid->vis[nr][nc] = VIS_FRONTIER;
            } else {
                heap_push(ctx->heap, neighbour);
                if (grid->vis[nr][nc] == VIS_NONE) grid->vis[nr][nc] = VIS_FRONTIER;
            }
        }
    }

finish:
    if (ctx->done) {
        ctx->result.timeMs = get_time_ms() - ctx->start_time_ms;
        ctx->result.final_score =
            ctx->result.pathLen    * 5 +
            ctx->result.penalties  * 3 -
            ctx->result.bonuses    * 2 +
            (int)(ctx->result.timeMs * 0.5);
    }
    return !ctx->done;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cleanup
// ═══════════════════════════════════════════════════════════════════════════════
void search_destroy(SearchCtx *ctx) {
    if (!ctx) return;
    if (ctx->queue)     queue_destroy(ctx->queue);
    if (ctx->stack)     stack_destroy(ctx->stack);
    if (ctx->heap)      heap_destroy(ctx->heap);
    if (ctx->back_heap) heap_destroy(ctx->back_heap);
    free(ctx);
}

SearchResult search_get_result(SearchCtx *ctx) { return ctx->result; }
