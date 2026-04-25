#include "maze.h"
#include "raylib.h"

bool button(int x, int y, int w, int h, const char *label, bool active) {
    Rectangle r={x,y,w,h};
    bool hover=CheckCollisionPointRec(GetMousePosition(),r);
    bool clicked=hover&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    Color bg    =active?CLITERAL(Color){80,160,255,255}:hover?CLITERAL(Color){50,55,90,255}:CLITERAL(Color){30,33,55,255};
    Color border=active?CLITERAL(Color){130,200,255,255}:CLITERAL(Color){60,65,100,255};
    Color txt   =active?CLITERAL(Color){10,10,20,255}:CLITERAL(Color){200,210,230,255};
    DrawRectangleRec(r,bg);
    DrawRectangleLinesEx(r,active?2:1,border);
    int tw=MeasureText(label,13);
    DrawText(label,x+(w-tw)/2,y+(h-13)/2,13,txt);
    return clicked;
}
