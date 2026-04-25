#include "maze.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

// ── Palette ───────────────────────────────────────────────────────────────────
#define C_WALL      CLITERAL(Color){ 28, 28, 48,255}
#define C_WALL_HL   CLITERAL(Color){ 45, 45, 72,255}
#define C_OPEN      CLITERAL(Color){ 45, 50, 70,255}
#define C_VISITED   CLITERAL(Color){ 55,100,190,255}
#define C_FRONTIER  CLITERAL(Color){130,180,255,255}
#define C_PATH      CLITERAL(Color){255,210, 50,255}
// trap border tints (used ONLY when PNG is missing)
#define C_SPIKE     CLITERAL(Color){210, 40, 40,255}
#define C_FREEZE    CLITERAL(Color){ 60,205,230,255}
#define C_TELE      CLITERAL(Color){155, 55,225,255}
#define C_BOMB      CLITERAL(Color){225,105, 15,255}
#define C_SPEED     CLITERAL(Color){240,230, 40,255}
#define C_COIN      CLITERAL(Color){255,195, 25,255}
// UI
#define C_SIDEBAR   CLITERAL(Color){ 18, 18, 32,255}
#define C_TOPBAR    CLITERAL(Color){ 20, 20, 35,255}
#define C_ACCENT    CLITERAL(Color){ 80,160,255,255}
#define C_TEXT      CLITERAL(Color){210,215,235,255}
#define C_DIM       CLITERAL(Color){ 95,100,120,255}
#define C_GOOD      CLITERAL(Color){ 70,200, 90,255}
#define C_BAD       CLITERAL(Color){220, 70, 70,255}
#define C_DIV       CLITERAL(Color){ 45, 48, 75,255}
#define C_POPUP_BG  CLITERAL(Color){ 12, 12, 24,245}
#define C_POPUP_BD  CLITERAL(Color){ 80,160,255,220}

// ── Textures ──────────────────────────────────────────────────────────────────
// Index mapping:
//   2 = CELL_START,  3 = CELL_END
//   4 = CELL_TRAP_SPIKE .. 9 = CELL_BONUS_COIN
static Texture2D tex[10];
static bool      tex_ok[10];

static const struct { int idx; const char *path; } TEX_MAP[] = {
    {2, "assets/start.png"},
    {3, "assets/end.png"},
    {4, "assets/spike.png"},
    {5, "assets/freeze.png"},
    {6, "assets/vortex.png"},
    {7, "assets/bomb.png"},
    {8, "assets/speed.png"},
    {9, "assets/coin.png"},
};
#define N_TEX (int)(sizeof(TEX_MAP)/sizeof(TEX_MAP[0]))

void render_init(void) {
    memset(tex_ok, 0, sizeof(tex_ok));
    for (int i=0;i<N_TEX;i++) {
        if (FileExists(TEX_MAP[i].path)) {
            tex[TEX_MAP[i].idx] = LoadTexture(TEX_MAP[i].path);
            SetTextureFilter(tex[TEX_MAP[i].idx], TEXTURE_FILTER_BILINEAR);
            tex_ok[TEX_MAP[i].idx] = true;
        }
    }
}

void render_free(void) {
    for (int i=0;i<10;i++)
        if (tex_ok[i]) UnloadTexture(tex[i]);
}

// fallback border colour when PNG is missing
static Color trap_border_color(CellType t) {
    switch(t){
        case CELL_TRAP_SPIKE:  return C_SPIKE;
        case CELL_TRAP_FREEZE: return C_FREEZE;
        case CELL_TRAP_TELE:   return C_TELE;
        case CELL_BONUS_BOMB:  return C_BOMB;
        case CELL_BONUS_SPEED: return C_SPEED;
        case CELL_BONUS_COIN:  return C_COIN;
        default:               return C_ACCENT;
    }
}

// draw a texture centred+fitted in the cell rectangle
static void draw_tex(int idx, int x, int y, int sz, Color tint) {
    Rectangle src = {0,0,(float)tex[idx].width,(float)tex[idx].height};
    Rectangle dst = {(float)x,(float)y,(float)sz,(float)sz};
    DrawTexturePro(tex[idx], src, dst, (Vector2){0,0}, 0, tint);
}

void render_grid(const Grid *g) {
    int cs = CELL_SIZE - WALL_THICK;   // drawn size per cell (gap between cells)

    for (int r=0;r<ROWS;r++) {
        for (int c=0;c<COLS;c++) {
            int x = GRID_OFF_X + c*CELL_SIZE;
            int y = GRID_OFF_Y + r*CELL_SIZE;
            CellType ct = g->cells[r][c];
            VisState vs = g->vis[r][c];

            // ── 1. Base floor ───────────────────────────────────────────────
            // Walls always get wall colour.
            // Everything else gets the open floor colour — traps have NO background.
            if (ct==CELL_WALL) {
                DrawRectangle(x,y,cs,cs,C_WALL);
                DrawLine(x,y,x+cs,y,C_WALL_HL);
                DrawLine(x,y,x,y+cs,C_WALL_HL);
                continue;
            }

            // open floor as base for all non-wall cells
            DrawRectangle(x,y,cs,cs,C_OPEN);

            // ── 2. Search overlay ───────────────────────────────────────────
            if (vs==VIS_VISITED)
                DrawRectangle(x,y,cs,cs,C_VISITED);
            else if (vs==VIS_FRONTIER)
                DrawRectangle(x,y,cs,cs,C_FRONTIER);
            else if (vs==VIS_PATH)
                DrawRectangle(x,y,cs,cs,CLITERAL(Color){255,210,50,180});
            else if (vs==VIS_EXPLODED) {
                // bomb crater — orange-tinted open cell so it's clearly visible
                DrawRectangle(x,y,cs,cs,CLITERAL(Color){80,45,10,255});
                // cracks: two diagonal lines
                DrawLine(x,y,x+cs,y+cs,CLITERAL(Color){225,105,15,180});
                DrawLine(x+cs,y,x,y+cs,CLITERAL(Color){225,105,15,120});
            }

            // ── 3. Tile icon ────────────────────────────────────────────────
            // Draw PNG if available, else a coloured border for trap tiles.
            // Always draw icons — even over search colours — so you can see
            // which traps the path hit.
            int pad=3;
            if (ct==CELL_START || ct==CELL_END) {
                // start and end icons always drawn
                int ti = (ct==CELL_START)?2:3;
                if (tex_ok[ti])
                    draw_tex(ti, x+pad, y+pad, cs-pad*2, WHITE);
            } else if (ct>=CELL_TRAP_SPIKE && ct<=CELL_BONUS_COIN) {
                if (tex_ok[ct]) {
                    // draw icon at full cell size with no background
                    draw_tex(ct, x+pad, y+pad, cs-pad*2, WHITE);
                } else {
                    // fallback: coloured border only — no filled background
                    DrawRectangleLinesEx(
                        (Rectangle){x+2,y+2,cs-4,cs-4},
                        2, trap_border_color(ct));
                    // small dot in centre so it's still visible at small sizes
                    DrawRectangle(x+cs/2-2, y+cs/2-2, 4, 4, trap_border_color(ct));
                }
            }
        }
    }
}

// ── Sidebar ───────────────────────────────────────────────────────────────────
static void lv(int x,int y,const char *lab,const char *val,Color vc){
    DrawTextEx(GetFontDefault(),lab,(Vector2){x,y},13,1,C_DIM);
    DrawTextEx(GetFontDefault(),val,(Vector2){x+148,y},13,1,vc);
}
static void divl(int x,int y,int w){DrawLine(x,y,x+w,y,C_DIV);}

void render_sidebar(AppPhase phase, GenAlgo gen, SearchAlgo algo,
                    const SearchResult *res, bool searching) {
    int sx=COLS*CELL_SIZE, sw=SIDEBAR_W;
    DrawRectangle(sx,0,sw,WIN_H,C_SIDEBAR);
    DrawLine(sx,0,sx,WIN_H,C_ACCENT);

    int x=sx+14, y=GRID_OFF_Y+10, iw=sw-28;

    DrawTextEx(GetFontDefault(),"TREASURE HUNT",(Vector2){x,y},20,2,C_ACCENT); y+=28;
    DrawTextEx(GetFontDefault(),"Algorithm Showdown",(Vector2){x,y},12,1,C_DIM); y+=18;
    divl(x,y,iw); y+=10;

    const char *gn[]={"Rec. Backtracker","Prim's","Kruskal's",
                      "Aldous-Broder","Braided (multi-path)","Wilson's"};
    const char *an[]={"BFS","DFS","Dijkstra","A*"};
    DrawTextEx(GetFontDefault(),"Generator",(Vector2){x,y},12,1,C_DIM); y+=14;
    DrawTextEx(GetFontDefault(),gn[gen],(Vector2){x,y},15,1,C_TEXT); y+=20;
    DrawTextEx(GetFontDefault(),"Algorithm",(Vector2){x,y},12,1,C_DIM); y+=14;
    DrawTextEx(GetFontDefault(),an[algo],(Vector2){x,y},15,1,C_TEXT); y+=18;
    divl(x,y,iw); y+=10;

    // live stats during search
    DrawTextEx(GetFontDefault(),"Statistics",(Vector2){x,y},13,1,C_ACCENT); y+=17;
    char buf[80];
    if (searching||phase==PHASE_DONE) {
        snprintf(buf,sizeof buf,"%d",res->visited);
        lv(x,y,"Cells visited:",buf,C_TEXT); y+=16;
        snprintf(buf,sizeof buf,"%d",res->pathLen);
        lv(x,y,"Path length:",buf,C_TEXT); y+=16;
        snprintf(buf,sizeof buf,"%.2f ms",res->timeMs);
        lv(x,y,"Time:",buf,C_TEXT); y+=16;
        if (phase==PHASE_DONE) {
            divl(x,y,iw); y+=8;
            // final score box
            DrawRectangle(x-2,y,iw+4,28,CLITERAL(Color){20,20,40,255});
            DrawRectangleLinesEx((Rectangle){x-2,y,iw+4,28},2,C_ACCENT);
            snprintf(buf,sizeof buf,"SCORE: %d",res->final_score);
            int tw=MeasureText(buf,16);
            DrawText(buf,sx+sw/2-tw/2,y+6,16,C_ACCENT);
            y+=34;
            DrawTextEx(GetFontDefault(),"(lower = better)",(Vector2){x,y},10,1,C_DIM); y+=14;
            DrawTextEx(GetFontDefault(),"[TAB] Full report",(Vector2){x,y},11,1,C_ACCENT); y+=16;
        }
    } else {
        DrawTextEx(GetFontDefault(),"Run a search to see stats",
                   (Vector2){x,y},12,1,C_DIM); y+=18;
    }

    divl(x,y,iw); y+=10;

    // tile legend — small squares with colour + name + score
    DrawTextEx(GetFontDefault(),"Tile Legend",(Vector2){x,y},13,1,C_ACCENT); y+=15;
    typedef struct{Color col;const char *nm;const char *pt;}Leg;
    Leg items[]={
        {CLITERAL(Color){50,210,80,255}, "Start",      ""},
        {CLITERAL(Color){230,180,20,255},"Treasure",   ""},
        {C_PATH,   "Path",     ""},
        {C_VISITED,"Visited",  ""},
        {CLITERAL(Color){80,45,10,255}, "Bomb crater",""},
        {C_SPIKE,  "Spike",   "+80"},
        {C_FREEZE, "Freeze",  "+50"},
        {C_TELE,   "Vortex",  "+65"},
        {C_BOMB,   "Bomb",    "wall"},
        {C_SPEED,  "Speed",   "-30"},
        {C_COIN,   "Coin",    "-15"},
    };
    for(int i=0;i<11;i++){
        DrawRectangle(x,y+1,11,11,items[i].col);
        DrawTextEx(GetFontDefault(),items[i].nm,(Vector2){x+15,y},11,1,C_TEXT);
        if(items[i].pt[0]){
            Color pc = items[i].pt[0]=='+' ? C_BAD
                     : items[i].pt[0]=='-' ? C_GOOD
                     : C_DIM;
            DrawTextEx(GetFontDefault(),items[i].pt,(Vector2){x+iw-28,y},11,1,pc);
        }
        y+=14;
    }

    divl(x,y,iw); y+=8;
    DrawTextEx(GetFontDefault(),"Controls",(Vector2){x,y},13,1,C_ACCENT); y+=14;
    const char *hints[]={"[G] Generate","[1-6] Generator",
        "[Q/W/E/R] Algorithm","[S] Search",
        "[X] Reset search","[Z] Full reset (same maze)",
        "[TAB/H] Report","[ESC] Menu"};
    for(int i=0;i<8;i++){
        DrawTextEx(GetFontDefault(),hints[i],(Vector2){x,y},11,1,C_DIM); y+=13;
    }
}

void render_topbar(AppPhase phase) {
    DrawRectangle(0,0,WIN_W,GRID_OFF_Y,C_TOPBAR);
    DrawLine(0,GRID_OFF_Y,WIN_W,GRID_OFF_Y,C_ACCENT);
    const char *msg="";
    switch(phase){
        case PHASE_MENU:       msg="Select generator & algorithm — [G] to build"; break;
        case PHASE_PICK_START: msg="Click to place START"; break;
        case PHASE_PICK_END:   msg="Click to place TREASURE (end)  |  [S] to search"; break;
        case PHASE_SEARCHING:  msg="Searching...  [X] to reset"; break;
        case PHASE_DONE:       msg="Done!  [TAB] report  |  [S] search again  |  [Z] full reset  |  [G] new maze"; break;
    }
    int tw=MeasureText(msg,13);
    DrawText(msg,(COLS*CELL_SIZE-tw)/2,18,13,C_TEXT);
}

// ── Report popup ──────────────────────────────────────────────────────────────
// Full-window overlay showing the detailed score breakdown with per-tile log.

static const char *tile_name(CellType t){
    switch(t){
        case CELL_TRAP_SPIKE:  return "Spike Trap";
        case CELL_TRAP_FREEZE: return "Freeze Zone";
        case CELL_TRAP_TELE:   return "Vortex Trap";
        case CELL_BONUS_BOMB:  return "Bomb (wall open)";
        case CELL_BONUS_SPEED: return "Speed Boost";
        case CELL_BONUS_COIN:  return "Gold Coin";
        default:               return "Unknown";
    }
}

static Color tile_color(CellType t){
    switch(t){
        case CELL_TRAP_SPIKE:  return C_SPIKE;
        case CELL_TRAP_FREEZE: return C_FREEZE;
        case CELL_TRAP_TELE:   return C_TELE;
        case CELL_BONUS_BOMB:  return C_BOMB;
        case CELL_BONUS_SPEED: return C_SPEED;
        case CELL_BONUS_COIN:  return C_COIN;
        default:               return C_TEXT;
    }
}

void render_report_popup(const SearchResult *res, SearchAlgo algo, bool *show) {
    // dim the background
    DrawRectangle(0, 0, WIN_W, WIN_H, CLITERAL(Color){0,0,0,170});

    // popup panel
    int pw=580, ph=520;
    int px=(WIN_W-pw)/2, py=(WIN_H-ph)/2;

    DrawRectangle(px,py,pw,ph,C_POPUP_BG);
    DrawRectangleLinesEx((Rectangle){px,py,pw,ph},2,C_POPUP_BD);
    // inner border for style
    DrawRectangleLinesEx((Rectangle){px+4,py+4,pw-8,ph-8},1,
                         CLITERAL(Color){60,80,120,120});

    const char *an[]={"BFS","DFS","Dijkstra","A*"};
    char title[64];
    snprintf(title,sizeof title,"Search Report — %s", an[algo]);
    int tw=MeasureText(title,20);
    DrawText(title, px+(pw-tw)/2, py+14, 20, C_ACCENT);
    DrawLine(px+20,py+42,px+pw-20,py+42,C_DIV);

    int x=px+24, y=py+52, rw=pw-48;
    char buf[128];

    // ── Summary row ──────────────────────────────────────────────────────────
    DrawTextEx(GetFontDefault(),"Summary",(Vector2){x,y},14,1,C_ACCENT); y+=18;

    // two-column mini table
    snprintf(buf,sizeof buf,"Cells explored: %d",res->visited);
    DrawTextEx(GetFontDefault(),buf,(Vector2){x,y},13,1,C_TEXT);
    snprintf(buf,sizeof buf,"Path length: %d steps",res->pathLen);
    DrawTextEx(GetFontDefault(),buf,(Vector2){x+rw/2,y},13,1,C_TEXT); y+=17;

    snprintf(buf,sizeof buf,"Time: %.3f ms",res->timeMs);
    DrawTextEx(GetFontDefault(),buf,(Vector2){x,y},13,1,C_TEXT);
    snprintf(buf,sizeof buf,"Walls opened by bomb: %d",res->walls_opened);
    DrawTextEx(GetFontDefault(),buf,(Vector2){x+rw/2,y},13,1,C_TEXT); y+=20;

    DrawLine(x,y,x+rw,y,C_DIV); y+=10;

    // ── Tile-by-tile log ─────────────────────────────────────────────────────
    DrawTextEx(GetFontDefault(),"Tiles on path",(Vector2){x,y},14,1,C_ACCENT); y+=18;

    if (res->event_count==0) {
        DrawTextEx(GetFontDefault(),"  No traps or bonuses on this path — clean run!",
                   (Vector2){x,y},13,1,C_GOOD); y+=18;
    } else {
        // header
        DrawTextEx(GetFontDefault(),"Tile",(Vector2){x,y},12,1,C_DIM);
        DrawTextEx(GetFontDefault(),"Position",(Vector2){x+190,y},12,1,C_DIM);
        DrawTextEx(GetFontDefault(),"Effect",(Vector2){x+280,y},12,1,C_DIM);
        DrawTextEx(GetFontDefault(),"Score delta",(Vector2){x+380,y},12,1,C_DIM);
        y+=15;
        DrawLine(x,y,x+rw,y,CLITERAL(Color){50,55,80,255}); y+=5;

        // list events (cap display at 12 to fit the popup)
        int show_n = res->event_count > 12 ? 12 : res->event_count;
        for (int i=0;i<show_n;i++){
            const TileEvent *ev=&res->events[i];
            Color tc=tile_color(ev->type);

            // colour dot
            DrawRectangle(x,y+2,10,10,tc);

            // name
            DrawTextEx(GetFontDefault(),tile_name(ev->type),(Vector2){x+14,y},12,1,C_TEXT);

            // position
            snprintf(buf,sizeof buf,"(%d,%d)",ev->r,ev->c);
            DrawTextEx(GetFontDefault(),buf,(Vector2){x+190,y},12,1,C_DIM);

            // human-readable effect
            const char *fx="";
            switch(ev->type){
                case CELL_TRAP_SPIKE:  fx="Heavy damage";  break;
                case CELL_TRAP_FREEZE: fx="Slowed";        break;
                case CELL_TRAP_TELE:   fx="Detour";        break;
                case CELL_BONUS_BOMB:  fx="Walls opened";  break;
                case CELL_BONUS_SPEED: fx="Speed boost";   break;
                case CELL_BONUS_COIN:  fx="Collected";     break;
                default: fx=""; break;
            }
            DrawTextEx(GetFontDefault(),fx,(Vector2){x+280,y},12,1,C_TEXT);

            // delta — bomb shows wall count, others show score delta
            if(ev->type==CELL_BONUS_BOMB) {
                snprintf(buf,sizeof buf,"%d wall%s",ev->delta,ev->delta!=1?"s":"");
                DrawTextEx(GetFontDefault(),buf,(Vector2){x+390,y},12,1,C_BOMB);
            } else if(ev->delta>0) {
                snprintf(buf,sizeof buf,"+%d",ev->delta);
                DrawTextEx(GetFontDefault(),buf,(Vector2){x+390,y},12,1,C_BAD);
            } else if(ev->delta<0) {
                snprintf(buf,sizeof buf,"-%d",-ev->delta);
                DrawTextEx(GetFontDefault(),buf,(Vector2){x+390,y},12,1,C_GOOD);
            } else {
                DrawTextEx(GetFontDefault(),"  —",(Vector2){x+390,y},12,1,C_DIM);
            }

            y+=16;
        }
        if (res->event_count>12){
            snprintf(buf,sizeof buf,"  ... and %d more tiles",res->event_count-12);
            DrawTextEx(GetFontDefault(),buf,(Vector2){x,y},12,1,C_DIM); y+=16;
        }
    }

    y+=6;
    DrawLine(x,y,x+rw,y,C_DIV); y+=10;

    // ── Score formula breakdown ───────────────────────────────────────────────
    DrawTextEx(GetFontDefault(),"Score Formula",(Vector2){x,y},14,1,C_ACCENT); y+=18;

    snprintf(buf,sizeof buf,"Path length  %d  ×  5  =  %d",
             res->pathLen, res->pathLen*5);
    DrawTextEx(GetFontDefault(),buf,(Vector2){x,y},13,1,C_TEXT); y+=16;

    snprintf(buf,sizeof buf,"Penalties    %d  ×  3  =  +%d  (%d trap tile(s))",
             res->penalties, res->penalties*3, res->traps_hit);
    DrawTextEx(GetFontDefault(),buf,(Vector2){x,y},13,1,
               res->penalties>0?C_BAD:C_TEXT); y+=16;

    snprintf(buf,sizeof buf,"Bonuses      %d  ×  2  =  -%d  (%d bonus tile(s))",
             res->bonuses, res->bonuses*2, res->bonuses_hit);
    DrawTextEx(GetFontDefault(),buf,(Vector2){x,y},13,1,
               res->bonuses>0?C_GOOD:C_TEXT); y+=16;

    DrawLine(x,y,x+rw,y,C_DIV); y+=8;

    snprintf(buf,sizeof buf,"%d + %d - %d  =  %d",
             res->pathLen*5, res->penalties*3, res->bonuses*2, res->final_score);
    DrawTextEx(GetFontDefault(),buf,(Vector2){x,y},13,1,C_DIM); y+=18;

    // final score big display
    snprintf(buf,sizeof buf,"FINAL SCORE:  %d",res->final_score);
    int stw=MeasureText(buf,20);
    DrawRectangle(px+20,y,pw-40,32,CLITERAL(Color){20,20,45,255});
    DrawRectangleLinesEx((Rectangle){px+20,y,pw-40,32},2,C_ACCENT);
    DrawText(buf,px+(pw-stw)/2,y+6,20,C_ACCENT);
    y+=40;

    DrawTextEx(GetFontDefault(),"lower score = better route",
               (Vector2){px+(pw-MeasureText("lower score = better route",11))/2,y},
               11,1,C_DIM);

    // close button
    int bw=160,bh=30;
    int bx=px+(pw-bw)/2, by=py+ph-44;
    bool hover=CheckCollisionPointRec(GetMousePosition(),
               (Rectangle){bx,by,bw,bh});
    DrawRectangle(bx,by,bw,bh,hover?CLITERAL(Color){60,80,140,255}
                                    :CLITERAL(Color){35,40,80,255});
    DrawRectangleLinesEx((Rectangle){bx,by,bw,bh},1,C_ACCENT);
    int ctw=MeasureText("Close  [TAB]",13);
    DrawText("Close  [TAB]",bx+(bw-ctw)/2,by+8,13,C_TEXT);

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        *show = false;
}
