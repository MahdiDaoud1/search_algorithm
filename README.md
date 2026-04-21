# Maze Search Visualizer
### Graph Algorithm Comparison — University Data Structures Project

A real-time visualization tool written in **C** using **Raylib** that lets you compare different search algorithms on procedurally generated mazes.

---

## Features

### 🏗️ Maze Generators (4 algorithms)
| Key | Algorithm | Characteristics |
|-----|-----------|-----------------|
| `1` | Recursive Backtracker (DFS) | Long winding corridors, low branching |
| `2` | Prim's (Randomised) | More branching, shorter dead-ends |
| `3` | Kruskal's (Randomised) | Uniform spanning tree, random texture |
| `4` | Aldous-Broder | Truly uniform random spanning tree (slow to generate) |

### 🔍 Search Algorithms (4 algorithms)
| Key | Algorithm | Guarantee |
|-----|-----------|-----------|
| `Q` | BFS       | Shortest path (unweighted) |
| `W` | DFS       | Any path, not necessarily shortest |
| `E` | Dijkstra  | Shortest path (uniform cost = same as BFS here) |
| `R` | A\*       | Shortest path with Manhattan heuristic (fastest) |

### 🎮 Interaction
- **Click** on any open cell to set Start (green) and End (red)
- Press **S** to begin the search animation
- Press **G** to generate a new maze
- Press **ESC** to return to the main menu

### 📊 Live Statistics
- Cells visited
- Path length
- Execution time (ms)

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
./maze_search
```

---

## Project Structure
```
maze_search/
├── include/
│   └── maze.h          # All types, constants, prototypes
├── src/
│   ├── main.c          # App state machine, input handling
│   ├── grid.c          # Grid init & search-state reset
│   ├── generators.c    # 4 maze generation algorithms
│   ├── search.c        # BFS / DFS / Dijkstra / A* (step iterators)
│   ├── render.c        # Raylib rendering (grid + sidebar + topbar)
│   └── ui.c            # Button widget helper
└── Makefile
```

---

## Data Structure Used
- **2D Grid** (`Grid.cells[ROWS][COLS]`) — the graph is an implicit grid graph
- **Adjacency**: 4-connected (N/S/E/W), no diagonal movement
- **BFS**: FIFO queue
- **DFS**: LIFO stack
- **Dijkstra / A\***: binary min-heap (priority queue)
- **Kruskal's**: Union-Find (path compression + union by rank)

---

## Visual Legend
| Color | Meaning |
|-------|---------|
| 🟢 Green | Start cell |
| 🔴 Red | End cell |
| 🔵 Blue | Visited |
| 💙 Light blue | Frontier (in queue/stack) |
| 🟡 Yellow | Final path |
| ⬛ Dark | Wall |
| ⬜ Light | Open passage |
