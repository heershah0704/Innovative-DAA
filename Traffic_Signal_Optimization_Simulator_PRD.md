# PRD: Traffic Signal Optimization Simulator

**Document type:** LLM implementation specification  
**Version:** 1.0  
**Project difficulty target:** 8/10  
**Primary deliverable:** A complete, runnable interactive web application  
**Product category:** Algorithm visualization / traffic simulation  
**Core distinction:** Optimize traffic-light timing, not vehicle routes.

---

## 1. Instructions to the Implementing LLM

Build the application described in this document as a complete, working project.

### Required behavior
- Implement the simulation logic and the UI; do not create only a static mockup.
- Follow the requirements and constraints in this PRD.
- Keep the project runnable with simple setup steps.
- Use real algorithm implementations for the features that claim to use them. Do not add decorative algorithm labels without functionality.
- Make the simulation deterministic when the same scenario seed and settings are used, so different signal strategies can be compared fairly.
- If a minor implementation detail is unspecified, choose a sensible default and document it rather than blocking implementation with questions.
- Do not claim that simulated results are real-world traffic predictions or that this is safe for deployment at real intersections.

### Delivery requirements
Provide:
1. Complete source code with a clear folder structure.
2. A README with setup and run instructions.
3. A working default scenario.
4. Tests for important simulation and algorithm rules.
5. A short explanation of how each required DAA concept is used.
6. No placeholder buttons or nonfunctional controls.

---

## 2. Product Summary

Create an interactive traffic signal optimization simulator. Vehicles arrive at the North, South, East, and West approaches of a four-way intersection at different rates. The application decides how signal green time should be allocated and shows how queues change as simulated time advances.

The user must be able to compare a fixed-time baseline controller with an optimized controller under the **same vehicle-arrival sequence**.

This project is not a route planner. It must not recommend paths or reroute vehicles.

### Core demonstration
The dashboard displays an intersection, current queues, current signal phase, timing information, and performance metrics. When the user clicks **OPTIMIZE SIGNALS**, the controller computes a valid signal plan. The user can then run the simulation and see queue lengths, signal states, and metrics update.

---

## 3. Goals

1. Make traffic-signal timing optimization visually understandable.
2. Show queues increasing as vehicles arrive and decreasing when vehicles are served.
3. Demonstrate meaningful use of data structures and algorithms.
4. Compare baseline and optimized strategies fairly.
5. Explain why a signal plan was selected.
6. Keep the MVP simple enough to run locally without external services.

### Success criteria
- All four approaches and their queue counts are visible.
- The Optimize Signals action produces a valid plan and an explanation.
- The simulation visibly updates queues and signal phases.
- Baseline and optimized runs use identical arrivals.
- Metrics are calculated from simulation state, not hardcoded.
- The user can start, pause, step, reset, and rerun scenarios.
- Every visible control works.

---

## 4. Scope

### MVP — required
- One four-way intersection.
- Four approach queues: North, South, East, West.
- Configurable arrival rates and service rates.
- Fixed-time baseline controller.
- Greedy green-time allocator.
- Priority Queue-based approach scheduling or ranking.
- Simulation clock and animated state updates.
- Queue bars and signal indicators.
- Before/after comparison metrics.
- Scenario presets and reset functionality.
- Algorithm explanation panel.
- Automated tests for core rules.

### Advanced features — implement if feasible after the MVP works
- Segment Tree for time-interval traffic queries.
- Graph representation for a network of intersections.
- Dynamic Programming planner for a bounded future horizon.
- Charts showing queue lengths over time and strategy comparisons.

Do not sacrifice correctness of the MVP to include advanced features. If an advanced feature is not completed, clearly mark it as unavailable rather than pretending it works.

### Out of scope
- Route optimization or navigation.
- Live traffic-camera feeds.
- Real-world traffic signal control.
- Cloud accounts or authentication.
- Claims of guaranteed congestion reduction in real cities.

---

## 5. Suggested Technology Stack

Unless the existing project environment requires otherwise, use:

- **Frontend:** HTML, CSS, and vanilla JavaScript.
- **Styling:** custom CSS; responsive layout; no build step required.
- **Charts:** use a lightweight chart library only if needed; otherwise draw simple charts with HTML/CSS or SVG.
- **Simulation engine:** JavaScript modules, separated from UI rendering.
- **Testing:** use a lightweight test setup compatible with the chosen project structure, or provide runnable test scripts.
- **Backend:** not required for the MVP.
- **Persistence:** optional local storage for preferences; simulation must work without it.

Avoid adding a backend, database, or heavy framework unless there is a concrete requirement. The project should be easy to launch locally.

---

## 6. Main User Journey

1. User opens the application and sees **INTERSECTION 4**.
2. User sees the four queues and the active green signal.
3. User chooses a scenario preset or edits arrival rates.
4. User runs the baseline simulation.
5. User resets to the same initial conditions and clicks **OPTIMIZE SIGNALS**.
6. The controller calculates a signal plan and explains its decision.
7. User runs the optimized simulation.
8. Queue counts and signal phases update as simulated time advances.
9. User compares average waiting time, throughput, residual queue, and maximum queue.
10. User can pause, step, reset, or change the scenario and repeat.

---

## 7. UI Specification

### 7.1 Overall layout

Build a modern, polished, responsive dashboard with a dark traffic-control-room aesthetic. Use a dark navy/charcoal background, readable text, restrained neon accents, clear status colors, and high-contrast queue bars.

The desktop layout should contain:

1. **Header**
   - Product name: `TRAFFIC SIGNAL OPTIMIZATION SIMULATOR`
   - Scenario selector.
   - Simulation status: Ready, Running, Paused, or Completed.
   - Reset control.

2. **Intersection visualization**
   - A clear four-way road crossing.
   - Labels for North, South, East, and West.
   - Queue/vehicle indicators on each approach.
   - Signal indicators showing red, yellow, or green.
   - Current phase and countdown timer.

3. **Queue cards**
   - One card for each approach.
   - Exact queue count.
   - Proportional bar.
   - Arrival rate and service rate.
   - Optional waiting-time indicator.

4. **Controller panel**
   - Current active phase.
   - Current green duration and remaining time.
   - Planned green duration for each approach or compatible phase.
   - Strategy selector.
   - Primary button: `OPTIMIZE SIGNALS`.

5. **Simulation controls**
   - Start / Resume.
   - Pause.
   - Step 1 second.
   - Reset.
   - Simulation speed selector, if practical.

6. **Performance metrics**
   - Average waiting time.
   - Vehicles served.
   - Total residual queue.
   - Maximum queue observed.
   - Optional phase-switch count.

7. **Algorithm inspector**
   - Active strategy.
   - Current scores/priorities.
   - Selected green-time allocations.
   - Human-readable reason for the decision.
   - Constraints applied to the plan.

8. **Comparison area**
   - Baseline vs optimized metrics.
   - Queue-over-time chart if implemented.
   - Absolute and percentage differences.
   - Clear statement that both runs use the same arrival sequence.

### 7.2 Initial example values

Use these as the default illustrative queue counts:

| Approach | Initial queue |
|---|---:|
| North | 82 cars |
| South | 21 cars |
| East | 51 cars |
| West | 13 cars |

The initial active green may be North with 25 seconds remaining. Make these values configurable.

These values are demo data, not live traffic measurements.

### 7.3 UI behavior
- Queue bars must reflect actual queue counts.
- Vehicle service must visibly reduce the queue when that movement has green.
- New arrivals may increase queues during any phase.
- Counts and metrics must update from the same simulation state used by the controller.
- Signal states must be communicated using labels or icons as well as color.
- Show a clear message when the optimized plan is ready.
- Disable or appropriately handle conflicting controls while the simulation is running.
- Reset must restore the initial state, timers, metrics, and reproducible arrival sequence.

---

## 8. Simulation Model

Use a simplified, documented discrete-time model. One simulation tick represents one second.

### 8.1 State

Maintain, at minimum:
- Current simulated time.
- Queue length for each approach.
- Arrival rate for each approach.
- Service capacity for each approach.
- Active signal phase.
- Time remaining in the phase.
- Transition state and remaining transition time.
- Cumulative waiting time.
- Vehicles served.
- Maximum queue observed.
- Queue history for charts and interval queries.
- Current controller and generated signal plan.

### 8.2 Arrival process

For each second, generate arrivals based on the configured approach demand. Use a seeded pseudo-random generator or pre-generate an arrival schedule.

The same seed and settings must produce the same arrivals in baseline and optimized runs.

Document whether the configured arrival rate is:
- an expected number of vehicles per second, or
- a probability/distribution parameter.

Use one consistent definition throughout the UI and simulation.

### 8.3 Service process

When an approach is permitted to move:
- Serve no more than the configured service capacity.
- Never serve more vehicles than are present in the queue.
- Never allow a queue to become negative.
- Do not serve a conflicting approach during a protected phase.
- Respect the chosen phase model consistently.

### 8.4 Phase model

For the MVP, use a clear phase plan. A reasonable simplified default is:
- Phase A: North-South compatible movements.
- Phase B: East-West compatible movements.
- Yellow/all-red transition intervals between conflicting phases.

If the implementation instead models all four approaches independently, clearly document that choice and enforce the same conflict and transition rules. Do not mix incompatible phase assumptions across the controller and simulator.

### 8.5 Waiting-time calculation

At each tick:
- Add waiting time for vehicles that remain queued.
- Track cumulative waiting time consistently.
- Define the average-wait denominator explicitly (for example, vehicles served during the run).
- Use the same definition in baseline and optimized metrics.

### 8.6 Simulation controls

- **Start/Resume:** continue advancing simulated time.
- **Pause:** stop simulation time and queue updates.
- **Step:** advance exactly one simulation tick.
- **Reset:** restore the selected scenario to its original state.
- **Optimize Signals:** calculate a new plan; do not silently change recorded baseline results.

---

## 9. DAA and Algorithm Requirements

Each algorithm listed below must have a genuine, explainable role. Do not use an algorithm only as a label.

### 9.1 Greedy algorithm — required

Purpose: allocate green time based on current demand and urgency.

A possible score for approach `i` is:

`score(i) = alpha * queueLength(i) + beta * predictedArrivals(i) + gamma * waitingTime(i)`

Where:
- `alpha`, `beta`, and `gamma` are configurable or documented weights.
- Inputs must use consistent units or carefully chosen weights.
- The score is a simulation heuristic, not a validated traffic-engineering formula.

Requirements:
1. Calculate scores from current simulation state.
2. Allocate green time according to scores or weighted demand.
3. Enforce minimum and maximum green durations.
4. Respect the total cycle-time budget.
5. Explain the resulting allocation in the UI.
6. Avoid starving low-demand approaches.

### 9.2 Priority Queue — required

Purpose: prioritize approaches or eligible phases using a changing urgency score.

Requirements:
- Implement an actual priority queue, such as a binary heap, or use a clearly documented standard implementation.
- Define how priority is calculated.
- Recompute priorities when queues and waiting times change.
- Avoid stale priority values when the state changes.
- Use a deterministic tie-breaking rule.
- Add a waiting-time bonus or maximum-wait safeguard to reduce starvation.

The UI should expose enough information for the user to understand why an approach or phase was prioritized.

### 9.3 Graph — advanced extension

Purpose: model a network of intersections.

Requirements when implemented:
- Each intersection is a vertex.
- Each connecting road is an edge.
- Edges may store traffic flow, capacity, or estimated travel time.
- Use an adjacency list or another documented graph representation.
- Do not turn this into a route-optimization project; the graph is for representing connected intersections and analyzing traffic interactions.

### 9.4 Segment Tree — advanced extension

Purpose: support efficient queries over time-series traffic measurements.

Possible operations:
- Point update for a recorded traffic value.
- Range-sum query for arrivals or queue measurements over a time interval.
- Range-maximum query for peak queue length over an interval, if supported by the selected tree design.

Requirements:
- State the query/update complexity.
- Use actual recorded simulation history.
- Include a small UI control to select a time interval and display the query result.
- Do not claim Segment Tree is required for ordinary queue updates.

### 9.5 Dynamic Programming — advanced extension

Purpose: select a phase sequence over a bounded future horizon to minimize estimated waiting cost.

Requirements:
- Define the state, transition, cost function, and planning horizon.
- Include phase-switch/transition costs and legal phase constraints.
- Keep the horizon bounded so the calculation remains responsive.
- Compare the selected plan against the greedy plan.
- Explain that the result is optimal only for the defined simplified model and planning horizon, not necessarily for real traffic.

### 9.6 Baseline controller

Implement a fixed-time controller for comparison. It should use a predictable, documented phase sequence and fixed green durations.

The baseline must use the same:
- initial queues,
- arrival sequence,
- service capacities,
- simulation duration,
- phase compatibility rules,
- and metric definitions

as the optimized run.

---

## 10. Green-Time Constraints and Safety Rules

The optimizer must never allocate time without checking constraints.

Implement configurable:
- Minimum green duration.
- Maximum green duration.
- Yellow transition duration.
- All-red clearance duration, if represented.
- Total cycle budget.
- Fairness or maximum-wait safeguard.

Rules:
1. No conflicting movements may receive green simultaneously.
2. A conflicting green-to-green transition must pass through the modeled transition state.
3. Every eligible approach must receive service within a bounded interval under the chosen simplified phase model.
4. Allocations must respect the cycle budget or report why a valid allocation could not be produced.
5. Durations must be valid integers or consistently defined time units.
6. Do not silently ignore infeasible settings; show a useful validation message.
7. Treat this as an educational simulation only, not a real traffic controller.

---

## 11. Metrics

Calculate all metrics from actual simulation events.

### Required metrics
- **Average waiting time:** cumulative waiting time divided by a clearly stated denominator.
- **Throughput:** number of vehicles served in the observation window.
- **Residual queue:** total number of vehicles still waiting at the end.
- **Maximum queue:** highest observed queue length, per approach and overall if practical.
- **Total queue:** sum of all approach queues at the current tick.

### Comparison
Show:
- Baseline value.
- Optimized value.
- Absolute difference.
- Percentage difference when mathematically defined.

Do not hardcode improvement percentages. The optimizer may perform worse in some scenarios; display actual results honestly.

---

## 12. Scenario Presets

Provide at least these presets:

1. **Balanced:** similar demand on all approaches.
2. **North-South Rush:** high demand from North and South.
3. **East-West Rush:** high demand from East and West.
4. **Single-Approach Congestion:** one approach has much higher demand.
5. **Burst Arrivals:** a temporary surge followed by lower demand.

Presets must change actual simulation parameters. The user should also be able to edit demand manually.

---

## 13. Architecture and Code Organization

Separate the application into modules with clear responsibilities. Adapt filenames to the chosen setup, but preserve separation of concerns.

Suggested structure:

```text
traffic-signal-simulator/
├── index.html
├── styles/
│   └── main.css
├── src/
│   ├── app.js
│   ├── simulation/
│   │   ├── engine.js
│   │   ├── arrivals.js
│   │   ├── metrics.js
│   │   └── phases.js
│   ├── algorithms/
│   │   ├── fixedTime.js
│   │   ├── greedy.js
│   │   ├── priorityQueue.js
│   │   ├── segmentTree.js
│   │   ├── graph.js
│   │   └── dynamicProgramming.js
│   ├── ui/
│   │   ├── render.js
│   │   ├── controls.js
│   │   └── charts.js
│   └── utils/
│       └── seededRandom.js
├── tests/
│   ├── simulation.test.js
│   ├── algorithms.test.js
│   └── metrics.test.js
└── README.md
```

Do not create empty files just to match the tree. If an advanced feature is not implemented, omit its file and mark the feature as not implemented in the README.

### Architecture rules
- Simulation logic must not depend on DOM elements.
- UI rendering must not be responsible for calculating traffic outcomes.
- Algorithms should accept explicit state/configuration and return a plan or ranking.
- Metrics should be computed by a dedicated module.
- Keep random generation reproducible.
- Avoid duplicated sources of truth for queue counts or signal state.

---

## 14. Data Structures

Use a clear data model.

### Approach
- `id`
- `direction`
- `queueLength`
- `arrivalRate`
- `serviceRate`
- `currentWaitingTime` or equivalent
- `accumulatedWait`
- `maxQueue`

### Signal phase
- `id`
- `approachesServed`
- `greenDuration`
- `yellowDuration`
- `allRedDuration`
- `minimumGreen`
- `maximumGreen`

### Scenario
- `id`
- `name`
- `seed`
- `duration`
- `arrivalProfile`
- `serviceRates`
- `phaseModel`

### Simulation state
- `time`
- `activePhaseId`
- `phaseTimeRemaining`
- `approaches`
- `metrics`
- `status`
- `arrivalSchedule`

### Optimization result
- `strategy`
- `scoresByApproach`
- `allocatedGreenDurations`
- `constraintsApplied`
- `explanation`

The final implementation may adapt the exact schema, but it must retain the information needed for deterministic simulation, optimization, explanation, and fair comparison.

---

## 15. Error Handling and Edge Cases

Handle the following:
- Zero arrivals on all approaches.
- A queue starting at zero.
- One approach with very high demand.
- Arrival demand greater than service capacity for a long time.
- Ties in priority scores.
- Minimum green durations whose sum exceeds the available budget.
- Invalid or negative input values.
- Reset while paused or after completion.
- Attempting to optimize while simulation state is changing.
- Empty or very short simulation durations.
- No vehicles served during a run, where average waiting time may be undefined.

For undefined metrics, display `N/A` or a clearly defined value instead of producing `NaN` or infinity.

---

## 16. Non-Functional Requirements

- **Performance:** remain responsive during a typical 120-second simulated run.
- **Usability:** controls and metrics must be clearly labeled.
- **Accessibility:** use readable contrast, keyboard-operable controls, and text/icons in addition to color.
- **Reliability:** reset returns the entire simulation to a known state.
- **Explainability:** each optimization result shows the strategy, allocations, and constraints.
- **Maintainability:** separate simulation, algorithms, metrics, and UI.
- **Privacy:** no personal data or account system is required.
- **Portability:** document the supported browser and local run instructions.

---

## 17. Testing Requirements

### Unit tests
- Arrival generation is deterministic for a fixed seed.
- Queue lengths never become negative.
- Vehicle service never exceeds the queue or service capacity.
- Priority Queue returns the highest-priority eligible item.
- Greedy allocations respect minimum/maximum durations and cycle budget.
- Metrics match manually calculable examples.
- Segment Tree queries and updates are correct if implemented.
- DP output uses only legal phase transitions if implemented.

### Scenario tests
- Balanced demand.
- One highly congested approach.
- No arrivals.
- Burst arrivals.
- Sustained overload.

### UI tests
- Start advances simulated time.
- Pause stops simulated time.
- Step advances exactly one tick.
- Reset restores the initial scenario.
- Queue bars match displayed counts.
- Optimize Signals creates a valid plan.
- Comparison uses the same arrival schedule.

### Fairness test
Run baseline and optimized strategies with identical arrivals and settings. Verify that the comparison is reproducible. Do not write tests that assume the optimized controller must always outperform baseline.

---

## 18. Acceptance Criteria / Definition of Done

The project is complete when:
- The app launches using the README instructions.
- The four-way intersection and all queue counts render correctly.
- Users can change demand and select a scenario.
- Start, Pause, Step, Reset, and Optimize Signals work.
- The baseline controller runs correctly.
- Greedy optimization is implemented and explained.
- Priority Queue has a real role in ranking/scheduling.
- Signal transitions obey the documented phase constraints.
- Queue lengths change in response to arrivals and service.
- Metrics are computed from real simulation events.
- Baseline and optimized runs use identical arrival data.
- The UI shows before/after results and a useful explanation.
- Automated tests cover the core rules.
- Advanced features are either implemented and tested or explicitly identified as not implemented.
- The README explains setup, controls, algorithms, assumptions, and limitations.

---

## 19. Recommended Implementation Order

1. Define the simulation state, phase model, and deterministic arrivals.
2. Implement the queue update and service rules.
3. Implement the fixed-time baseline.
4. Implement metrics and unit tests.
5. Build the dashboard and connect it to the simulation state.
6. Implement Greedy allocation with constraints.
7. Implement Priority Queue scheduling and starvation safeguards.
8. Add deterministic baseline-vs-optimized comparison.
9. Add scenario presets and visual polish.
10. Add Segment Tree, Graph network mode, and DP planner only after the MVP is stable.
11. Complete README and verify the full acceptance checklist.

---

## 20. Final Instruction to the LLM

Start by inspecting the available project files if any exist. Preserve working code where possible. Then implement the project end-to-end according to this PRD.

Do not stop after generating a plan or a UI mockup. Build a runnable application, test the core behavior, fix errors found during testing, and provide:
- a concise summary of what was implemented,
- exact run instructions,
- the project file tree,
- the tests performed and their results,
- and any clearly disclosed limitations.

**Educational-use disclaimer:** This is a simplified simulation for learning and demonstration. It must not be connected to or used to control real traffic signals.
