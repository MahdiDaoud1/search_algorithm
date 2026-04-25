#include "maze.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ── Node ──────────────────────────────────────────────────────────────────────
typedef struct Node {
    int   r, c, parent_r, parent_c;
    float g, f;
} Node;

// ── Min-heap ──────────────────────────────────────────────────────────────────
typedef struct { Node *data; int size, cap; } PQueue;
static PQueue *pq_new(void) {
    PQueue *q=malloc(sizeof(PQueue)); q->cap=1024; q->size=0;
    q->data=malloc(q->cap*sizeof(Node)); return q;
}
static void pq_push(PQueue *q, Node n) {
    if (q->size>=q->cap){q->cap*=2;q->data=realloc(q->data,q->cap*sizeof(Node));}
    q->data[q->size++]=n;
    for(int i=q->size-1;i>0;){int p=(i-1)/2;if(q->data[p].f<=q->data[i].f)break;
        Node t=q->data[p];q->data[p]=q->data[i];q->data[i]=t;i=p;}
}
static Node pq_pop(PQueue *q) {
    Node r=q->data[0]; q->data[0]=q->data[--q->size];
    for(int i=0;;){int l=2*i+1,rt=2*i+2,s=i;
        if(l<q->size&&q->data[l].f<q->data[s].f)s=l;
        if(rt<q->size&&q->data[rt].f<q->data[s].f)s=rt;
        if(s==i)break;
        Node t=q->data[s];q->data[s]=q->data[i];q->data[i]=t;i=s;
    }
    return r;
}
static void pq_free(PQueue *q){free(q->data);free(q);}

// ── Stack ─────────────────────────────────────────────────────────────────────
typedef struct{Node *data;int top,cap;}Stack;
static Stack *stk_new(void){Stack *s=malloc(sizeof(Stack));s->cap=512;s->top=0;
    s->data=malloc(s->cap*sizeof(Node));return s;}
static void stk_push(Stack *s,Node n){
    if(s->top>=s->cap){s->cap*=2;s->data=realloc(s->data,s->cap*sizeof(Node));}
    s->data[s->top++]=n;}
static Node stk_pop(Stack *s){return s->data[--s->top];}
static void stk_free(Stack *s){free(s->data);free(s);}

// ── Queue ─────────────────────────────────────────────────────────────────────
typedef struct{Node *data;int head,tail,cap;}Queue;
static Queue *que_new(void){Queue *q=malloc(sizeof(Queue));q->cap=2048;q->head=q->tail=0;
    q->data=malloc(q->cap*sizeof(Node));return q;}
static void que_push(Queue *q,Node n){
    if((q->tail+1)%q->cap==q->head){q->cap*=2;q->data=realloc(q->data,q->cap*sizeof(Node));}
    q->data[q->tail]=n;q->tail=(q->tail+1)%q->cap;}
static Node que_pop(Queue *q){Node n=q->data[q->head];q->head=(q->head+1)%q->cap;return n;}
static int  que_empty(Queue *q){return q->head==q->tail;}
static void que_free(Queue *q){free(q->data);free(q);}

// ── SearchCtx ─────────────────────────────────────────────────────────────────
struct SearchCtx {
    SearchAlgo algo;
    Grid      *grid;
    bool       done, found;
    int        par_r[ROWS][COLS];
    int        par_c[ROWS][COLS];
    bool       visited[ROWS][COLS];
    float      cost[ROWS][COLS];
    Queue     *bfs_q;
    Stack     *dfs_s;
    PQueue    *pq;
    SearchResult result;
    struct timespec t0;
};

static const int DR[4]={-1,1,0,0};
static const int DC[4]={0,0,-1,1};

static float heuristic(int r,int c,int er,int ec){
    return (float)(abs(r-er)+abs(c-ec));
}

/*
 * Edge cost for weighted algorithms (Dijkstra, A*).
 * Trap tiles raise the cost so the algorithm prefers routing around them.
 * BFS and DFS always use cost 1 — they are trap-blind, which is the point.
 *
 * Scaling: penalty/15.0 so a spike (+80) costs ~6.3 extra.
 * That means Dijkstra/A* will detour if the detour saves ~7 steps.
 */
static float edge_cost(SearchAlgo algo, CellType ct) {
    if (algo==ALGO_BFS||algo==ALGO_DFS) return 1.0f;
    int p = cell_penalty(ct);
    return 1.0f + (p > 0 ? (float)p / 15.0f : 0.0f);
}

/*
 * bomb_effect: opens one wall cell that bridges two open areas.
 * Must create a genuine shortcut — a wall at (wr,wc) that has open cells
 * on BOTH sides at (r,c) and (fr,fc). Simply opening a dead-end wall
 * would just add a stub passage going nowhere useful.
 *
 * The opened cell is set to CELL_OPEN in cells[][] immediately so the
 * calling algorithm's expansion loop (which runs right after this) will
 * discover it — the wall check in expand uses cells[][] live.
 */
static int bomb_effect(Grid *g, SearchCtx *ctx, int r, int c) {
    int dr[]={-1,1,0,0}, dc[]={0,0,-1,1};
    int opened = 0;

    for (int d=0;d<4;d++) {
        int wr=r+dr[d], wc=c+dc[d];
        int fr=r+dr[d]*2, fc=c+dc[d]*2;

        if (wr<=0||wr>=ROWS-1||wc<=0||wc>=COLS-1) continue;
        if (fr<=0||fr>=ROWS-1||fc<=0||fc>=COLS-1) continue;
        if (g->cells[wr][wc]!=CELL_WALL) continue;
        if (g->cells[fr][fc]==CELL_WALL)  continue;

        g->cells[wr][wc] = CELL_OPEN;
        g->vis[wr][wc]   = VIS_EXPLODED;
        if (g->crater_count < MAX_CRATERS) {
            g->craters[g->crater_count].r = wr;
            g->craters[g->crater_count].c = wc;
            g->crater_count++;
        }

        if (ctx->algo==ALGO_DIJKSTRA||ctx->algo==ALGO_ASTAR) {
            if (!ctx->visited[fr][fc])
                ctx->cost[fr][fc] = 1e30f;
        }

        opened++;
        /* no return — continue checking all 4 directions */
    }
    return opened;
}

SearchCtx *search_create(SearchAlgo algo, Grid *g) {
    SearchCtx *ctx=calloc(1,sizeof(SearchCtx));
    ctx->algo=algo; ctx->grid=g;
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++){
        ctx->par_r[r][c]=-1; ctx->par_c[r][c]=-1; ctx->cost[r][c]=1e30f;
    }
    ctx->cost[g->startR][g->startC]=0;
    Node start={g->startR,g->startC,-1,-1,0,0};
    start.f=heuristic(g->startR,g->startC,g->endR,g->endC);
    switch(algo){
        case ALGO_BFS:
            ctx->bfs_q=que_new(); que_push(ctx->bfs_q,start);
            ctx->visited[g->startR][g->startC]=true; break;
        case ALGO_DFS:
            ctx->dfs_s=stk_new(); stk_push(ctx->dfs_s,start); break;
        default:
            ctx->pq=pq_new(); pq_push(ctx->pq,start); break;
    }
    clock_gettime(CLOCK_MONOTONIC,&ctx->t0);
    return ctx;
}

// trace path backward, accumulate score, log each tile event
static void trace_path(SearchCtx *ctx) {
    Grid *g=ctx->grid;
    int r=g->endR,c=g->endC;
    int len=0,pen=0,bon=0,th=0,bh=0,wo=0;
    ctx->result.event_count=0;

    while(!(r==g->startR&&c==g->startC)){
        if(r<0)break;
        CellType ct=g->cells[r][c];
        if(ct!=CELL_END&&ct!=CELL_START){
            int p=cell_penalty(ct);
            if(p>0){pen+=p;th++;}
            else if(p<0){bon+=(-p);bh++;}
            if(ct==CELL_BONUS_BOMB) {
                /* count how many craters in g->craters[] are adjacent to this bomb */
                int bomb_walls = 0;
                int dr[]={-1,1,0,0}, dc[]={0,0,-1,1};
                for (int d=0;d<4;d++) {
                    int wr=r+dr[d], wc=c+dc[d];
                    for (int k=0;k<g->crater_count;k++)
                        if (g->craters[k].r==wr && g->craters[k].c==wc)
                            bomb_walls++;
                }
                wo += bomb_walls;
                if (ctx->result.event_count<MAX_EVENTS) {
                    TileEvent ev;
                    ev.type  = ct;
                    ev.r     = r;
                    ev.c     = c;
                    ev.delta = bomb_walls;  /* store wall count in delta for display */
                    ctx->result.events[ctx->result.event_count++] = ev;
                }
            } else if(cell_is_special(ct) && ctx->result.event_count<MAX_EVENTS){
                TileEvent ev;
                ev.type  = ct;
                ev.r     = r;
                ev.c     = c;
                ev.delta = p;
                ctx->result.events[ctx->result.event_count++] = ev;
            }
        }
        g->vis[r][c]=VIS_PATH;
        int pr=ctx->par_r[r][c],pc=ctx->par_c[r][c];
        r=pr;c=pc;len++;
    }
    g->vis[g->startR][g->startC]=VIS_NONE;
    g->vis[g->endR  ][g->endC  ]=VIS_NONE;
    ctx->result.pathLen    =len;
    ctx->result.penalties  =pen;
    ctx->result.bonuses    =bon;
    ctx->result.traps_hit  =th;
    ctx->result.bonuses_hit=bh;
    ctx->result.walls_opened=wo;
    ctx->result.final_score=len*5 + pen*3 - bon*2;
}

bool search_step(SearchCtx *ctx) {
    if(ctx->done)return false;
    Grid *g=ctx->grid;
    int r=-1,c=-1,pr=-1,pc=-1;
    bool ok=false;

    if(ctx->algo==ALGO_BFS){
        if(que_empty(ctx->bfs_q)){ctx->done=true;goto fin;}
        Node n=que_pop(ctx->bfs_q);r=n.r;c=n.c;pr=n.parent_r;pc=n.parent_c;ok=true;
    } else if(ctx->algo==ALGO_DFS){
        if(!ctx->dfs_s->top){ctx->done=true;goto fin;}
        Node n=stk_pop(ctx->dfs_s);
        if(ctx->visited[n.r][n.c])return true;
        r=n.r;c=n.c;pr=n.parent_r;pc=n.parent_c;
        ctx->visited[r][c]=true;ok=true;
    } else {
        if(!ctx->pq->size){ctx->done=true;goto fin;}
        Node n=pq_pop(ctx->pq);
        if(ctx->visited[n.r][n.c])return true;
        r=n.r;c=n.c;pr=n.parent_r;pc=n.parent_c;
        ctx->visited[r][c]=true;ok=true;
    }
    if(!ok)return true;

    ctx->par_r[r][c]=pr; ctx->par_c[r][c]=pc;
    ctx->result.visited++;

    CellType ct=g->cells[r][c];

    // BOMB fires the moment the algorithm steps on the tile — BEFORE expanding
    // neighbours. The wall opens in cells[][] right now, so the expansion loop
    // below will see the newly passable cell and add it to the frontier.
    // The algorithm therefore knows about the shortcut and CAN route through it.
    if(ct==CELL_BONUS_BOMB)
        ctx->result.walls_opened += bomb_effect(g,ctx,r,c);

    // mark visited in vis overlay — don't overwrite START/END
    if(ct!=CELL_START&&ct!=CELL_END)
        g->vis[r][c]=VIS_VISITED;

    if(r==g->endR&&c==g->endC){
        ctx->done=ctx->found=true;
        trace_path(ctx);
        goto fin;
    }

    // expand neighbours — bomb-opened walls are now CELL_OPEN so they pass here
    for(int d=0;d<4;d++){
        int nr=r+DR[d],nc=c+DC[d];
        if(nr<0||nr>=ROWS||nc<0||nc>=COLS)continue;
        if(g->cells[nr][nc]==CELL_WALL)continue;
        if(ctx->visited[nr][nc])continue;

        float ec=edge_cost(ctx->algo,g->cells[nr][nc]);
        float ng=ctx->cost[r][c]+ec;
        if(ctx->algo!=ALGO_BFS&&ctx->algo!=ALGO_DFS&&ng>=ctx->cost[nr][nc])continue;
        ctx->cost[nr][nc]=ng;

        Node nn={nr,nc,r,c,ng,ng};
        if(ctx->algo==ALGO_ASTAR)
            nn.f=ng+heuristic(nr,nc,g->endR,g->endC);

        if(ctx->algo==ALGO_BFS){
            if(!ctx->visited[nr][nc]){
                ctx->visited[nr][nc]=true;
                que_push(ctx->bfs_q,nn);
                if(g->vis[nr][nc]==VIS_NONE)g->vis[nr][nc]=VIS_FRONTIER;
            }
        } else if(ctx->algo==ALGO_DFS){
            stk_push(ctx->dfs_s,nn);
            if(g->vis[nr][nc]==VIS_NONE)g->vis[nr][nc]=VIS_FRONTIER;
        } else {
            pq_push(ctx->pq,nn);
            if(g->vis[nr][nc]==VIS_NONE)g->vis[nr][nc]=VIS_FRONTIER;
        }
    }

fin:
    if(ctx->done){
        struct timespec t1;
        clock_gettime(CLOCK_MONOTONIC,&t1);
        ctx->result.timeMs=(t1.tv_sec-ctx->t0.tv_sec)*1000.0
                          +(t1.tv_nsec-ctx->t0.tv_nsec)/1e6;
    }
    return !ctx->done;
}

void search_destroy(SearchCtx *ctx){
    if(!ctx)return;
    if(ctx->bfs_q)que_free(ctx->bfs_q);
    if(ctx->dfs_s)stk_free(ctx->dfs_s);
    if(ctx->pq)pq_free(ctx->pq);
    free(ctx);
}

SearchResult search_get_result(SearchCtx *ctx){return ctx->result;}
