#include "maze.h"
#include "raylib.h"

bool button(int x, int y, int w, int h, const char *label, bool active) {
    Rectangle r = { x, y, w, h };
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    bool clicked = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    Color bg   = active  ? (Color){80,160,255,255} :
                 hover   ? (Color){50, 55, 90,255} :
                           (Color){30, 33, 55,255};
    Color border = active ? (Color){130,200,255,255} : (Color){60,65,100,255};
    Color txt    = active ? (Color){10,10,20,255}    : (Color){200,210,230,255};

    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1, border);

    int tw = MeasureText(label, 13);
    DrawText(label, x + (w-tw)/2, y + (h-13)/2, 13, txt);
    return clicked;
}
