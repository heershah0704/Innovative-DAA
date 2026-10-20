// Display names only. No traffic logic lives in the frontend.
export const DIRECTIONS = ['N', 'S', 'E', 'W'];
export const DIRECTION_NAMES = { N: 'North', S: 'South', E: 'East', W: 'West' };
export const PHASES = {
  NS: { label: 'North–South', short: 'N–S', approaches: ['N', 'S'] },
  EW: { label: 'East–West', short: 'E–W', approaches: ['E', 'W'] },
};
export const STRATEGIES = {
  fixed: { label: 'Fixed-time (baseline)', short: 'Fixed-time' },
  greedy: { label: 'Greedy + priority queue', short: 'Greedy + PQ' },
};
