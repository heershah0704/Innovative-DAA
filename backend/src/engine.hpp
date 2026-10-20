// engine.hpp
// The simulation. One call to step() = one simulated second:
//   1. this second's arrivals join their queues
//   2. approaches with green let cars through (up to their service rate)
//   3. every car still waiting adds 1 second of delay
//   4. the light's clock ticks; when a green ends, the controller decides
#pragma once
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>
#include "arrivals.hpp"
#include "controllers.hpp"
#include "model.hpp"
#include "segment_tree.hpp"

// Snapshot after each second, sent to the frontend to animate.
struct Frame {
    int t = 0;
    int q[4] = {0, 0, 0, 0};
    int red[4] = {0, 0, 0, 0};
    int maxQ[4] = {0, 0, 0, 0};
    int total = 0;
    int served = 0;
    long long wait = 0;
    int switches = 0;
    Signal signal;
};

class Simulation {
public:
    Simulation(const Scenario& sc, const Timing& t, std::unique_ptr<Controller> controller, const Schedule& schedule)
        : scenario_(sc), timing_(t), controller_(std::move(controller)), schedule_(schedule),
          arrivalsTree_(makeSumTree(sc.duration)), queueTree_(makeMaxTree(sc.duration + 1)) {
        if (static_cast<int>(schedule_.size()) < sc.duration) throw std::invalid_argument("schedule shorter than run");
        for (int d = 0; d < 4; ++d) {
            a_[d].queue = sc.initialQueues[d];
            a_[d].maxQueue = sc.initialQueues[d];
            a_[d].arrivalRate = sc.arrivalRates[d];
            a_[d].serviceRate = sc.serviceRates[d];
        }
        signal_.phase = sc.initialPhase;
        signal_.state = Light::Green;
        signal_.remaining = sc.initialGreenRemaining;
        signal_.greenPlanned = sc.initialGreenRemaining;
        maxTotalQueue_ = totalQueue();
        queueTree_.update(0, totalQueue());
        frames_.push_back(snapshot());
    }

    bool done() const { return time_ >= scenario_.duration; }

    bool step() {
        if (done()) return false;
        const int t = time_;
        int arrivedNow = 0;

        // 1. arrivals
        for (int d = 0; d < 4; ++d) {
            a_[d].queue += schedule_[t][d];
            a_[d].arrived += schedule_[t][d];
            arrivedNow += schedule_[t][d];
        }
        totalArrived_ += arrivedNow;

        // 2. service and 3. waiting
        for (int d = 0; d < 4; ++d) {
            Approach& a = a_[d];
            bool green = signal_.state == Light::Green && phaseServes(signal_.phase, d);
            if (green) {
                a.credit += a.serviceRate;
                int leave = std::min(a.queue, static_cast<int>(a.credit + 1e-9));  // whole cars only
                a.queue -= leave;
                a.credit -= leave;
                if (a.queue == 0) a.credit -= static_cast<int>(a.credit);  // unused capacity is not saved up
                a.served += leave;
                totalServed_ += leave;
                a.redTime = 0;
            } else {
                a.credit = 0;
                ++a.redTime;
            }
            a.accumulatedWait += a.queue;
            totalWait_ += a.queue;
            a.maxQueue = std::max(a.maxQueue, a.queue);
        }

        time_ = t + 1;
        maxTotalQueue_ = std::max(maxTotalQueue_, totalQueue());
        arrivalsTree_.update(t, arrivedNow);
        queueTree_.update(time_, totalQueue());

        // 4. signal clock
        advanceSignal();
        frames_.push_back(snapshot());
        return true;
    }

    void runToEnd() { while (step()) {} }

    // The controller's view: current queues, red times and arrival rates only.
    ControllerView view() const {
        ControllerView v;
        v.time = time_;
        v.phase = signal_.phase;
        v.state = signal_.state;
        v.greenElapsed = signal_.greenElapsed;
        for (int d = 0; d < 4; ++d) {
            v.a[d].queue = a_[d].queue;
            v.a[d].redTime = a_[d].redTime;
            v.a[d].arrivalRate = a_[d].arrivalRate;
            v.a[d].serviceRate = a_[d].serviceRate;
        }
        return v;
    }

    int totalQueue() const { int n = 0; for (const auto& a : a_) n += a.queue; return n; }

    // Accessors
    int time() const { return time_; }
    const Scenario& scenario() const { return scenario_; }
    const Timing& timing() const { return timing_; }
    const Signal& signal() const { return signal_; }
    const Approach& approach(int d) const { return a_[d]; }
    const std::vector<Frame>& frames() const { return frames_; }
    const std::vector<Decision>& decisions() const { return decisions_; }
    const Schedule& schedule() const { return schedule_; }
    std::string controllerName() const { return controller_->name(); }
    int totalServed() const { return totalServed_; }
    int totalArrived() const { return totalArrived_; }
    long long totalWait() const { return totalWait_; }
    int switches() const { return switches_; }
    int maxTotalQueue() const { return maxTotalQueue_; }
    const SegmentTree<int>& arrivalsTree() const { return arrivalsTree_; }
    const SegmentTree<int>& queueTree() const { return queueTree_; }

private:
    void startNextGreen() {
        signal_.phase = signal_.nextPhase;
        signal_.state = Light::Green;
        signal_.remaining = signal_.nextDuration;
        signal_.greenElapsed = 0;
        signal_.greenPlanned = signal_.nextDuration;
        signal_.hasNext = false;
        ++switches_;
    }

    void advanceSignal() {
        --signal_.remaining;
        if (signal_.state == Light::Green) ++signal_.greenElapsed;
        if (signal_.remaining > 0) return;

        if (signal_.state == Light::Green) {
            Decision d = controller_->decide(view());
            d.time = time_;
            d.from = signal_.phase;
            // The engine has the final say on safety rules, whatever the controller asked.
            if (d.phase == signal_.phase && signal_.greenElapsed >= timing_.maxGreen) {
                d.phase = otherPhase(signal_.phase);
                d.overridden = "Maximum green reached; the engine forced a switch.";
            }
            if (d.phase == signal_.phase) {
                int ext = std::max(1, std::min(d.duration, timing_.maxGreen - signal_.greenElapsed));
                signal_.remaining = ext;
                signal_.greenPlanned += ext;
                d.duration = ext;
                d.kind = "extend";
            } else {
                int dur = std::max(timing_.minGreen, std::min(d.duration, timing_.maxGreen));
                signal_.state = Light::Yellow;
                signal_.remaining = timing_.yellow;
                signal_.hasNext = true;
                signal_.nextPhase = d.phase;
                signal_.nextDuration = dur;
                d.duration = dur;
                d.kind = "switch";
            }
            decisions_.push_back(d);
        } else if (signal_.state == Light::Yellow) {
            if (timing_.allRed > 0) {
                signal_.state = Light::AllRed;
                signal_.remaining = timing_.allRed;
            } else {
                startNextGreen();
            }
        } else {
            startNextGreen();
        }
    }

    Frame snapshot() const {
        Frame f;
        f.t = time_;
        for (int d = 0; d < 4; ++d) { f.q[d] = a_[d].queue; f.red[d] = a_[d].redTime; f.maxQ[d] = a_[d].maxQueue; }
        f.total = totalQueue();
        f.served = totalServed_;
        f.wait = totalWait_;
        f.switches = switches_;
        f.signal = signal_;
        return f;
    }

    Scenario scenario_;
    Timing timing_;
    std::unique_ptr<Controller> controller_;
    Schedule schedule_;
    int time_ = 0;
    Approach a_[4];
    Signal signal_;
    int totalServed_ = 0, totalArrived_ = 0, switches_ = 0, maxTotalQueue_ = 0;
    long long totalWait_ = 0;
    std::vector<Frame> frames_;
    std::vector<Decision> decisions_;
    SegmentTree<int> arrivalsTree_, queueTree_;
};

// Factory used by the server and tests.
inline std::unique_ptr<Controller> makeController(const std::string& strategy, const Timing& t) {
    if (strategy == "fixed") return std::make_unique<FixedTimeController>(t);
    if (strategy == "greedy") return std::make_unique<GreedyController>(t);
    throw std::invalid_argument("unknown strategy: " + strategy);
}
