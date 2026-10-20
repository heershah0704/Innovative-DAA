// Small hand-rolled SVG line chart: total queue per second.
export const SERIES_COLORS = { fixed: 'var(--muted)', greedy: 'var(--accent)', current: 'var(--ink)' };

function niceMax(v) {
  if (v <= 10) return 10;
  const p = 10 ** Math.floor(Math.log10(v));
  for (const m of [1, 2, 2.5, 5, 10]) if (m * p >= v) return m * p;
  return 10 * p;
}

export function queueChartSvg({ duration, current, currentStrategy, results, strategies }) {
  const W = 440;
  const H = 150;
  const left = 34;
  const right = 6;
  const top = 8;
  const bottom = 20;
  const series = [];
  for (const k of Object.keys(strategies)) {
    if (results[k] && !(k === currentStrategy && current.length === results[k].series.length)) {
      series.push({ key: k, data: results[k].series, color: SERIES_COLORS[k], width: 1.5, dash: '5 4' });
    }
  }
  series.push({ key: 'current', data: current, color: SERIES_COLORS.current, width: 2, dash: '' });

  const ymax = niceMax(Math.max(1, ...series.flatMap((s) => s.data)));
  const xs = (t) => left + (t / Math.max(1, duration)) * (W - left - right);
  const ys = (v) => top + (1 - v / ymax) * (H - top - bottom);
  const out = [];
  for (const f of [0, 0.5, 1]) {
    const v = ymax * f;
    out.push(`<line x1="${left}" x2="${W - right}" y1="${ys(v)}" y2="${ys(v)}" stroke="var(--rule)"/>`);
    out.push(`<text x="${left - 6}" y="${ys(v) + 4}" text-anchor="end" font-size="10" fill="var(--muted)">${Math.round(v)}</text>`);
  }
  for (const f of [0, 0.5, 1]) {
    const t = Math.round(duration * f);
    out.push(`<text x="${xs(t)}" y="${H - 5}" text-anchor="${f === 0 ? 'start' : f === 1 ? 'end' : 'middle'}" font-size="10" fill="var(--muted)">${t} s</text>`);
  }
  for (const s of series) {
    if (s.data.length < 2) continue;
    const pts = s.data.map((v, i) => `${xs(i).toFixed(1)},${ys(v).toFixed(1)}`).join(' ');
    out.push(`<polyline points="${pts}" fill="none" stroke="${s.color}" stroke-width="${s.width}" ${s.dash ? `stroke-dasharray="${s.dash}"` : ''} stroke-linejoin="round"/>`);
  }
  const svg = `<svg viewBox="0 0 ${W} ${H}" role="img" aria-label="Total queue over time for the current run and finished runs">${out.join('')}</svg>`;
  const legend = series
    .map((s) => `<span><i style="border-color:${s.color};${s.dash ? 'border-top-style:dashed' : ''}"></i>${s.key === 'current' ? `This run (${strategies[currentStrategy].short})` : `${strategies[s.key].short}, finished`}</span>`)
    .join('');
  return `${svg}<div class="chart-legend">${legend}</div>`;
}
