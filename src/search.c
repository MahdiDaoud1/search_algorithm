#include "maze.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════════════════
   Generic node for BFS/DFS/Dijkstra/A*
   ══════════════════════════════════════════════════════════════════════════ */
typedef struct Node {
    int r, c;
    int parent_r, parent_c;
    float g;      // cost so far  (Dijkstra / A*)
    float f;      // total  score (A* only)
} Node;

/* ── priority queue (min-heap on f) ─────────────────────────────────────── */
typedef struct {
    Node *data;
    int   size, cap;
} PQueue;

static PQueue *pq_create(void) {
    PQueue *q = malloc(sizeof(PQueue));
    q->cap  = 1024; q->size = 0;
    q->data = malloc(q->cap * sizeof(Node));
    return q;
}
static void pq_push(PQueue *q, Node n) {
    if (q->size >= q->cap) { q->cap *= 2; q->data = realloc(q->data, q->cap*sizeof(Node)); }
    q->data[q->size++] = n;
    // bubble up
    int i = q->size-1;
    while (i > 0) {
        int p = (i-1)/2;
        if (q->data[p].f <= q->data[i].f) break;
        Node tmp = q->data[p]; q->data[p] = q->data[i]; q->data[i] = tmp;
        i = p;
    }
}
static Node pq_pop(PQueue *q) {
    Node ret = q->data[0];
    q->data[0] = q->data[--q->size];
    int i = 0;
    for (;;) {
        int l = 2*i+1, r = 2*i+2, s = i;
        if (l < q->size && q->data[l].f < q->data[s].f) s = l;
        if (r < q->size && q->data[r].f < q->data[s].f) s = r;
        if (s == i) break;
        Node tmp = q->data[s]; q->data[s] = q->data[i]; q->data[i] = tmp;
        i = s;
    }
    return ret;
}
static void pq_free(PQueue *q) { free(q->data); free(q); }

/* ── simple stack for DFS ─────────────────────────────────────────────────── */
typedef struct { Node *data; int top, cap; } Stack;
static Stack *stack_create(void) {
    Stack *s = malloc(sizeof(Stack)); s->cap=512; s->top=0;
    s->data = malloc(s->cap*sizeof(Node)); return s;
}
static void stack_push(Stack *s, Node n) {
    if (s->top >= s->cap) { s->cap*=2; s->data=realloc(s->data,s->cap*sizeof(Node)); }
    s->data[s->top++] = n;
}
static Node stack_pop(Stack *s) { return s->data[--s->top]; }
static void stack_free(Stack *s) { free(s->data); free(s); }

/* ── simple FIFO queue for BFS ────────────────────────────────────────────── */
typedef struct { Node *data; int head, tail, cap; } Queue;
static Queue *queue_create(void) {
    Queue *q = malloc(sizeof(Queue)); q->cap=2048; q->head=q->tail=0;
    q->data = malloc(q->cap*sizeof(Node)); return q;
}
static void queue_push(Queue *q, Node n) {
    if ((q->tail+1)%q->cap == q->head) {
        q->cap*=2; q->data=realloc(q->data,q->cap*sizeof(Node));
    }
    q->data[q->tail] = n; q->tail=(q->tail+1)%q->cap;
}
static Node queue_pop(Queue *q) { Node n=q->data[q->head]; q->head=(q->head+1)%q->cap; return n; }
static int  queue_empty(Queue *q) { return q->head==q->tail; }
static void queue_free(Queue *q) { free(q->data); free(q); }

/* ══════════════════════════════════════════════════════════════════════════
   SearchCtx
   ══════════════════════════════════════════════════════════════════════════ */
struct SearchCtx {
    SearchAlgo algo;
    Grid      *grid;
    bool       done;
    bool       found;

    /* parent map for path reconstruction */
    int par_r[ROWS][COLS];
    int par_c[ROWS][COLS];
    bool visited_map[ROWS][COLS];
    float cost[ROWS][COLS];   // for Dijkstra / A*

    /* containers */
    Queue  *bfs_q;
    Stack  *dfs_s;
    PQueue *pq;

    SearchResult result;
    struct timespec t0;
};

static const int DR[4] = {-1,1,0,0};
static const int DC[4] = {0,0,-1,1};

static float heuristic(int r, int c, int er, int ec) {
    return (float)(abs(r-er) + abs(c-ec));   // Manhattan
}

SearchCtx *search_create(SearchAlgo algo, Grid *g) {
    SearchCtx *ctx = calloc(1, sizeof(SearchCtx));
    ctx->algo  = algo;
    ctx->grid  = g;
    ctx->done  = false;
    ctx->found = false;
    ctx->result.pathLen = 0;
    ctx->result.visited = 0;
    ctx->result.timeMs  = 0;

    for (int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) {
        ctx->par_r[r][c] = -1;
        ctx->par_c[r][c] = -1;
        ctx->cost[r][c]  = 1e30f;
    }
    ctx->cost[g->startR][g->startC] = 0;

    Node start = { g->startR, g->startC, -1, -1, 0, 0 };
    start.f = heuristic(g->startR,g->startC,g->endR,g->endC);

    switch (algo) {
        case ALGO_BFS:
            ctx->bfs_q = queue_create();
            queue_push(ctx->bfs_q, start);
            ctx->visited_map[g->startR][g->startC] = true;
            break;
        case ALGO_DFS:
            ctx->dfs_s = stack_create();
            stack_push(ctx->dfs_s, start);
            break;
        case ALGO_DIJKSTRA:
        case ALGO_ASTAR:
            ctx->pq = pq_create();
            pq_push(ctx->pq, start);
            break;
        default: break;
    }

    clock_gettime(CLOCK_MONOTONIC, &ctx->t0);
    return ctx;
}

static void trace_path(SearchCtx *ctx) {
    Grid *g = ctx->grid;
    int r = g->endR, c = g->endC;
    int len = 0;
    while (!(r == g->startR && c == g->startC)) {
        if (r < 0) break;
        g->vis[r][c] = STATE_PATH;
        int pr = ctx->par_r[r][c];
        int pc = ctx->par_c[r][c];
        r = pr; c = pc;
        len++;
    }
    g->vis[g->startR][g->startC] = STATE_START;
    g->vis[g->endR  ][g->endC  ] = STATE_END;
    ctx->result.pathLen = len;
}

bool search_step(SearchCtx *ctx) {
    if (ctx->done) return false;
    Grid *g = ctx->grid;

    /* process one cell per frame for visible animation */
    int batch = 1;

    for (int b = 0; b < batch && !ctx->done; b++) {
        int r = -1, c = -1, pr = -1, pc = -1;
        bool process = false;

        if (ctx->algo == ALGO_BFS) {
            if (queue_empty(ctx->bfs_q)) { ctx->done=true; break; }
            Node n = queue_pop(ctx->bfs_q);
            r=n.r; c=n.c; pr=n.parent_r; pc=n.parent_c;
            process=true;
        } else if (ctx->algo == ALGO_DFS) {
            if (ctx->dfs_s->top == 0) { ctx->done=true; break; }
            Node n = stack_pop(ctx->dfs_s);
            if (ctx->visited_map[n.r][n.c]) continue;
            r=n.r; c=n.c; pr=n.parent_r; pc=n.parent_c;
            ctx->visited_map[r][c] = true;
            process=true;
        } else { // Dijkstra / A*
            if (ctx->pq->size == 0) { ctx->done=true; break; }
            Node n = pq_pop(ctx->pq);
            if (ctx->visited_map[n.r][n.c]) continue;
            r=n.r; c=n.c; pr=n.parent_r; pc=n.parent_c;
            ctx->visited_map[r][c] = true;
            process=true;
        }

        if (!process) continue;

        // record parent
        ctx->par_r[r][c] = pr;
        ctx->par_c[r][c] = pc;

        if (g->vis[r][c] != STATE_START && g->vis[r][c] != STATE_END)
            g->vis[r][c] = STATE_VISITED;
        ctx->result.visited++;

        // goal check
        if (r == g->endR && c == g->endC) {
            ctx->done  = true;
            ctx->found = true;
            trace_path(ctx);
            break;
        }

        // expand neighbours
        for (int d = 0; d < 4; d++) {
            int nr = r+DR[d], nc = c+DC[d];
            if (nr<0||nr>=ROWS||nc<0||nc>=COLS) continue;
            if (g->cells[nr][nc] == WALL) continue;
            if (ctx->visited_map[nr][nc]) continue;

            float ng = ctx->cost[r][c] + 1.0f;
            if (ng >= ctx->cost[nr][nc] && ctx->algo != ALGO_BFS && ctx->algo != ALGO_DFS)
                continue;
            ctx->cost[nr][nc] = ng;

            Node nn = { nr, nc, r, c, ng, ng };
            if (ctx->algo == ALGO_ASTAR)
                nn.f = ng + heuristic(nr,nc,g->endR,g->endC);

            if (ctx->algo == ALGO_BFS) {
                if (!ctx->visited_map[nr][nc]) {
                    ctx->visited_map[nr][nc] = true;
                    queue_push(ctx->bfs_q, nn);
                    if (g->vis[nr][nc]!=STATE_END) g->vis[nr][nc]=STATE_FRONTIER;
                }
            } else if (ctx->algo == ALGO_DFS) {
                stack_push(ctx->dfs_s, nn);
                if (g->vis[nr][nc]!=STATE_END) g->vis[nr][nc]=STATE_FRONTIER;
            } else {
                pq_push(ctx->pq, nn);
                if (g->vis[nr][nc]!=STATE_END) g->vis[nr][nc]=STATE_FRONTIER;
            }
        }
    }

    if (ctx->done) {
        struct timespec t1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        ctx->result.timeMs = (t1.tv_sec - ctx->t0.tv_sec)*1000.0
                           + (t1.tv_nsec - ctx->t0.tv_nsec)/1e6;
    }
    return !ctx->done;
}

void search_destroy(SearchCtx *ctx) {
    if (!ctx) return;
    if (ctx->bfs_q) queue_free(ctx->bfs_q);
    if (ctx->dfs_s) stack_free(ctx->dfs_s);
    if (ctx->pq)    pq_free(ctx->pq);
    free(ctx);
}

SearchResult search_get_result(SearchCtx *ctx) {
    return ctx->result;
}
