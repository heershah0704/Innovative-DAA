// Talks to the C++ server. Every number shown on the page comes from these calls.
import { DIRECTIONS } from './labels.js';

async function get(path, params = {}) {
  const url = `${path}?${new URLSearchParams(params)}`;
  let res;
  try {
    res = await fetch(url);
  } catch {
    throw new Error('Cannot reach the C++ server. Start it (see README) and open the page from http://localhost:8080.');
  }
  const body = await res.json().catch(() => null);
  if (!body) throw new Error(`Server returned ${res.status} without data.`);
  return body;
}

// Scenario + timing objects -> URL parameters the C++ server reads.
export function toParams(scenario, timing, strategy) {
  const p = {
    scenario: scenario.id,
    strategy,
    seed: scenario.seed,
    duration: scenario.duration,
    phase: scenario.initialPhase,
    green0: scenario.initialGreenRemaining,
    minGreen: timing.minGreen,
    maxGreen: timing.maxGreen,
    yellow: timing.yellow,
    allRed: timing.allRed,
    cycleBudget: timing.cycleBudget,
    maxWait: timing.maxWait,
    fixNS: timing.fixedGreen.NS,
    fixEW: timing.fixedGreen.EW,
    alpha: timing.weights.alpha,
    beta: timing.weights.beta,
    gamma: timing.weights.gamma,
  };
  for (const d of DIRECTIONS) {
    p[`q${d}`] = scenario.initialQueues[d];
    p[`r${d}`] = scenario.arrivalRates[d];
    p[`s${d}`] = scenario.serviceRates[d];
  }
  return p;
}

export const fetchScenarios = () => get('/api/scenarios');
export const fetchRun = (scenario, timing, strategy) => get('/api/simulate', toParams(scenario, timing, strategy));
export const fetchWindow = (scenario, timing, strategy, from, to) =>
  get('/api/query', { ...toParams(scenario, timing, strategy), from, to });
