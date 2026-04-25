#include "maze.h"
#include <stdlib.h>
#include <string.h>

// ── helpers ───────────────────────────────────────────────────────────────────
static void shuffle4(int d[4]) {
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i+1);
        int t = d[i]; d[i] = d[j]; d[j] = t;
    }
}

// open the two rooms AND the wall cell between them
static void carve(Grid *g, int r1, int c1, int r2, int c2) {
    g->cells[r1][c1]                = CELL_OPEN;
    g->cells[(r1+r2)/2][(c1+c2)/2] = CELL_OPEN;
    g->cells[r2][c2]                = CELL_OPEN;
}

static bool is_wall(const Grid *g, int r, int c) {
    return g->cells[r][c] == CELL_WALL;
}

// ── 1. Recursive Backtracker ──────────────────────────────────────────────────
static void rb_dfs(Grid *g, int r, int c) {
    int dr[] = {-2, 2,  0,  0};
    int dc[] = { 0, 0, -2,  2};
    int d[4] = {0,1,2,3};
    shuffle4(d);
    for (int i = 0; i < 4; i++) {
        int nr = r+dr[d[i]], nc = c+dc[d[i]];
        if (nr>0 && nr<ROWS-1 && nc>0 && nc<COLS-1 && is_wall(g,nr,nc)) {
            carve(g, r, c, nr, nc);
            rb_dfs(g, nr, nc);
        }
    }
}

void gen_recursive_backtrack(Grid *g) {
    grid_init(g);
    rb_dfs(g, 1, 1);
}

// ── 2. Prim's ─────────────────────────────────────────────────────────────────
typedef struct { int r, c; } Cell;
#define MAX_F (ROWS*COLS)

void gen_prims(Grid *g) {
    grid_init(g);
    Cell *front = malloc(MAX_F * sizeof(Cell));
    int  fs = 0;
    int  dr[] = {-2,2,0,0}, dc[] = {0,0,-2,2};

    g->cells[1][1] = CELL_OPEN;
#define ADDF(rr,cc) \
    if((rr)>0&&(rr)<ROWS-1&&(cc)>0&&(cc)<COLS-1&&is_wall(g,(rr),(cc))) \
        front[fs++]=(Cell){(rr),(cc)};
    ADDF(1,3) ADDF(3,1)

    while (fs > 0) {
        int idx  = rand() % fs;
        Cell cur = front[idx];
        front[idx] = front[--fs];
        if (!is_wall(g, cur.r, cur.c)) continue;

        Cell nb[4]; int nn=0;
        for (int d=0;d<4;d++) {
            int nr=cur.r+dr[d], nc=cur.c+dc[d];
            if (nr>0&&nr<ROWS-1&&nc>0&&nc<COLS-1&&!is_wall(g,nr,nc))
                nb[nn++]=(Cell){nr,nc};
        }
        if (!nn) continue;
        Cell ch = nb[rand()%nn];
        carve(g, cur.r, cur.c, ch.r, ch.c);
        for (int d=0;d<4;d++) { ADDF(cur.r+dr[d], cur.c+dc[d]) }
    }
    free(front);
}

// ── 3. Kruskal's ──────────────────────────────────────────────────────────────
static int uf[ROWS*COLS];
static int uf_find(int x){ return uf[x]==x?x:(uf[x]=uf_find(uf[x])); }
static void uf_union(int a,int b){ uf[uf_find(a)]=uf_find(b); }

void gen_kruskals(Grid *g) {
    grid_init(g);
    for (int r=1;r<ROWS;r+=2) for (int c=1;c<COLS;c+=2) {
        g->cells[r][c]=CELL_OPEN; uf[r*COLS+c]=r*COLS+c;
    }
    typedef struct{int r1,c1,r2,c2;}Wall;
    Wall *walls = malloc(2*ROWS*COLS*sizeof(Wall));
    int wn=0;
    for (int r=1;r<ROWS;r+=2) for (int c=1;c<COLS;c+=2) {
        if (r+2<ROWS) walls[wn++]=(Wall){r,c,r+2,c};
        if (c+2<COLS) walls[wn++]=(Wall){r,c,r,c+2};
    }
    for (int i=wn-1;i>0;i--){ int j=rand()%(i+1); Wall t=walls[i];walls[i]=walls[j];walls[j]=t; }
    for (int i=0;i<wn;i++) {
        Wall w=walls[i];
        if (uf_find(w.r1*COLS+w.c1)!=uf_find(w.r2*COLS+w.c2)) {
            uf_union(w.r1*COLS+w.c1, w.r2*COLS+w.c2);
            carve(g, w.r1,w.c1,w.r2,w.c2);
        }
    }
    free(walls);
}

// ── 4. Aldous-Broder ──────────────────────────────────────────────────────────
void gen_aldous_broder(Grid *g) {
    grid_init(g);
    int total = ((ROWS-1)/2)*((COLS-1)/2);
    int visited=1, r=1, c=1;
    g->cells[r][c]=CELL_OPEN;
    int dr[]={-2,2,0,0}, dc[]={0,0,-2,2};
    while (visited<total) {
        int d=rand()%4, nr=r+dr[d], nc=c+dc[d];
        if (nr<=0||nr>=ROWS-1||nc<=0||nc>=COLS-1) continue;
        if (is_wall(g,nr,nc)) { carve(g,r,c,nr,nc); visited++; }
        r=nr; c=nc;
    }
}

// ── 5. Braided (Prim's base + extra walls removed for cycles) ─────────────────
#define BRAID_RATIO 0.42f

void gen_braided(Grid *g) {
    gen_prims(g);
    typedef struct{int r,c;}WP;
    WP *cands = malloc(ROWS*COLS*sizeof(WP));
    int nc=0;
    for (int r=1;r<ROWS-1;r++) for (int c=1;c<COLS-1;c++) {
        if (g->cells[r][c]!=CELL_WALL) continue;
        int h=(g->cells[r][c-1]!=CELL_WALL && g->cells[r][c+1]!=CELL_WALL);
        int v=(g->cells[r-1][c]!=CELL_WALL && g->cells[r+1][c]!=CELL_WALL);
        if (h||v) cands[nc++]=(WP){r,c};
    }
    for (int i=nc-1;i>0;i--){ int j=rand()%(i+1); WP t=cands[i];cands[i]=cands[j];cands[j]=t; }
    int rm=(int)(nc*BRAID_RATIO);
    for (int i=0;i<rm;i++) g->cells[cands[i].r][cands[i].c]=CELL_OPEN;
    free(cands);
}

// ── 6. Wilson's algorithm (loop-erased random walk) ───────────────────────────
// Produces a UNIFORM spanning tree — every possible perfect maze equally likely.
// Better texture than Aldous-Broder with similar fairness, and much faster.
void gen_wilsons(Grid *g) {
    grid_init(g);
    int total = ((ROWS-1)/2)*((COLS-1)/2);

    // track which ODD-indexed rooms have been added to the maze
    bool in_maze[ROWS][COLS];
    memset(in_maze, 0, sizeof(in_maze));

    // track direction we came from during a walk (for loop erasure)
    // 0=up 1=down 2=left 3=right, -1=unset
    int walk_dir[ROWS][COLS];

    int dr[]={-2,2,0,0}, dc[]={0,0,-2,2};

    // seed: add (1,1) to maze
    g->cells[1][1]=CELL_OPEN;
    in_maze[1][1]=true;
    int done=1;

    while (done < total) {
        // pick a random unvisited room as walk start
        int sr, sc;
        do {
            sr = 1 + 2*(rand()%((ROWS-1)/2));
            sc = 1 + 2*(rand()%((COLS-1)/2));
        } while (in_maze[sr][sc]);

        // perform a loop-erased random walk until we hit the maze
        memset(walk_dir, -1, sizeof(walk_dir));
        int r=sr, c=sc;

        while (!in_maze[r][c]) {
            int d=rand()%4;
            int nr=r+dr[d], nc2=c+dc[d];
            if (nr<=0||nr>=ROWS-1||nc2<=0||nc2>=COLS-1) continue;
            walk_dir[r][c]=d;
            r=nr; c=nc2;
        }

        // carve the recorded path from start to maze
        r=sr; c=sc;
        while (!in_maze[r][c]) {
            int d=walk_dir[r][c];
            int nr=r+dr[d], nc2=c+dc[d];
            carve(g, r, c, nr, nc2);
            in_maze[r][c]=true;
            done++;
            r=nr; c=nc2;
        }
    }
}
