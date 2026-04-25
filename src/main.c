#include "maze.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static Grid        g_grid;
static AppPhase    g_phase    = PHASE_MENU;
static GenAlgo     g_gen      = GEN_BRAIDED;   // Braided = multi-path, best for trap research
static SearchAlgo  g_algo     = ALGO_BFS;
static SearchCtx  *g_ctx      = NULL;
static SearchResult g_result;
static bool        g_searching  = false;
static bool        g_show_report = false;  // TAB toggles the report popup

static void do_generate(void);
static void do_start_search(void);
static void do_reset_search(void);
static void do_full_reset(void);
static void draw_menu_overlay(void);
static void handle_click(void);

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIN_W, WIN_H, "Treasure Hunt — Graph Algorithm Showdown");
    SetTargetFPS(60);
    srand((unsigned int)time(NULL));
    render_init();   // load textures
    memset(&g_result,0,sizeof(g_result));
    grid_init(&g_grid);

    while (!WindowShouldClose()) {

        // ── input ────────────────────────────────────────────────────────────
        // TAB or H — toggle report popup
        if ((IsKeyPressed(KEY_TAB)||IsKeyPressed(KEY_H)) && g_phase==PHASE_DONE)
            g_show_report = !g_show_report;

        // block all other input while popup is open
        if (g_show_report) goto draw;

        if (IsKeyPressed(KEY_ESCAPE)){
            if(g_ctx){search_destroy(g_ctx);g_ctx=NULL;}
            g_searching=false; g_phase=PHASE_MENU;
        }
        if (IsKeyPressed(KEY_ONE))   g_gen=GEN_RECURSIVE_BACKTRACK;
        if (IsKeyPressed(KEY_TWO))   g_gen=GEN_PRIMS;
        if (IsKeyPressed(KEY_THREE)) g_gen=GEN_KRUSKALS;
        if (IsKeyPressed(KEY_FOUR))  g_gen=GEN_ALDOUS_BRODER;
        if (IsKeyPressed(KEY_FIVE))  g_gen=GEN_BRAIDED;
        if (IsKeyPressed(KEY_SIX))   g_gen=GEN_WILSONS;

        if (g_phase!=PHASE_SEARCHING) {
            if (IsKeyPressed(KEY_Q)) g_algo=ALGO_BFS;
            if (IsKeyPressed(KEY_W)) g_algo=ALGO_DFS;
            if (IsKeyPressed(KEY_E)) g_algo=ALGO_DIJKSTRA;
            if (IsKeyPressed(KEY_R)) g_algo=ALGO_ASTAR;
        }
        if (IsKeyPressed(KEY_G)) do_generate();
        if (IsKeyPressed(KEY_S)) {
            if (g_phase==PHASE_PICK_END||g_phase==PHASE_DONE||
               (g_phase==PHASE_PICK_START&&g_grid.startR>=0&&g_grid.endR>=0))
                do_start_search();
        }
        if (IsKeyPressed(KEY_X))
            if (g_phase==PHASE_SEARCHING||g_phase==PHASE_DONE)
                do_reset_search();

        // Z = full reset — same maze, same traps, bomb craters undone, start/end cleared
        if (IsKeyPressed(KEY_Z) && g_phase!=PHASE_MENU && g_phase!=PHASE_SEARCHING)
            do_full_reset();

        if ((g_phase==PHASE_PICK_START||g_phase==PHASE_PICK_END||g_phase==PHASE_DONE)
                &&IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            handle_click();

        // ── search step ──────────────────────────────────────────────────────
        if (g_phase==PHASE_SEARCHING&&g_ctx) {
            bool running=search_step(g_ctx);
            if (!running) {
                g_result=search_get_result(g_ctx);
                g_searching=false;
                g_phase=PHASE_DONE;
                g_show_report=true;   // auto-open report when search finishes
            }
        }

        draw:;
        // ── draw ─────────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(CLITERAL(Color){15,15,25,255});
        render_grid(&g_grid);
        render_topbar(g_phase);
        render_sidebar(g_phase,g_gen,g_algo,&g_result,g_searching);
        if (g_phase==PHASE_MENU) draw_menu_overlay();
        if (g_show_report && g_phase==PHASE_DONE)
            render_report_popup(&g_result, g_algo, &g_show_report);
        EndDrawing();
    }

    render_free();
    if (g_ctx) search_destroy(g_ctx);
    CloseWindow();
    return 0;
}

static void do_generate(void) {
    if(g_ctx){search_destroy(g_ctx);g_ctx=NULL;}
    g_searching=false;
    g_show_report=false;
    switch(g_gen){
        case GEN_RECURSIVE_BACKTRACK: gen_recursive_backtrack(&g_grid); break;
        case GEN_PRIMS:               gen_prims(&g_grid);               break;
        case GEN_KRUSKALS:            gen_kruskals(&g_grid);            break;
        case GEN_ALDOUS_BRODER:       gen_aldous_broder(&g_grid);       break;
        case GEN_BRAIDED:             gen_braided(&g_grid);             break;
        case GEN_WILSONS:             gen_wilsons(&g_grid);             break;
        default: break;
    }
    traps_place(&g_grid);
    g_grid.startR=g_grid.startC=-1;
    g_grid.endR  =g_grid.endC  =-1;
    memset(&g_result,0,sizeof(g_result));
    g_phase=PHASE_PICK_START;
}

static void do_start_search(void) {
    if(g_grid.startR<0||g_grid.endR<0)return;
    if(g_ctx){search_destroy(g_ctx);g_ctx=NULL;}
    grid_clear_search(&g_grid);
    g_ctx=search_create(g_algo,&g_grid);
    g_searching=true;
    g_phase=PHASE_SEARCHING;
    memset(&g_result,0,sizeof(g_result));
}

static void do_full_reset(void) {
    if(g_ctx){search_destroy(g_ctx);g_ctx=NULL;}
    g_searching=false;
    g_show_report=false;
    grid_full_reset(&g_grid);   // undo bombs, clear start/end, wipe vis
    memset(&g_result,0,sizeof(g_result));
    g_phase=PHASE_PICK_START;
}

static void do_reset_search(void) {
    if(g_ctx){search_destroy(g_ctx);g_ctx=NULL;}
    g_searching=false;
    g_show_report=false;
    grid_clear_search(&g_grid);
    memset(&g_result,0,sizeof(g_result));
    g_phase=PHASE_PICK_START;
}

static void handle_click(void) {
    Vector2 mp=GetMousePosition();
    int c=(int)((mp.x-GRID_OFF_X)/CELL_SIZE);
    int r=(int)((mp.y-GRID_OFF_Y)/CELL_SIZE);
    if(r<0||r>=ROWS||c<0||c>=COLS)return;

    CellType ct=g_grid.cells[r][c];

    // block walls and ALL special tiles — can only place on plain OPEN cells
    // (CELL_START and CELL_END are allowed so user can reuse the same spot)
    if(ct==CELL_WALL)return;
    if(cell_is_special(ct))return;   // trap or bonus — not a valid start/end position

    if(g_phase==PHASE_DONE){
        if(g_ctx){search_destroy(g_ctx);g_ctx=NULL;}
        g_searching=false;
        g_show_report=false;
        grid_clear_search(&g_grid);
        memset(&g_result,0,sizeof(g_result));
    }

    if(g_phase==PHASE_PICK_START||g_phase==PHASE_DONE){
        if(g_grid.startR>=0) g_grid.cells[g_grid.startR][g_grid.startC]=CELL_OPEN;
        if(r==g_grid.endR&&c==g_grid.endC)return;
        g_grid.startR=r; g_grid.startC=c;
        g_grid.cells[r][c]=CELL_START;
        g_phase=PHASE_PICK_END;
    } else {
        if(r==g_grid.startR&&c==g_grid.startC)return;
        if(g_grid.endR>=0) g_grid.cells[g_grid.endR][g_grid.endC]=CELL_OPEN;
        g_grid.endR=r; g_grid.endC=c;
        g_grid.cells[r][c]=CELL_END;
        g_phase=PHASE_PICK_START;
    }
}

static void draw_menu_overlay(void) {
    int gw=COLS*CELL_SIZE, gh=WIN_H;
    DrawRectangle(gw/2-225,gh/2-215,450,430,CLITERAL(Color){10,12,22,235});
    DrawRectangleLines(gw/2-225,gh/2-215,450,430,CLITERAL(Color){80,160,255,200});

    DrawText("TREASURE HUNT",
        gw/2-MeasureText("TREASURE HUNT",26)/2,gh/2-200,26,CLITERAL(Color){80,160,255,255});
    DrawText("Graph Algorithm Showdown",
        gw/2-MeasureText("Graph Algorithm Showdown",13)/2,gh/2-168,13,CLITERAL(Color){100,110,140,255});

    int y=gh/2-140;
    DrawText("Maze Generator:",gw/2-205,y,13,CLITERAL(Color){200,210,230,255}); y+=20;
    const char *gn[]={"1-Recursive BT","2-Prim's","3-Kruskal's",
                      "4-Aldous-Broder","5-Braided (multi-path)","6-Wilson's"};
    for(int i=0;i<6;i++){if(button(gw/2-205,y,210,24,gn[i],g_gen==(GenAlgo)i))g_gen=(GenAlgo)i;y+=28;}

    y+=6;
    DrawText("Search Algorithm:",gw/2-205,y,13,CLITERAL(Color){200,210,230,255}); y+=20;
    const char *an[]={"Q-BFS","W-DFS","E-Dijkstra","R-A*"};
    for(int i=0;i<4;i++){if(button(gw/2-205,y,210,24,an[i],g_algo==(SearchAlgo)i))g_algo=(SearchAlgo)i;y+=28;}

    y+=12;
    if(button(gw/2-105,y,210,36,"GENERATE MAZE  [G]",false)) do_generate();
}
