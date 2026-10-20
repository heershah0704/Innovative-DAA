// Holds the settings and plays back a run computed by the C++ server.
// Start / Pause / Step / Reset only move through the frames the server sent.
import { fetchScenarios, fetchRun, fetchWindow } from './api.js';
import { DIRECTIONS, PHASES, STRATEGIES } from './labels.js';

const clone = (x) => JSON.parse(JSON.stringify(x));

export class Player {
  constructor({ onChange }) {
    this.onChange = onChange;
    this.strategy = 'fixed';
    this.speed = 5;
    this.timer = null;
    this.results = {};
    this.message = '';
    this.preview = null;
    this.version = 0;
    this.frame = 0;
    this.playing = false;
  }

  emit() { this.onChange(this); }

  async init() {
    const data = await fetchScenarios();
    this.presets = data.scenarios;
    this.defaultTiming = data.timing;
    this.scenario = clone(this.presets[0]);
    this.timing = clone(this.defaultTiming);
    await this.loadRun();
    await this.runBoth();
    this.message = 'Default scenario loaded with results for both strategies. Press Start to watch a run.';
    this.emit();
  }

  // Ask the server for the full run with the current settings and strategy.
  async loadRun() {
    this.stopTimer();
    const res = await fetchRun(this.scenario, this.timing, this.strategy);
    if (!res.ok) throw new Error(res.errors.join(' '));
    this.trace = res;
    this.frame = 0;
    this.playing = false;
  }

  get last() { return this.trace.frames.length - 1; }
  get status() {
    if (this.playing) return 'running';
    if (this.frame >= this.last) return 'completed';
    return this.frame === 0 ? 'ready' : 'paused';
  }

  // Everything the page needs for the current second.
  view() {
    const f = this.trace.frames[this.frame];
    const q = {}, red = {}, maxQ = {};
    DIRECTIONS.forEach((d, i) => { q[d] = f.q[i]; red[d] = f.red[i]; maxQ[d] = f.maxQ[i]; });
    const decisions = this.trace.decisions.filter((d) => d.time <= f.t);
    return {
      time: f.t,
      duration: this.trace.duration,
      status: this.status,
      q, red, maxQ,
      signal: f.signal,
      controller: this.trace.controller,
      decisions,
      lastDecision: decisions[decisions.length - 1] ?? null,
      history: this.trace.frames.slice(0, this.frame + 1).map((x) => x.total),
      stats: {
        served: f.served,
        totalWait: f.wait,
        avgWait: f.served > 0 ? f.wait / f.served : null,
        totalQueue: f.total,
        maxQueue: Math.max(...f.maxQ),
        maxQueueBy: maxQ,
        switches: f.switches,
      },
    };
  }

  start() {
    if (this.playing || this.frame >= this.last) return;
    this.playing = true;
    this.startTimer();
    this.emit();
  }

  pause() {
    if (!this.playing) return;
    this.stopTimer();
    this.playing = false;
    this.emit();
  }

  step() {
    if (this.playing || this.frame >= this.last) return;
    this.advance();
    this.emit();
  }

  reset(message = 'Reset to the starting state. Same arrivals as before.') {
    this.stopTimer();
    this.playing = false;
    this.frame = 0;
    this.preview = null;
    this.message = message;
    this.emit();
  }

  setSpeed(speed) {
    this.speed = speed;
    if (this.playing) { this.stopTimer(); this.startTimer(); }
  }

  startTimer() {
    this.timer = setInterval(() => { this.advance(); this.emit(); }, 1000 / this.speed);
  }

  stopTimer() {
    clearInterval(this.timer);
    this.timer = null;
  }

  advance() {
    if (this.frame < this.last) this.frame += 1;
    if (this.frame >= this.last) {
      this.stopTimer();
      this.playing = false;
      this.record(this.strategy, this.trace);
      this.message = `${STRATEGIES[this.strategy].short} run finished at ${this.trace.duration} s.`;
    }
  }

  record(strategy, trace) {
    this.results[strategy] = { summary: trace.summary, series: trace.frames.map((f) => f.total), scheduleId: trace.scheduleId };
  }

  async setStrategy(strategy) {
    if (this.playing) return;
    this.strategy = strategy;
    this.preview = null;
    this.message = '';
    await this.loadRun();
    this.emit();
  }

  async setScenario(id) {
    if (this.playing) return;
    this.scenario = clone(this.presets.find((s) => s.id === id));
    this.results = {};
    this.preview = null;
    this.version += 1;
    await this.loadRun();
    await this.runBoth();
    this.message = `${this.scenario.name} loaded with results for both strategies.`;
    this.emit();
  }

  // Send the changed settings to the server. Keep them only if the server accepts them.
  async tryUpdate(kind, mutate) {
    if (this.playing) return ['Pause the simulation before changing settings.'];
    const scenario = clone(this.scenario);
    const timing = clone(this.timing);
    mutate(kind === 'timing' ? timing : scenario);
    const res = await fetchRun(scenario, timing, this.strategy);
    if (!res.ok) return res.errors;
    this.scenario = scenario;
    this.timing = timing;
    this.trace = res;
    this.frame = 0;
    this.results = {};
    this.preview = null;
    this.version += 1;
    this.message = 'Settings changed. Earlier results were cleared because they used different inputs.';
    this.emit();
    return [];
  }

  // Switch to the greedy controller, go back to 0 s, and show its first decision.
  async optimize() {
    if (this.playing) return;
    const wasRunning = this.frame > 0;
    this.strategy = 'greedy';
    await this.loadRun();
    const first = this.trace.decisions[0];
    const prefix = wasRunning ? 'Reset to 0 s so this run gets the same arrivals as the baseline. ' : '';
    if (!first) {
      this.preview = null;
      this.message = `${prefix}The run ends before the opening green does, so there is nothing to plan. Increase the duration.`;
    } else {
      this.preview = {
        decision: first,
        context: `First decision, at ${first.time} s when the opening ${PHASES[first.from].label} green ends. The controller only learns about later cars as they arrive.`,
      };
      const what = first.kind === 'extend'
        ? `extend ${PHASES[first.phase].label} by ${first.duration} s`
        : `switch to ${PHASES[first.phase].label} for ${first.duration} s`;
      this.message = `${prefix}Plan ready: at ${first.time} s, ${what}. Press Start to run it.`;
    }
    this.emit();
  }

  async runBoth() {
    if (this.playing) return;
    for (const s of Object.keys(STRATEGIES)) {
      const res = s === this.strategy ? this.trace : await fetchRun(this.scenario, this.timing, s);
      if (res.ok) this.record(s, res);
    }
    this.message = 'Both strategies ran to the end on the same arrivals.';
    this.emit();
  }

  async queryWindow(from, to) {
    const t = this.trace.frames[this.frame].t;
    if (t === 0) return { error: 'Run or step the simulation first; nothing is recorded yet.' };
    if (!Number.isInteger(from) || !Number.isInteger(to) || from < 0 || to > t || from >= to) {
      return { error: `Pick whole seconds with 0 ≤ from < to ≤ ${t}.` };
    }
    const res = await fetchWindow(this.scenario, this.timing, this.strategy, from, to);
    return res.ok ? res : { error: res.errors.join(' ') };
  }
}
