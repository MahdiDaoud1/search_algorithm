# Treasure Hunt — Graph Algorithm Showdown

> University project — Data Structures (IN104)  
> ENSTA Paris  
> Language: C · Graphics: Raylib · Data structures: libin103

A real-time maze visualizer that compares five pathfinding algorithms under identical conditions. Trap and bonus tiles scattered across the maze create a scoring system that reveals genuine differences between algorithms — a geometrically short path through spike traps can score worse than a longer route that collects bonuses and avoids penalties.

---

## Build

### Requirements

- [Raylib](https://www.raylib.com/)
- [libin103](https://perso.ensta.fr/~chapoutot/teaching/in103/) — set `LIBIN103` path in Makefile

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

> The `LIBIN103` variable in the Makefile should point to your libin103 installation folder, e.g. `~/Library/libin103-1.4.2`.

---

## Controls (AZERTY)

| Key         | Action                                                      |
| ----------- | ----------------------------------------------------------- |
| `G`         | Generate new maze                                           |
| `1` – `6`   | Select generator                                            |
| `A`         | Algorithm: BFS                                              |
| `Z`         | Algorithm: DFS                                              |
| `E`         | Algorithm: Dijkstra                                         |
| `R`         | Algorithm: A\*                                              |
| `T`         | Algorithm: Bidirectional Dijkstra                           |
| `S`         | Start search                                                |
| `X`         | Reset search — keeps maze, traps, bomb craters              |
| `W`         | Full reset — restores bomb walls, keeps traps and start/end |
| `H` / `TAB` | Toggle detailed score report                                |
| `ESC`       | Return to menu                                              |
| Left click  | Odd clicks place START, even clicks place END               |

---

## Maze Generators

| Key | Name                  | Type       | Characteristics                              |
| --- | --------------------- | ---------- | -------------------------------------------- |
| `1` | Recursive Backtracker | Perfect    | Long winding corridors, few branches         |
| `2` | Prim's                | Perfect    | Branchy, short dead-ends, spreads outward    |
| `3` | Kruskal's             | Perfect    | Uniform texture, Union-Find based            |
| `4` | Aldous-Broder         | Perfect    | Statistically fairest spanning tree          |
| `5` | **Braided** ★         | **Cyclic** | **Multiple valid paths — best for research** |
| `6` | Wilson's              | Perfect    | Loop-erased random walk, varied texture      |

> ★ **Use Braided** for meaningful algorithm comparison — it is the only generator that creates cycles, forcing algorithms to make real routing decisions around traps.
>
> All other generators produce a **perfect maze** (spanning tree): exactly one path between any two points. On a perfect maze every algorithm finds the same route — only exploration efficiency differs.

---

## Search Algorithms

| Key | Algorithm              | Trap-aware | Path guarantee    | Data structure                         |
| --- | ---------------------- | ---------- | ----------------- | -------------------------------------- |
| `A` | BFS                    | ❌         | Shortest by steps | `generic_queue_t` (libin103)           |
| `Z` | DFS                    | ❌         | Any path          | `generic_stack_t` (libin103)           |
| `E` | Dijkstra               | ✅         | Least cost        | `generic_heap_t` (libin103)            |
| `R` | A\*                    | ✅         | Least cost        | `generic_heap_t` + Manhattan heuristic |
| `T` | Bidirectional Dijkstra | ✅         | Least cost        | Two `generic_heap_t` instances         |

**BFS and DFS are intentionally trap-blind** — they treat every open cell as uniform cost 1. Dijkstra and A\* weight cells by their trap penalty, actively routing around heavy traps and toward bonuses when the detour is worth it.

**Bidirectional Dijkstra** runs two simultaneous searches — one from start, one from end — stopping when their frontiers meet. Visits roughly half the cells of regular Dijkstra.

---

## Trap & Bonus Tiles

| Tile   | Score effect | Edge cost effect           | Description                              |
| ------ | ------------ | -------------------------- | ---------------------------------------- |
| Spike  | +80 penalty  | +5.3 cost                  | Heavy damage — Dijkstra/A\* avoids       |
| Freeze | +50 penalty  | +3.3 cost                  | Movement slowed                          |
| Vortex | +65 penalty  | +4.3 cost                  | Teleport detour                          |
| Bomb   | neutral      | neutral                    | Opens all 8 adjacent walls during search |
| Speed  | −30 bonus    | −1.0 cost                  | Dijkstra/A\* actively seeks it           |
| Coin   | −15 bonus    | −0.0 cost (clamped to 0.1) | Dijkstra/A\* routes toward it            |

Trap/bonus tiles influence Dijkstra and A\* routing through their edge cost. BFS and DFS always use cost 1 regardless of tile type — this is the core difference the project demonstrates.

---

## Scoring Formula

```
final_score = pathLen × 5  +  penalties × 3  −  bonuses × 2  +  time(ms) × 0.5
```

**Lower score = better algorithm.**

| Component   | Weight | Rationale                                           |
| ----------- | ------ | --------------------------------------------------- |
| Path length | ×5     | Route quality dominates the score                   |
| Penalties   | ×3     | One spike (80×3=240) justifies a significant detour |
| Bonuses     | ×2     | Reward for collecting without dominating            |
| Time        | ×0.5   | Penalises slow algorithms on equal paths            |

---

## Data Structures

All search data structures use **libin103**'s generic implementations.

### Queue — `generic_queue_t` — BFS

FIFO structure. Nodes are enqueued at the tail and dequeued from the head. BFS explores cells in wave order — nearest cells first — guaranteeing the shortest path by step count.

### Stack — `generic_stack_t` — DFS

LIFO structure. Nodes are pushed and popped from the top. DFS dives deep along one corridor before backtracking — finds a path quickly but not necessarily the shortest.

### Min-Heap — `generic_heap_t` — Dijkstra / A\* / BiDir Dijkstra

Priority queue ordered by node score (lowest first). The `node_compare` callback inverts the natural ordering since libin103's heap is a max-heap by convention — returning +1 for lower scores pushes them to the root.

### Union-Find — `integer_uf_t` — Kruskal's generator

Path compression makes repeated `integer_uf_are_connected` calls nearly O(1). Used to detect whether two rooms are already connected before removing a wall between them.

### Grid — two parallel 2D arrays

```c
typedef struct {
    CellType  cells[ROWS][COLS];       // permanent: wall / open / trap / bonus
    VisState  vis[ROWS][COLS];         // search overlay: visited / frontier / path
    CraterPos craters[MAX_CRATERS];    // bomb-opened walls for full reset
    int       crater_count;
    int startR, startC, endR, endC;
} Grid;
```

`cells[][]` is the permanent ground truth — written by generators and traps, never cleared between searches. `vis[][]` is the rendering overlay — cleared between runs, leaving trap tiles intact. This separation is what makes traps persist correctly across multiple algorithm runs on the same maze.

---

## Project Structure

```
maze_search/
├── assets/             # PNG icons (optional — see assets/README.txt)
├── include/
│   └── maze.h          # All types, enums, constants, prototypes
├── src/
│   ├── main.c          # Main loop, input handling, state machine
│   ├── grid.c          # grid_init / grid_clear_search / grid_full_reset
│   ├── generators.c    # 6 maze generation algorithms
│   ├── traps.c         # Trap placement, score helpers
│   ├── search.c        # 5 search algorithms, data structures, bomb effect
│   ├── render.c        # Raylib rendering, score report popup
│   └── ui.c            # Button widget
└── Makefile
```

---

## Key Research Findings

**Dijkstra vs A\*** — On trap-heavy mazes, Dijkstra can outperform A\* on final score. A\*'s Manhattan heuristic pulls it toward the goal even through trap clusters. Dijkstra evaluates cost more conservatively and takes longer detours to avoid heavy penalties.

**BFS vs Dijkstra** — On the same maze with no traps, both find identical paths. The moment traps appear, Dijkstra routes around them while BFS walks straight through — the score gap reveals exactly how costly the traps were.

**DFS** — Unpredictable. On braided mazes with many alternative routes, DFS occasionally finds a clean short path and wins. On perfect mazes it almost always loses. Its variance across runs is itself a research data point.

**Bidirectional Dijkstra** — Consistently visits fewer cells than regular Dijkstra. On long paths the reduction is dramatic. However path quality depends on where the two frontiers meet, which can occasionally produce a slightly suboptimal result compared to full Dijkstra.

---

## Icons (optional)

Place 64×64 PNG files with transparent backgrounds in `assets/`:

| File         | Tile                  |
| ------------ | --------------------- |
| `start.png`  | Start marker          |
| `end.png`    | Treasure / end marker |
| `spike.png`  | Spike trap            |
| `freeze.png` | Freeze zone           |
| `vortex.png` | Vortex trap           |
| `bomb.png`   | Bomb                  |
| `speed.png`  | Speed boost           |
| `coin.png`   | Gold coin             |

If a file is missing the tile still renders with its colour — no crash.
