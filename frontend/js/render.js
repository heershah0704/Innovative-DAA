// Turns the current frame from the C++ server into markup. Display only.
import { DIRECTIONS, DIRECTION_NAMES, PHASES } from './labels.js';

export const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
const r1 = (x) => (Math.round(x * 10) / 10).toFixed(1);
const int = (x) => Math.round(x).toLocaleString('en-US');

export const hasGreen = (signal, dir) => signal.state === 'green' && PHASES[signal.phase].approaches.includes(dir);
export function lampState(signal, dir) {
  if (!PHASES[signal.phase].approaches.includes(dir)) return 'red';
  return signal.state === 'green' ? 'green' : signal.state === 'yellow' ? 'yellow' : 'red';
}
const LAMP_WORD = { green: 'Go', yellow: 'Clearing', red: 'Stop' };
const LAMP_LETTER = { green: 'G', yellow: 'Y', red: 'R' };

export const queueScale = (v) => Math.max(40, ...DIRECTIONS.map((d) => v.maxQ[d]));
export const barPercent = (q, scale) => (scale > 0 ? Math.min(100, Math.max(0, (q / scale) * 100)) : 0);

// Drawing geometry: roads 120 wide, junction box from 140 to 260 on both axes.
const SHOWN_ROWS = 9;
const PITCH = 13;
const carRect = {
  N: (i, c) => ({ x: 147 + c * 30, y: 122 - i * PITCH, w: 16, h: 10 }),
  S: (i, c) => ({ x: 207 + c * 30, y: 268 + i * PITCH, w: 16, h: 10 }),
  E: (i, c) => ({ x: 268 + i * PITCH, y: 147 + c * 30, w: 10, h: 16 }),
  W: (i, c) => ({ x: 122 - i * PITCH, y: 207 + c * 30, w: 10, h: 16 }),
};
const head = { N: [128, 124], S: [272, 276], E: [276, 128], W: [124, 272] };
const label = {
  N: { x: 128, y: 24, anchor: 'end' },
  E: { x: 392, y: 24, anchor: 'end' },
  S: { x: 272, y: 368, anchor: 'start' },
  W: { x: 8, y: 290, anchor: 'start' },
};

export function intersectionSvg(v) {
  const s = v.signal;
  const parts = [];
  parts.push('<rect x="140" y="0" width="120" height="400" fill="var(--road)"/>');
  parts.push('<rect x="0" y="140" width="400" height="120" fill="var(--road)"/>');
  const dash = 'stroke="var(--lane)" stroke-width="2" stroke-dasharray="10 8"';
  parts.push(`<line x1="200" y1="0" x2="200" y2="136" ${dash}/><line x1="200" y1="264" x2="200" y2="400" ${dash}/>`);
  parts.push(`<line x1="0" y1="200" x2="136" y2="200" ${dash}/><line x1="264" y1="200" x2="400" y2="200" ${dash}/>`);
  const stop = 'stroke="var(--lane)" stroke-width="3"';
  parts.push(`<line x1="140" y1="136" x2="200" y2="136" ${stop}/><line x1="200" y1="264" x2="260" y2="264" ${stop}/>`);
  parts.push(`<line x1="264" y1="140" x2="264" y2="200" ${stop}/><line x1="136" y1="200" x2="136" y2="260" ${stop}/>`);

  for (const d of DIRECTIONS) {
    const q = v.q[d];
    const shown = Math.min(q, SHOWN_ROWS * 2);
    for (let k = 0; k < shown; k++) {
      const { x, y, w, h } = carRect[d](Math.floor(k / 2), k % 2);
      parts.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="2" fill="var(--car)"/>`);
    }
    const st = lampState(s, d);
    const [hx, hy] = head[d];
    parts.push(`<circle cx="${hx}" cy="${hy}" r="8" fill="var(--${st === 'green' ? 'go' : st === 'yellow' ? 'slow' : 'stop'})"/>`);
    parts.push(`<text x="${hx}" y="${hy + 3.5}" text-anchor="middle" font-size="10" font-weight="700" fill="#fff">${LAMP_LETTER[st]}</text>`);
    const L = label[d];
    parts.push(`<text x="${L.x}" y="${L.y}" text-anchor="${L.anchor}" font-size="12" fill="var(--muted)">${DIRECTION_NAMES[d]}, ${LAMP_WORD[st].toLowerCase()}</text>`);
    parts.push(`<text x="${L.x}" y="${L.y + 20}" text-anchor="${L.anchor}" font-size="17" font-weight="600" fill="var(--ink)">${q} car${q === 1 ? '' : 's'}</text>`);
  }
  const title = s.state === 'allRed' ? 'All red' : PHASES[s.phase].short;
  const sub = s.state === 'allRed' ? `${s.remaining} s` : `${s.state}, ${s.remaining} s`;
  parts.push(`<text x="200" y="196" text-anchor="middle" font-size="18" font-weight="600" fill="var(--ink)">${title}</text>`);
  parts.push(`<text x="200" y="216" text-anchor="middle" font-size="12" fill="var(--ink)">${sub}</text>`);
  return parts.join('');
}

export function signalSentence(v) {
  const s = v.signal;
  if (s.state === 'green') return `${PHASES[s.phase].label} green, ${s.remaining} s left of ${s.greenPlanned} s`;
  if (s.state === 'yellow') return `${PHASES[s.phase].label} yellow, ${s.remaining} s`;
  return `All red, ${s.remaining} s`;
}

export function intersectionDescription(v) {
  const q = DIRECTIONS.map((d) => `${DIRECTION_NAMES[d]} ${v.q[d]} cars, ${LAMP_WORD[lampState(v.signal, d)].toLowerCase()}`).join('; ');
  return `${signalSentence(v)}. ${q}.`;
}

export function signalNowHtml(v) {
  const s = v.signal;
  const lamp = s.state === 'green' ? 'green' : s.state === 'yellow' ? 'yellow' : 'red';
  return `<span class="lamp ${lamp}" aria-hidden="true"></span>${esc(signalSentence(v))}`;
}

export function signalNextText(v) {
  if (v.signal.next) return `Next: ${PHASES[v.signal.next.phase].label} green for ${v.signal.next.duration} s.`;
  if (v.status === 'completed') return 'Run finished.';
  return `When this green ends, the ${v.controller.toLowerCase()} controller decides what comes next.`;
}

export function metricsHtml(m, completed) {
  const maxDir = DIRECTIONS.reduce((a, d) => (m.maxQueueBy[d] > m.maxQueueBy[a] ? d : a), 'N');
  const items = [
    ['Average wait', m.avgWait == null ? 'N/A' : `${r1(m.avgWait)} <small>s</small>`],
    ['Vehicles served', int(m.served)],
    [completed ? 'Residual queue' : 'Waiting now', int(m.totalQueue)],
    ['Longest queue', `${int(m.maxQueue)} <small>${DIRECTION_NAMES[maxDir]}</small>`],
    ['Total delay', `${int(m.totalWait)} <small>veh·s</small>`],
    ['Phase switches', int(m.switches)],
  ];
  return items.map(([k, val]) => `<div><dt>${k}</dt><dd>${val}</dd></div>`).join('');
}

export function inspectorHtml(d, context) {
  if (!d) return '<p class="muted">Press Optimize signals, or start a run, to see the controller\'s reasoning.</p>';
  const out = [];
  out.push(`<p class="kv"><span>Strategy</span> ${esc(d.strategy)}</p>`);
  if (context) out.push(`<p class="muted">${esc(context)}</p>`);
  out.push(`<p>${esc(d.explanation)}</p>`);
  if (d.scores) {
    const rows = DIRECTIONS.map((k) => {
      const s = d.scores[k];
      return `<tr><td>${DIRECTION_NAMES[k]}</td><td class="num">${s.queue}</td><td class="num">${r1(s.predicted)}</td><td class="num">${s.wait} s</td><td class="num">${r1(s.score)}</td></tr>`;
    }).join('');
    out.push(`<table class="inspector-table"><thead><tr><th>Approach</th><th class="num">Queue</th><th class="num">Expected</th><th class="num">Red for</th><th class="num">Score</th></tr></thead><tbody>${rows}</tbody></table>`);
  }
  if (d.ranking?.length) {
    const order = d.ranking.map((r, i) => `${i + 1}. ${PHASES[r.phase].label} (${r1(r.score)}${r.forced ? ', overdue' : ''})`).join('&ensp;');
    out.push(`<p class="kv"><span>Priority queue order</span> ${order}</p>`);
  }
  if (d.alloc) out.push(`<p class="kv"><span>Green split this cycle</span> North–South ${d.alloc.NS} s, East–West ${d.alloc.EW} s</p>`);
  if (d.constraints?.length) out.push(`<ul class="constraints">${d.constraints.map((c) => `<li>${esc(c)}</li>`).join('')}</ul>`);
  return out.join('');
}

export function logHtml(decisions, limit = 40) {
  if (!decisions.length) return '<li class="muted">No decisions yet. The first comes when the opening green ends.</li>';
  return decisions.slice(-limit).reverse().map((d) => {
    const body = d.kind === 'extend'
      ? `Extend ${PHASES[d.phase].short} by ${d.duration} s`
      : `${PHASES[d.from].short} → ${PHASES[d.phase].short}, ${d.duration} s green`;
    const note = d.overridden ? ` <span class="muted">(${esc(d.overridden)})</span>` : '';
    return `<li><span class="t">${d.time} s</span>${body}${note}</li>`;
  }).join('');
}

const METRIC_ROWS = [
  { key: 'avgWait', label: 'Average wait (s per served vehicle)', better: 'lower', digits: 1 },
  { key: 'totalWait', label: 'Total delay (vehicle-seconds)', better: 'lower', digits: 0 },
  { key: 'served', label: 'Vehicles served', better: 'higher', digits: 0 },
  { key: 'residual', label: 'Residual queue at end', better: 'lower', digits: 0 },
  { key: 'maxQueue', label: 'Longest single queue', better: 'lower', digits: 0 },
  { key: 'maxTotalQueue', label: 'Peak total queue', better: 'lower', digits: 0 },
  { key: 'switches', label: 'Phase switches', better: null, digits: 0 },
];

export function comparisonHtml(results, strategies, scenario) {
  const keys = Object.keys(strategies);
  if (!keys.some((k) => results[k])) return '<p class="muted">No finished runs yet. Run the fixed-time baseline, then the optimized strategy, or use the button below.</p>';
  const base = results.fixed?.summary;
  const fmt = (row, x) => (x == null ? 'N/A' : row.digits ? r1(x) : int(x));
  const head = keys.map((k) => `<th class="num">${strategies[k].short}</th>`).join('');
  const body = METRIC_ROWS.map((row) => {
    const cells = keys.map((k) => {
      if (!results[k]) return '<td class="num muted">not run</td>';
      const x = results[k].summary[row.key];
      if (k === 'fixed' || !base || base[row.key] == null || x == null) return `<td class="num">${fmt(row, x)}</td>`;
      const diff = x - base[row.key];
      if (Math.abs(diff) < 1e-9) return `<td class="num">${fmt(row, x)}<small class="muted">no change</small></td>`;
      const pct = base[row.key] !== 0 ? ` (${diff > 0 ? '+' : '−'}${Math.abs((diff / base[row.key]) * 100).toFixed(1)}%)` : '';
      const better = row.better == null ? '' : (row.better === 'lower' ? diff < 0 : diff > 0) ? 'better' : 'worse';
      return `<td class="num ${better}">${fmt(row, x)}<small>${diff > 0 ? '+' : '−'}${row.digits ? r1(Math.abs(diff)) : int(Math.abs(diff))}${pct}</small></td>`;
    }).join('');
    return `<tr><th scope="row">${row.label}</th>${cells}</tr>`;
  }).join('');
  const ids = [...new Set(keys.filter((k) => results[k]).map((k) => results[k].scheduleId))];
  const note = ids.length === 1
    ? `Both runs used the same arrivals (set ${ids[0]}, seed ${scenario.seed}). Differences against the fixed-time baseline are shown under each value. Green means better, red means worse.`
    : 'These runs used different arrivals and should not be compared.';
  const missing = !base ? '<p class="muted same-note">Run the fixed-time baseline to see differences.</p>' : '';
  return `<table class="compare"><thead><tr><th>Metric</th>${head}</tr></thead><tbody>${body}</tbody></table><p class="muted same-note">${note}</p>${missing}`;
}
