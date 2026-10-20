// Page wiring: buttons -> Player, Player -> render.
import { Player } from './player.js';
import { DIRECTIONS, DIRECTION_NAMES, STRATEGIES } from './labels.js';
import {
  intersectionSvg, intersectionDescription, signalNowHtml, signalNextText, metricsHtml,
  queueScale, barPercent, hasGreen,
} from './render.js';
import { queueChartSvg } from './charts.js';


const $ = (id) => document.getElementById(id);
const el = {
  scenario: $('scenario'), scenarioNote: $('scenario-note'), status: $('status'), resetTop: $('reset-top'),
  clock: $('clock'), svg: $('intersection'), svgDesc: $('intersection-desc'), backendError: $('backend-error'),
  start: $('start'), pause: $('pause'), step: $('step'), reset: $('reset'), speed: $('speed'),
  chart: $('chart'), queueRows: $('queue-rows'), queueError: $('queue-error'),
  strategy: $('strategy'), optimize: $('optimize'), signalNow: $('signal-now'), signalNext: $('signal-next'), message: $('message'),
  metrics: $('metrics'),
};

const STATUS_TEXT = { ready: 'Ready', running: 'Running', paused: 'Paused', completed: 'Completed' };


el.queueRows.innerHTML = DIRECTIONS.map((d) => `
  <tr data-dir="${d}">
    <th scope="row">${DIRECTION_NAMES[d]}</th>
    <td class="num count" data-q></td>
    <td><div class="bar" data-bar role="presentation"><span></span></div></td>
    <td class="num" data-red></td>
  </tr>`).join('');

let syncedVersion = -1;
let busy = false;

function render(p) {
  if (!p.trace) return;
  const v = p.view();
  const running = v.status === 'running';
  const done = v.status === 'completed';

  if (p.version !== syncedVersion) {
    syncedVersion = p.version;
    el.scenario.value = p.scenario.id;
    el.scenarioNote.textContent = p.scenario.note;
    el.queueError.textContent = '';
  }


  el.status.textContent = busy ? 'Working…' : STATUS_TEXT[v.status];
  el.status.dataset.state = v.status;
  el.clock.textContent = `${v.time} / ${v.duration} s`;
  el.svg.innerHTML = intersectionSvg(v);
  el.svgDesc.textContent = intersectionDescription(v);

  const scale = queueScale(v);
  for (const row of el.queueRows.querySelectorAll('tr')) {
    const d = row.dataset.dir;
    const moving = hasGreen(v.signal, d);
    row.querySelector('[data-q]').textContent = v.q[d];
    const bar = row.querySelector('[data-bar]');
    bar.firstElementChild.style.width = `${barPercent(v.q[d], scale)}%`;
    bar.classList.toggle('moving', moving);
    row.querySelector('[data-red]').textContent = moving ? 'green now' : `${v.red[d]} s`;
  }

  el.strategy.value = p.strategy;
  el.signalNow.innerHTML = signalNowHtml(v);
  el.signalNext.textContent = signalNextText(v);
  el.message.textContent = p.message;
  el.metrics.innerHTML = metricsHtml(v.stats, done);

  el.chart.innerHTML = queueChartSvg({ duration: v.duration, current: v.history, currentStrategy: p.strategy, results: p.results, strategies: STRATEGIES });


  const lock = running || busy;
  el.start.disabled = lock || done;
  el.start.textContent = v.status === 'paused' ? 'Resume' : 'Start';
  el.pause.disabled = !running;
  el.step.disabled = lock || done;
  for (const c of [el.scenario, el.strategy, el.optimize]) c.disabled = lock;
  el.reset.disabled = el.resetTop.disabled = busy;

}

const player = new Player({ onChange: render });

// Wrap server calls so the page shows "Working…" and any connection error.
async function call(fn) {
  busy = true;
  render(player);
  try {
    el.backendError.textContent = '';
    return await fn();
  } catch (e) {
    el.backendError.textContent = e.message;
  } finally {
    busy = false;
    render(player);
  }
}

el.start.addEventListener('click', () => player.start());
el.pause.addEventListener('click', () => player.pause());
el.step.addEventListener('click', () => player.step());
el.reset.addEventListener('click', () => player.reset());
el.resetTop.addEventListener('click', () => player.reset());
el.speed.addEventListener('change', () => player.setSpeed(Number(el.speed.value)));
el.strategy.addEventListener('change', () => call(() => player.setStrategy(el.strategy.value)));
el.scenario.addEventListener('change', () => call(() => player.setScenario(el.scenario.value)));
el.optimize.addEventListener('click', () => call(() => player.optimize()));

document.addEventListener('keydown', (e) => {
  if (e.target.closest('input, select, textarea, button, summary')) return;
  if (e.key === ' ') { e.preventDefault(); player.playing ? player.pause() : player.start(); }
  else if (e.key === 'ArrowRight') { e.preventDefault(); player.step(); }
});

el.scenario.innerHTML = '<option>Loading…</option>';
call(async () => {
  await player.init();
  el.scenario.innerHTML = player.presets.map((s) => `<option value="${s.id}">${s.name}</option>`).join('');
  el.scenario.value = player.scenario.id;
});
