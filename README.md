# Treasure Hunt — Graph Algorithm Showdown

> University project — Data Structures (IN104)  
> ENSTA Paris  
> Language: C · Graphics: Raylib

A real-time maze visualizer and research tool that compares five pathfinding algorithms under identical conditions. Trap and bonus tiles scattered across the maze introduce a scoring system that reveals genuine differences between algorithms beyond simple path length — a geometrically short path through spike traps can score worse than a longer clean route.

---

## Screenshots

> Generate a maze with `G`, place start and end by clicking, press `S` to watch the algorithm search, press `H` for the full score report.

---

## Build

### Windows — MSYS2 UCRT64
```bash
pacman -S mingw-w64-ucrt-x86_64-raylib
cd maze_search
make
./maze_search.exe
```

### Linux
```bash
sudo apt install libraylib-dev
make
./maze_search
```

### macOS
```bash
brew install raylib
make
./maze_search
```

---

## Controls (AZERTY)

| Key | Action |
|-----|--------|
| `G` | Generate new maze |
| `1` – `6` | Select generator |
| `A` | Algorithm: BFS |
| `Z` | Algorithm: DFS |
| `E` | Algorithm: Dijkstra |
| `R` | Algorithm: A\* |
| `T` | Algorithm: Bidirectional Dijkstra |
| `S` | Start search |
| `X` | Reset search (keeps maze + bomb craters) |
| `W` | Full reset (restores bomb walls, clears start/end, keeps traps) |
| `H` / `TAB` | Toggle detailed score report popup |
| `ESC` | Return to menu |
| Left click | Place START on first click, END on second, alternates |

---

## Maze Generators

| Key | Name | Type | Characteristics |
|-----|------|------|-----------------|
| `1` | Recursive Backtracker | Perfect | Long winding corridors, few branches |
| `2` | Prim's | Perfect | Branchy, short dead-ends, spreads outward |
| `3` | Kruskal's | Perfect | Uniform texture, Union-Find based |
| `4` | Aldous-Broder | Perfect | Statistically fairest spanning tree |
| `5` | **Braided** ★ | **Cyclic** | **Multiple valid paths — best for research** |
| `6` | Wilson's | Perfect | Loop-erased random walk, varied texture |

> ★ Use **Braided** for meaningful algorithm comparison — it is the only generator that creates cycles (multiple paths between points), forcing algorithms to make real routing decisions around traps.
>
> All other generators produce a **perfect maze** (spanning tree): exactly one path between any two points. On a perfect maze every algorithm finds the same route — only exploration efficiency differs.

---

## Search Algorithms

| Key | Algorithm | Trap-aware | Path guarantee | Data structure |
|-----|-----------|------------|----------------|----------------|
| `A` | BFS | ❌ | Shortest (steps) | Linked-list queue (FIFO) |
| `Z` | DFS | ❌ | Any path | Linked-list stack (LIFO) |
| `E` | Dijkstra | ✅ | Least cost | Binary min-heap |
| `R` | A\* | ✅ | Least cost | Binary min-heap + Manhattan heuristic |
| `T` | Bidirectional Dijkstra | ✅ | Least cost | Two binary min-heaps |

**BFS and DFS are intentionally trap-blind** — they treat every open cell as cost 1. This is the key design decision that makes the comparison meaningful: BFS will walk straight through a spike cluster while Dijkstra routes around it.

**Bidirectional Dijkstra** runs two simultaneous searches — one from start, one from end — and stops when their frontiers meet. Visits roughly half the cells of regular Dijkstra.

---

## Trap & Bonus Tiles

| Tile | Score effect | Description |
|------|-------------|-------------|
| 💀 Spike | **+80 penalty** | Heavy damage |
| 🧊 Freeze | **+50 penalty** | Movement slowed |
| 🌀 Vortex | **+65 penalty** | Teleport detour |
| 💣 Bomb | neutral | Opens all 8 adjacent wall cells during search |
| ⚡ Speed | **−30 bonus** | Speed boost |
| 🪙 Coin | **−15 bonus** | Treasure collected |

---

## Scoring Formula

```
final_score = pathLen × 5  +  penalties × 3  −  bonuses × 2  +  time(ms) × 0.5
```

**Lower score = better algorithm.**

| Component | Weight | Meaning |
|-----------|--------|---------|
| Path length | ×5 | Route quality dominates |
| Penalties | ×3 | One spike (+80×3=240) justifies a detour |
| Bonuses | ×2 | Reward for collecting without dominating |
| Time | ×0.5 | Penalises slow algorithms |

---

## Data Structures

### Queue — linked list (FIFO) — BFS
```c
typedef struct QueueNode {
    Node             data;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *head;  // dequeue from here — O(1)
    QueueNode *tail;  // enqueue here    — O(1)
    int        size;
} Queue;
```
Each push allocates one node, each pop frees it. No array, no reallocation.

### Stack — linked list (LIFO) — DFS
```c
typedef struct StackNode {
    Node             data;
    struct StackNode *next;
} StackNode;

typedef struct {
    StackNode *top;
    int        size;
} Stack;
```
Push prepends to the head, pop removes the head. O(1) both ways.

### Min-Heap — fixed array — Dijkstra / A\* / BiDir Dijkstra
```c
typedef struct {
    Node data[ROWS * COLS];  // maximum possible nodes: 961 — allocated once, never resized
    int  size;
} MinHeap;
```
A binary heap **requires** array storage because the parent/child index formulas (`parent = (i-1)/2`, `children = 2i+1 / 2i+2`) need O(1) random access — impossible with a linked list. The maximum size is `ROWS × COLS = 961`, known at compile time, so no reallocation is ever needed.

### Union-Find — Kruskal's generator
```c
static int parent[ROWS * COLS];

int find(int x) {
    return parent[x] == x ? x : (parent[x] = find(parent[x]));
}
```
Path compression makes repeated `find` calls nearly O(1). Used to detect whether two rooms are already connected before removing a wall.

### Grid — two parallel 2D arrays
```c
typedef struct {
    CellType cells[ROWS][COLS];  // permanent: wall / open / trap / bonus
    VisState vis[ROWS][COLS];    // search overlay: visited / frontier / path / exploded
    CraterPos craters[MAX_CRATERS];
    int       crater_count;
    int startR, startC, endR, endC;
} Grid;
```
Two arrays with different responsibilities. `cells[][]` is the ground truth — generators and traps write here, search never changes it (except bomb openings). `vis[][]` is the rendering overlay — cleared between runs without touching traps. This separation is what makes trap persistence across search resets work correctly.

---

## Project Structure

```
maze_search/
├── assets/             # PNG icons (optional — see assets/README.txt)
├── include/
│   └── maze.h          # All types, enums, constants, prototypes
├── src/
│   ├── main.c          # Main loop, input handling, state machine
│   ├── grid.c          # Grid init / clear_search / full_reset
│   ├── generators.c    # 6 maze generation algorithms
│   ├── traps.c         # Trap placement, score helpers
│   ├── search.c        # 5 search algorithms + data structures
│   ├── render.c        # Raylib rendering + score report popup
│   └── ui.c            # Button widget
└── Makefile
```

---

## Key Research Finding

> **Dijkstra can outperform A\* on final score in trap-heavy mazes.**
>
> A\*'s Manhattan heuristic pulls it toward the goal even through clusters of spike traps.
> Dijkstra evaluates cost more conservatively and will take longer detours to avoid heavy penalties.
> On clean mazes A\* dominates. On braided trap-heavy mazes the rankings become unpredictable —
> which is exactly what makes the comparison interesting.

---

## Icons (optional)

Place 64×64 PNG files with transparent backgrounds in `assets/`:

| File | Tile |
|------|------|
| `start.png` | Start marker |
| `end.png` | End / treasure marker |
| `spike.png` | Spike trap |
| `freeze.png` | Freeze zone |
| `vortex.png` | Vortex trap |
| `bomb.png` | Bomb |
| `speed.png` | Speed boost |
| `coin.png` | Gold coin |

If a file is missing the tile still renders with its colour — no crash.
