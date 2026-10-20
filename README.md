# Traffic Signal Optimization Simulator (C++ backend)

All traffic logic is written in C++. The web page only displays what the C++ program computes.
The UI, scenarios and results are the same as the JavaScript version.

```
backend/src/     C++ logic + a small built-in web server   (start reading at docs/CODE_GUIDE.pdf)
backend/tests/   C++ tests (19 tests, 62 checks)
frontend/        HTML, CSS, JavaScript: display and buttons only
docs/            CODE_GUIDE.pdf, a walkthrough of the C++ for explaining it
```

No libraries to install. You only need a C++17 compiler.

## Build and run

### Windows: quickest way

Double-click **run.bat**. It builds `server.exe` the first time (needs g++), starts it in its own window, and opens http://localhost:8080. Close the server window to stop.

Do not double-click `frontend/index.html`: opened as a file, the page cannot reach the C++ server and stays empty.

### Windows by hand (MinGW / MSYS2 g++)

From the project folder:

```
g++ -std=c++17 -O2 -o server.exe backend/src/main.cpp -lws2_32
server.exe
```

Open **http://localhost:8080**. Keep the terminal open while using the page; Ctrl+C stops it.

Tests:

```
g++ -std=c++17 -O2 -I backend/src -o tests.exe backend/tests/tests.cpp
tests.exe
```

Visual Studio (Developer Command Prompt): `cl /std:c++17 /EHsc /O2 backend\src\main.cpp /Fe:server.exe`

### Linux / macOS

```
make          # builds ./server and ./tests
./server      # then open http://localhost:8080
./tests
```

If port 8080 is busy: `server 8081`. If you run the server from another folder, pass the frontend path: `server 8080 path/to/frontend`.

Opening `frontend/index.html` directly does not work: the page needs the C++ server to get its data.

## How the two halves talk

| Request | C++ answers with |
|---|---|
| `GET /api/scenarios` | the two presets and default timing |
| `GET /api/simulate?strategy=greedy&seed=42&rN=0.35&...` | the full run: one frame per second, every decision with its explanation, final summary; or a list of errors for invalid settings |
| `GET /api/query?from=10&to=40&...` | segment-tree answers for that time window |
| anything else | files from `frontend/` |

Start, Pause, Step and Reset just move through the frames C++ already sent. Changing a setting, switching strategy, Optimize signals and Look up all ask C++ again.

## C++ files

| File | Role |
|---|---|
| `model.hpp` | data types: directions, phases, Timing, Scenario, Approach, Signal, Decision |
| `random.hpp` | seeded mulberry32 generator, Poisson arrivals, FNV hash |
| `scenarios.hpp` | Peak demo and Single-approach congestion; validation rules |
| `arrivals.hpp` | builds the full arrival schedule before the run (same for both strategies) |
| `priority_queue.hpp` | hand-written binary max-heap |
| `segment_tree.hpp` | range sum / range max, O(log n) |
| `controllers.hpp` | Fixed-time baseline and Greedy + priority queue optimizer |
| `engine.hpp` | one-second step, light state machine (green, yellow, all-red), safety clamps |
| `metrics.hpp` | average wait, served, residual queue, peaks |
| `json.hpp` | small JSON writer |
| `http_server.hpp` | socket server (Winsock on Windows, POSIX elsewhere) |
| `main.cpp` | reads settings from the URL, runs the simulation, writes JSON |

## Model in one paragraph

Each second: new cars arrive (Poisson draw, seeded), roads with green let up to their service rate through, every waiting car adds one second of delay, then the light's clock ticks. North+South share a phase, East+West share the other; every change goes through 3 s yellow and 2 s all-red. Greens stay between 10 and 60 s. Average wait = total waiting car-seconds / cars served (N/A if none).

## Optimize signals

Switches the controller from Fixed-time to Greedy + priority queue, rewinds to 0 s so the run uses the same cars as the baseline, and shows the first decision it will make and why. Press Start, then compare in "Baseline vs optimized".

## Results (default settings)

| Scenario | Avg wait fixed / greedy | Residual queue fixed / greedy |
|---|---|---|
| Peak demo | 87.7 s / 84.4 s | 94 / 88 |
| Single-approach congestion | 91.3 s / 44.9 s | 70 / 22 |

## Not implemented

DP planner and multi-intersection graph mode from the brief's advanced list.

## Limitations

Educational model only. One intersection, two phases, no turning lanes, pedestrians or start-up delay, constant arrival rates. Never use it to control real signals.
