#include "maze.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════ */
static Grid       g_grid;
static AppPhase   g_phase   = PHASE_MENU;
static GenAlgo    g_gen     = GEN_RECURSIVE_BACKTRACK;
static SearchAlgo g_algo    = ALGO_BFS;
static SearchCtx *g_ctx     = NULL;
static SearchResult g_result = {0, 0, 0.0};
static bool       g_searching = false;
static unsigned int g_seed  = 0;

/* ── forward ────────────────────────────────────────────────────────────── */
static void do_generate(void);
static void do_start_search(void);
static void do_reset_search(void);
static void draw_menu_overlay(void);
static void handle_cell_click(void);

/* ══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIN_W, WIN_H, "Maze Search — Graph Algorithm Visualizer");
    SetTargetFPS(60);

    g_seed = (unsigned int)time(NULL);
    grid_init(&g_grid);

    while (!WindowShouldClose()) {

        /* ── keyboard shortcuts ──────────────────────────────────────────── */
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (g_ctx) { search_destroy(g_ctx); g_ctx = NULL; }
            g_searching = false;
            g_phase = PHASE_MENU;
        }
        if (IsKeyPressed(KEY_ONE))   g_gen  = GEN_RECURSIVE_BACKTRACK;
        if (IsKeyPressed(KEY_TWO))   g_gen  = GEN_PRIMS;
        if (IsKeyPressed(KEY_THREE)) g_gen  = GEN_KRUSKALS;
        if (IsKeyPressed(KEY_FOUR))  g_gen  = GEN_ALDOUS_BRODER;
        if (IsKeyPressed(KEY_FIVE))  g_gen  = GEN_BRAIDED;
        if (IsKeyPressed(KEY_Q))     g_algo = ALGO_BFS;
        if (IsKeyPressed(KEY_W))     g_algo = ALGO_DFS;
        if (IsKeyPressed(KEY_E))     g_algo = ALGO_DIJKSTRA;
        if (IsKeyPressed(KEY_R) && g_phase != PHASE_SEARCHING)
            g_algo = ALGO_ASTAR;

        if (IsKeyPressed(KEY_G)) do_generate();

        if (IsKeyPressed(KEY_S)) {
            if (g_phase == PHASE_PICK_END || g_phase == PHASE_DONE ||
               (g_phase == PHASE_PICK_START && g_grid.startR >= 0 && g_grid.endR >= 0))
                do_start_search();
        }
        if (IsKeyPressed(KEY_R) && g_phase == PHASE_SEARCHING) {
            do_reset_search();
        }
        if (IsKeyPressed(KEY_R) && g_phase == PHASE_DONE) {
            do_reset_search();
        }

        /* ── cell picking ────────────────────────────────────────────────── */
        if ((g_phase == PHASE_PICK_START || g_phase == PHASE_PICK_END || g_phase == PHASE_DONE)
            && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            handle_cell_click();

        /* ── search stepping ─────────────────────────────────────────────── */
        if (g_phase == PHASE_SEARCHING && g_ctx) {
            bool running = search_step(g_ctx);
            if (!running) {
                g_result   = search_get_result(g_ctx);
                g_searching = false;
                g_phase    = PHASE_DONE;
            }
        }

        /* ═══════════════ DRAW ═════════════════════════════════════════════ */
        BeginDrawing();
        ClearBackground((Color){15,15,25,255});

        render_grid(&g_grid);
        render_topbar(g_phase);
        render_sidebar(g_phase, g_gen, g_algo, &g_result, g_searching);

        if (g_phase == PHASE_MENU)
            draw_menu_overlay();

        EndDrawing();
    }

    if (g_ctx) search_destroy(g_ctx);
    CloseWindow();
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════ */
static void do_generate(void) {
    if (g_ctx) { search_destroy(g_ctx); g_ctx = NULL; }
    g_searching = false;
    g_seed++;
    switch (g_gen) {
        case GEN_RECURSIVE_BACKTRACK: gen_recursive_backtrack(&g_grid, g_seed); break;
        case GEN_PRIMS:               gen_prims(&g_grid, g_seed);               break;
        case GEN_KRUSKALS:            gen_kruskals(&g_grid, g_seed);            break;
        case GEN_ALDOUS_BRODER:       gen_aldous_broder(&g_grid, g_seed);       break;
        case GEN_BRAIDED:             gen_braided(&g_grid, g_seed);             break;
        default: break;
    }
    g_grid.startR = g_grid.startC = -1;
    g_grid.endR   = g_grid.endC   = -1;
    memset(&g_result, 0, sizeof(g_result));
    g_phase = PHASE_PICK_START;
}

static void do_start_search(void) {
    if (g_grid.startR < 0 || g_grid.endR < 0) return;
    if (g_ctx) { search_destroy(g_ctx); g_ctx = NULL; }
    grid_clear_search(&g_grid);
    g_ctx = search_create(g_algo, &g_grid);
    g_searching = true;
    g_phase = PHASE_SEARCHING;
    memset(&g_result, 0, sizeof(g_result));
}

static void do_reset_search(void) {
    if (g_ctx) { search_destroy(g_ctx); g_ctx = NULL; }
    g_searching = false;
    grid_clear_search(&g_grid);
    memset(&g_result, 0, sizeof(g_result));
    g_phase = PHASE_PICK_START;
}

static void handle_cell_click(void) {
    Vector2 mp = GetMousePosition();
    int c = (int)((mp.x - GRID_OFF_X) / CELL_SIZE);
    int r = (int)((mp.y - GRID_OFF_Y) / CELL_SIZE);
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return;
    if (g_grid.cells[r][c] != OPEN) return;

    // if a search just finished, reset visuals before re-picking
    if (g_phase == PHASE_DONE) {
        if (g_ctx) { search_destroy(g_ctx); g_ctx = NULL; }
        g_searching = false;
        grid_clear_search(&g_grid);
        memset(&g_result, 0, sizeof(g_result));
    }

    // odd click = start, even click = end
    // g_phase == PHASE_PICK_START means next click is a start
    if (g_phase == PHASE_PICK_START || g_phase == PHASE_DONE) {
        // place start
        if (g_grid.startR >= 0)
            g_grid.vis[g_grid.startR][g_grid.startC] = STATE_OPEN;
        // don't overwrite the end marker
        if (r == g_grid.endR && c == g_grid.endC) return;
        g_grid.startR = r; g_grid.startC = c;
        g_grid.vis[r][c] = STATE_START;
        g_phase = PHASE_PICK_END;
    } else {
        // place end
        if (r == g_grid.startR && c == g_grid.startC) return;
        if (g_grid.endR >= 0)
            g_grid.vis[g_grid.endR][g_grid.endC] = STATE_OPEN;
        g_grid.endR = r; g_grid.endC = c;
        g_grid.vis[r][c] = STATE_END;
        g_phase = PHASE_PICK_START;  // next click will set a new start
    }
}

/* ── Menu overlay shown before first generation ───────────────────────────── */
static void draw_menu_overlay(void) {
    int gw = COLS * CELL_SIZE;
    int gh = ROWS * CELL_SIZE + GRID_OFF_Y;

    // semi-transparent panel
    DrawRectangle(gw/2-220, gh/2-190, 440, 380, (Color){10,12,22,230});
    DrawRectangleLines(gw/2-220, gh/2-190, 440, 380, (Color){80,160,255,200});

    DrawText("MAZE SEARCH VISUALIZER",
             gw/2 - MeasureText("MAZE SEARCH VISUALIZER",20)/2,
             gh/2-175, 20, (Color){80,160,255,255});
    DrawText("Graph Algorithm Comparison",
             gw/2 - MeasureText("Graph Algorithm Comparison",13)/2,
             gh/2-148, 13, (Color){130,140,160,255});

    int y = gh/2 - 120;
    DrawText("Select Generator:", gw/2-200, y, 14, (Color){200,210,230,255}); y+=22;
    const char *gnames[5] = {"1 - Recursive Backtracker","2 - Prim's","3 - Kruskal's","4 - Aldous-Broder","5 - Braided (cycles)"};
    for (int i=0;i<5;i++) {
        if (button(gw/2-200, y, 200, 24, gnames[i], g_gen==(GenAlgo)i)) g_gen=(GenAlgo)i;
        y += 30;
    }

    y += 6;
    DrawText("Select Search Algorithm:", gw/2-200, y, 14, (Color){200,210,230,255}); y+=22;
    const char *anames[4] = {"Q - BFS","W - DFS","E - Dijkstra","R - A*"};
    for (int i=0;i<4;i++) {
        if (button(gw/2-200, y, 200, 24, anames[i], g_algo==(SearchAlgo)i)) g_algo=(SearchAlgo)i;
        y += 30;
    }

    y += 10;
    if (button(gw/2-100, y, 200, 36, "GENERATE MAZE  [G]", false))
        do_generate();
}
