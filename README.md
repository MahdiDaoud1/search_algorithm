# Treasure Hunt — Graph Algorithm Showdown

### University Data Structures Project — C + Raylib

A real-time maze visualizer and research tool that compares four pathfinding algorithms under identical conditions. Trap and bonus tiles scattered across the maze create a scoring system that reveals genuine differences between algorithms beyond simple path length.

---

## Features

### 🏗️ Maze Generators (6 algorithms)

| Key | Algorithm             | Type       | Characteristics                                |
| --- | --------------------- | ---------- | ---------------------------------------------- |
| `1` | Recursive Backtracker | Perfect    | Long winding corridors, few branches           |
| `2` | Prim's                | Perfect    | Branchy, short dead-ends, grows outward        |
| `3` | Kruskal's             | Perfect    | Uniform random texture, Union-Find based       |
| `4` | Aldous-Broder         | Perfect    | Statistically fairest spanning tree, slow      |
| `5` | Braided ★             | **Cyclic** | **Multiple valid paths** — best for comparison |
| `6` | Wilson's              | Perfect    | Loop-erased random walk, varied texture        |

> ★ **Used Braided for research** — it's the only generator with multiple paths between points, forcing algorithms to make real routing decisions around traps.

### 🔍 Search Algorithms (4 algorithms)

| Key | Algorithm | Trap-aware | Guarantee                                |
| --- | --------- | ---------- | ---------------------------------------- |
| `Q` | BFS       | ❌ No      | Shortest path by step count              |
| `W` | DFS       | ❌ No      | Any path — unpredictable                 |
| `E` | Dijkstra  | ✅ Yes     | Least-cost path (avoids heavy traps)     |
| `R` | A\*       | ✅ Yes     | Least-cost path with Manhattan heuristic |

BFS and DFS treat all cells as equal cost — they are blind to traps. Dijkstra and A\* add a penalty weight to trap cells and can route around them when the detour is worth it.

### 💀 Trap & Bonus Tiles

| Tile   | Score effect | Behaviour                                       |
| ------ | ------------ | ----------------------------------------------- |
| Spike  | +80 penalty  | Heavy damage                                    |
| Freeze | +50 penalty  | Movement slowed                                 |
| Vortex | +65 penalty  | Teleport detour                                 |
| Bomb   | neutral      | Opens all adjacent bridging walls during search |
| Speed  | −30 bonus    | Speed boost                                     |
| Coin   | −15 bonus    | Treasure collected                              |

### 📊 Scoring Formula

```
final_score = pathLen × 5 + penalties × 3 − bonuses × 2
```

**Lower score = better algorithm.** A\* does not automatically win — a geometrically short path through spike traps can score worse than a longer clean route found by Dijkstra.

### 🎮 Controls

| Key         | Action                                                      |
| ----------- | ----------------------------------------------------------- |
| `G`         | Generate new maze                                           |
| `1`–`6`     | Select generator                                            |
| `Q/W/E/R`   | Select search algorithm                                     |
| `S`         | Start search                                                |
| `X`         | Reset search (keep maze + bomb craters)                     |
| `Z`         | Full reset (undo bombs, clear start/end, keep maze + traps) |
| `TAB` / `H` | Toggle detailed score report popup                          |
| `ESC`       | Return to menu                                              |
| Left click  | Place start (odd clicks) / end (even clicks)                |

---

## Building

### Prerequisites

Install [Raylib](https://www.raylib.com/) (v4.x or v5.x):

**Ubuntu / Debian:**

```bash
sudo apt install libraylib-dev
# or build from source: https://github.com/raysan5/raylib
```

**macOS (Homebrew):**

```bash
brew install raylib
```

**Windows (MSYS2 / MinGW):**

```bash
pacman -S mingw-w64-x86_64-raylib
```

### Compile & Run

```bash
make
./maze_search.exe      # Windows
./maze_search          # Linux / macOS
```

---

## Icons (optional)

Place PNG icons (64×64, transparent background) in the `assets/` folder:

| Filename     | Tile                        |
| ------------ | --------------------------- |
| `start.png`  | Explorer / start marker     |
| `end.png`    | Treasure chest / end marker |
| `spike.png`  | Spike trap                  |
| `freeze.png` | Freeze zone                 |
| `vortex.png` | Vortex trap                 |
| `bomb.png`   | Bomb                        |
| `speed.png`  | Speed boost                 |
| `coin.png`   | Gold coin                   |

If a file is missing the tile still renders with its colour — no crash.

---

## Project Structure

```
maze_search/
├── assets/             # PNG icons (optional)
├── include/
│   └── maze.h          # All types, enums, constants, prototypes
├── src/
│   ├── main.c          # Main loop, input, state machine
│   ├── grid.c          # Grid init, clear_search, full_reset
│   ├── generators.c    # 6 maze generation algorithms
│   ├── traps.c         # Trap placement, scoring helpers
│   ├── search.c        # BFS / DFS / Dijkstra / A* step iterators
│   ├── render.c        # Raylib rendering + score report popup
│   └── ui.c            # Button widget
└── Makefile
```

---

## Data Structures

| Purpose                 | Structure                                                      |
| ----------------------- | -------------------------------------------------------------- |
| Graph                   | Implicit 2D grid — `cells[ROWS][COLS]` (CellType)              |
| Search overlay          | `vis[ROWS][COLS]` (VisState) — separate from logical state     |
| BFS frontier            | Circular FIFO queue                                            |
| DFS frontier            | Dynamic LIFO stack                                             |
| Dijkstra / A\* frontier | Binary min-heap (priority queue on cost `f`)                   |
| Kruskal's connectivity  | Union-Find with path compression                               |
| Bomb craters            | Explicit position list (`craters[]`) — survives vis overwrites |

The grid uses a **room-and-wall encoding**: rooms sit at odd coordinates, wall cells sit at even coordinates between them. Generators carve passages by opening the wall cell between two rooms.

---

## Algorithm Comparison

|                      | BFS                         | DFS                  | Dijkstra                  | A\*                                  |
| -------------------- | --------------------------- | -------------------- | ------------------------- | ------------------------------------ |
| Trap awareness       | ❌                          | ❌                   | ✅                        | ✅                                   |
| Optimal path         | ✅ (steps)                  | ❌                   | ✅ (cost)                 | ✅ (cost)                            |
| Exploration pattern  | Radial wave                 | Deep dive            | Cost-ordered              | Goal-directed                        |
| Best score scenario  | Clean maze, no traps        | Lucky run on braided | Trap-heavy maze           | Fast + trap-aware                    |
| Worst score scenario | Shortest path through traps | Long winding path    | Same as BFS on clean maze | Heuristic pulls through trap cluster |

The most interesting finding: **Dijkstra can outperform A\* on final score** in trap-heavy mazes because A\*'s heuristic pulls it toward the goal even through dangerous areas, while Dijkstra evaluates cost more conservatively.
