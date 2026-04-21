#include "maze.h"
#include "raylib.h"
#include <stdio.h>

/* ── Palette ──────────────────────────────────────────────────────────────── */
#define C_BG        (Color){ 15, 15, 25, 255}
#define C_WALL      (Color){ 30, 30, 50, 255}
#define C_OPEN      (Color){ 50, 55, 75, 255}
#define C_START     (Color){ 60,220, 90, 255}
#define C_END       (Color){220, 70, 70, 255}
#define C_VISITED   (Color){ 60,110,200, 255}
#define C_FRONTIER  (Color){150,200,255, 255}
#define C_PATH      (Color){255,210, 50, 255}
#define C_SIDEBAR   (Color){ 20, 22, 35, 255}
#define C_ACCENT    (Color){ 80,160,255, 255}
#define C_TEXT      (Color){210,215,230, 255}
#define C_DIM       (Color){100,105,120, 255}
#define C_TOPBAR    (Color){ 25, 27, 42, 255}

static Color state_color(CellState s) {
    switch (s) {
        case STATE_WALL:     return C_WALL;
        case STATE_OPEN:     return C_OPEN;
        case STATE_START:    return C_START;
        case STATE_END:      return C_END;
        case STATE_VISITED:  return C_VISITED;
        case STATE_FRONTIER: return C_FRONTIER;
        case STATE_PATH:     return C_PATH;
        default:             return C_OPEN;
    }
}

void render_grid(const Grid *g) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int x = GRID_OFF_X + c * CELL_SIZE;
            int y = GRID_OFF_Y + r * CELL_SIZE;
            Color col = state_color(g->vis[r][c]);
            DrawRectangle(x, y, CELL_SIZE - WALL_THICK, CELL_SIZE - WALL_THICK, col);
        }
    }
}

/* ── helper: label + value row ────────────────────────────────────────────── */
static void stat_row(int x, int y, const char *label, const char *val) {
    DrawTextEx(GetFontDefault(), label, (Vector2){x, y}, 14, 1, C_DIM);
    DrawTextEx(GetFontDefault(), val,   (Vector2){x+110, y}, 14, 1, C_TEXT);
}

void render_sidebar(AppPhase phase, GenAlgo gen, SearchAlgo algo,
                    const SearchResult *res, bool searching) {
    int sx = COLS * CELL_SIZE;
    DrawRectangle(sx, 0, SIDEBAR_W, WIN_H, C_SIDEBAR);
    DrawLine(sx, 0, sx, WIN_H, C_ACCENT);

    int y = GRID_OFF_Y + 10;
    int x = sx + 14;

    /* Title */
    DrawTextEx(GetFontDefault(), "MAZE SEARCH", (Vector2){x, y}, 20, 2, C_ACCENT);
    y += 30;
    DrawLine(x, y, sx+SIDEBAR_W-14, y, C_ACCENT); y += 12;

    /* Generator */
    const char *gen_names[] = {"Recursive BT","Prim's","Kruskal's","Aldous-Broder","Braided (cycles)"};
    DrawTextEx(GetFontDefault(), "Generator", (Vector2){x, y}, 13, 1, C_DIM); y+=18;
    DrawTextEx(GetFontDefault(), gen_names[gen], (Vector2){x, y}, 15, 1, C_TEXT); y+=26;

    /* Algorithm */
    const char *algo_names[] = {"BFS","DFS","Dijkstra","A*"};
    DrawTextEx(GetFontDefault(), "Search", (Vector2){x, y}, 13, 1, C_DIM); y+=18;
    DrawTextEx(GetFontDefault(), algo_names[algo], (Vector2){x, y}, 15, 1, C_TEXT); y+=28;

    DrawLine(x, y, sx+SIDEBAR_W-14, y, (Color){50,55,80,255}); y+=12;

    /* Stats */
    DrawTextEx(GetFontDefault(), "Statistics", (Vector2){x, y}, 14, 1, C_ACCENT); y+=20;

    char buf[64];
    if (searching || phase == PHASE_DONE) {
        snprintf(buf,64,"%d", res->visited);
        stat_row(x, y, "Visited:", buf); y+=20;
        snprintf(buf,64,"%d", res->pathLen);
        stat_row(x, y, "Path len:", buf); y+=20;
        snprintf(buf,64,"%.2f ms", res->timeMs);
        stat_row(x, y, "Time:", buf); y+=20;
    } else {
        DrawTextEx(GetFontDefault(), "Run a search to see stats",
                   (Vector2){x,y}, 12, 1, C_DIM);
        y += 20;
    }

    DrawLine(x, y+4, sx+SIDEBAR_W-14, y+4, (Color){50,55,80,255}); y+=18;

    /* Legend */
    DrawTextEx(GetFontDefault(), "Legend", (Vector2){x, y}, 14, 1, C_ACCENT); y+=18;
    typedef struct { Color col; const char *name; } LegItem;
    LegItem items[] = {
        {C_START,   "Start"},
        {C_END,     "End"},
        {C_VISITED, "Visited"},
        {C_FRONTIER,"Frontier"},
        {C_PATH,    "Path"},
        {C_OPEN,    "Open"},
        {C_WALL,    "Wall"},
    };
    for (int i = 0; i < 7; i++) {
        DrawRectangle(x, y+1, 12, 12, items[i].col);
        DrawTextEx(GetFontDefault(), items[i].name, (Vector2){x+18, y}, 12, 1, C_TEXT);
        y += 17;
    }

    y += 10;
    DrawLine(x, y, sx+SIDEBAR_W-14, y, (Color){50,55,80,255}); y+=10;

    /* Controls hint */
    DrawTextEx(GetFontDefault(), "Controls", (Vector2){x, y}, 14, 1, C_ACCENT); y+=18;
    const char *hints[] = {
        "[G] New maze","[1-5] Generator","[Q-R] Algorithm",
        "[S] Search","[R] Reset search","[ESC] Menu"
    };
    for (int i = 0; i < 6; i++) {
        DrawTextEx(GetFontDefault(), hints[i], (Vector2){x, y}, 11, 1, C_DIM);
        y += 15;
    }
}

void render_topbar(AppPhase phase) {
    DrawRectangle(0, 0, COLS*CELL_SIZE, GRID_OFF_Y, C_TOPBAR);
    DrawLine(0, GRID_OFF_Y, COLS*CELL_SIZE, GRID_OFF_Y, C_ACCENT);
    const char *msg = "";
    switch (phase) {
        case PHASE_MENU:        msg = "Select generator & algorithm below, then press SPACE to build maze"; break;
        case PHASE_PICK_START:  msg = "Click to set START  (next click will be end)"; break;
        case PHASE_PICK_END:    msg = "Click to set END  —  press [S] to search, or click again to move start"; break;
        case PHASE_SEARCHING:   msg = "Searching...  press [R] to reset"; break;
        case PHASE_DONE:        msg = "Done!  Click to place new START  |  [S] search again  |  [G] new maze"; break;
    }
    DrawTextEx(GetFontDefault(), msg, (Vector2){10, 8}, 13, 1, C_TEXT);
}
