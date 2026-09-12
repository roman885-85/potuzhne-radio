/*  ПОТУЖНЕ РАДІО — веб-сторінка.
 *
 *  Одна сторінка на все: сторінки-розділи перемикаються адресою після «#».
 *  Живий стан плеєра приходить websocket'ом yoRadio (/ws), усе дописане —
 *  JSON-ом з /api/state раз на дві секунди. Команди yoRadio йдуть у
 *  websocket, дописані — у /api/set.
 */
'use strict';

const AP = typeof playMode !== 'undefined' && playMode === 'ap';
const VER = typeof yoVersion !== 'undefined' ? yoVersion : '';
const BUILD = typeof prBuild !== 'undefined' ? prBuild : '';

/* ---------------------------------------------------------------- утиліти */
const $ = (s, r = document) => r.querySelector(s);
/*  append() без «null» на сторінці: умовні шматки передаємо як є  */
const _append = Element.prototype.append;
Element.prototype.append = function (...k) { return _append.apply(this, k.flat(Infinity).filter(x => x != null && x !== false)); };
function h(tag, props, ...kids) {
  const el = document.createElement(tag);
  if (props) for (const k in props) {
    const v = props[k];
    if (v == null || v === false) continue;
    if (k === 'class') el.className = v;
    else if (k === 'style' && typeof v === 'object') Object.assign(el.style, v);
    else if (k.startsWith('on') && typeof v === 'function') el.addEventListener(k.slice(2), v);
    else if (k === 'html') el.innerHTML = v;
    else if (k === 'value' || k === 'checked' || k === 'disabled' || k === 'selected') el[k] = v;
    else el.setAttribute(k, v === true ? '' : v);
  }
  for (const c of kids.flat(Infinity)) {
    if (c == null || c === false) continue;
    el.append(c.nodeType ? c : document.createTextNode(String(c)));
  }
  return el;
}
const P = {
  play: '<path d="M7 4.5v15l12-7.5z" fill="currentColor" stroke="none"/>',
  pause: '<rect x="6" y="4.5" width="4" height="15" rx="1" fill="currentColor" stroke="none"/><rect x="14" y="4.5" width="4" height="15" rx="1" fill="currentColor" stroke="none"/>',
  stop: '<rect x="6" y="6" width="12" height="12" rx="1.5" fill="currentColor" stroke="none"/>',
  prev: '<path d="M18 5v14L8 12z" fill="currentColor" stroke="none"/><path d="M6 5v14"/>',
  next: '<path d="M6 5v14l10-7z" fill="currentColor" stroke="none"/><path d="M18 5v14"/>',
  vol: '<path d="M4 9v6h4l5 4V5L8 9z"/><path d="M16.5 8.5a5 5 0 0 1 0 7M19 6a8.5 8.5 0 0 1 0 12"/>',
  vol0: '<path d="M4 9v6h4l5 4V5L8 9z"/><path d="M17 9l5 5M22 9l-5 5"/>',
  radio: '<circle cx="12" cy="11" r="2"/><path d="M8.5 7.5a5 5 0 0 0 0 7M15.5 7.5a5 5 0 0 1 0 7M5.6 4.6a9 9 0 0 0 0 12.8M18.4 4.6a9 9 0 0 1 0 12.8M12 13v8"/>',
  sd: '<path d="M7 3h8l4 4v13a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z"/><path d="M10 3v4M13 3v4M16 4v3"/>',
  book: '<path d="M4 5a2 2 0 0 1 2-2h13v16H6a2 2 0 0 0-2 2z"/><path d="M4 21V5M11.5 6.5v7M9 9h5"/>',
  star: '<path d="M12 3.5l2.6 5.3 5.9.9-4.3 4.1 1 5.8-5.2-2.7-5.2 2.7 1-5.8-4.3-4.1 5.9-.9z"/>',
  starf: '<path d="M12 3.5l2.6 5.3 5.9.9-4.3 4.1 1 5.8-5.2-2.7-5.2 2.7 1-5.8-4.3-4.1 5.9-.9z" fill="currentColor"/>',
  edit: '<path d="M4 20h4L19 9l-4-4L4 16z"/><path d="M13.5 6.5l4 4"/>',
  trash: '<path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13M10 11v6M14 11v6"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  grip: '<circle cx="9" cy="6" r="1.2" fill="currentColor"/><circle cx="15" cy="6" r="1.2" fill="currentColor"/><circle cx="9" cy="12" r="1.2" fill="currentColor"/><circle cx="15" cy="12" r="1.2" fill="currentColor"/><circle cx="9" cy="18" r="1.2" fill="currentColor"/><circle cx="15" cy="18" r="1.2" fill="currentColor"/>',
  search: '<circle cx="11" cy="11" r="6.5"/><path d="M16 16l4.5 4.5"/>',
  up: '<path d="M12 19V5M6 11l6-6 6 6"/>',
  down: '<path d="M12 5v14M6 13l6 6 6-6"/>',
  upload: '<path d="M12 16V4M7 9l5-5 5 5M4 16v3a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-3"/>',
  download: '<path d="M12 4v12M7 11l5 5 5-5M4 16v3a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-3"/>',
  wifi: '<path d="M2.5 9a14 14 0 0 1 19 0M5.5 12.5a9.5 9.5 0 0 1 13 0M8.5 16a5 5 0 0 1 7 0"/><circle cx="12" cy="19.2" r=".9" fill="currentColor"/>',
  lock: '<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>',
  alarm: '<circle cx="12" cy="13" r="7.5"/><path d="M12 9v4l2.5 2M4 4.5L6.5 2.5M20 4.5l-2.5-2"/>',
  screen: '<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4"/>',
  eq: '<path d="M5 20v-6M5 10V4M12 20v-9M12 7V4M19 20v-4M19 12V4M3 14h4M10 7h4M17 16h4"/>',
  gear: '<circle cx="12" cy="12" r="3"/><path d="M12 2.5v3M12 18.5v3M4.2 5.6l2.1 2.1M17.7 16.3l2.1 2.1M2.5 12h3M18.5 12h3M4.2 18.4l2.1-2.1M17.7 7.7l2.1-2.1"/>',
  code: '<path d="M8 7l-5 5 5 5M16 7l5 5-5 5M13.5 4l-3 16"/>',
  refresh: '<path d="M20 11a8 8 0 0 0-14.3-4.5M4 13a8 8 0 0 0 14.3 4.5"/><path d="M5 3v4h4M19 21v-4h-4"/>',
  info: '<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.5"/>',
  menu: '<path d="M4 7h16M4 12h16M4 17h16"/>',
  x: '<path d="M6 6l12 12M18 6L6 18"/>',
  check: '<path d="M5 12.5l4.5 4.5L19 7.5"/>',
  rec: '<circle cx="12" cy="12" r="6" fill="currentColor" stroke="none"/>',
  globe: '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18"/>',
  moon: '<path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/>',
  chip: '<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2.5v3M15 2.5v3M9 18.5v3M15 18.5v3M2.5 9h3M2.5 15h3M18.5 9h3M18.5 15h3"/>',
  clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
  image: '<rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="9" cy="10" r="2"/><path d="M21 16l-5-5-9 9"/>',
  headph: '<path d="M4 15v-3a8 8 0 0 1 16 0v3"/><rect x="3" y="14" width="4" height="7" rx="1.5"/><rect x="17" y="14" width="4" height="7" rx="1.5"/>',
  shuffle: '<path d="M3 7h3.5c2 0 3.2 1 4.5 3l2 4c1.3 2 2.5 3 4.5 3H21M3 17h3.5c1.6 0 2.7-.6 3.7-1.8M14 8.8c1-1.2 2.1-1.8 3.5-1.8H21M18 4l3 3-3 3M18 14l3 3-3 3"/>',
  file: '<path d="M7 3h7l5 5v12a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z"/><path d="M14 3v5h5"/>',
};
function ic(name, cls) {
  const s = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  s.setAttribute('viewBox', '0 0 24 24'); s.setAttribute('fill', 'none'); s.setAttribute('stroke', 'currentColor');
  s.setAttribute('stroke-width', '2'); s.setAttribute('stroke-linecap', 'round'); s.setAttribute('stroke-linejoin', 'round');
  s.setAttribute('aria-hidden', 'true');
  if (cls) s.setAttribute('class', cls);
  s.innerHTML = P[name] || '';
  return s;
}
const pad2 = n => String(n).padStart(2, '0');
function mmss(s) { s = Math.max(0, Math.floor(s || 0)); const hh = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, ss = s % 60; return hh ? `${hh}:${pad2(m)}:${pad2(ss)}` : `${m}:${pad2(ss)}`; }
function size(b) { if (b < 1024) return b + ' Б'; if (b < 1048576) return (b / 1024).toFixed(0) + ' КБ'; if (b < 1073741824) return (b / 1048576).toFixed(1).replace('.', ',') + ' МБ'; return (b / 1073741824).toFixed(1).replace('.', ',') + ' ГБ'; }
function plural(n, one, few, many) { const a = Math.abs(n) % 100, b = a % 10; if (a > 10 && a < 20) return many; if (b > 1 && b < 5) return few; if (b === 1) return one; return many; }
const bytes = s => new TextEncoder().encode(s).length;

let toastT = 0;
function toast(msg, bad) {
  let t = $('.toast');
  if (!t) { t = h('div', { class: 'toast', role: 'status' }); document.body.append(t); }
  t.textContent = msg; t.classList.toggle('bad', !!bad); t.hidden = false;
  clearTimeout(toastT); toastT = setTimeout(() => { t.hidden = true; }, bad ? 5000 : 2600);
}

function fillRange(r) { const mn = +r.min || 0, mx = +r.max || 100; r.style.setProperty('--p', ((r.value - mn) / (mx - mn) * 100) + '%'); }
function range(min, max, val, oninput, onchange, step) {
  const r = h('input', { type: 'range', min, max, step: step || 1, value: val });
  fillRange(r);
  r.addEventListener('input', () => { fillRange(r); oninput && oninput(+r.value, r); });
  if (onchange) r.addEventListener('change', () => onchange(+r.value, r));
  return r;
}
function sw(checked, onchange) {
  const i = h('input', { type: 'checkbox', checked: !!checked });
  i.addEventListener('change', () => onchange(i.checked, i));
  return h('label', { class: 'sw' }, i, h('i'));
}
function seg(items, cur, onpick) {
  const el = h('div', { class: 'seg', role: 'group' });
  items.forEach(([v, label]) => {
    const b = h('button', { type: 'button', class: v === cur ? 'on' : '' }, label);
    b.onclick = () => { [...el.children].forEach(x => x.classList.remove('on')); b.classList.add('on'); onpick(v); };
    b.dataset.v = v;
    el.append(b);
  });
  el.set = v => [...el.children].forEach(x => x.classList.toggle('on', x.dataset.v == String(v)));
  return el;
}
function row(title, sub, ctl) { return h('div', { class: 'row' }, h('div', { class: 'lbl' }, h('b', null, title), sub ? h('small', null, sub) : null), ctl); }
/*  Вибір файлу своєю кнопкою: напис рідного поля залежить від мови браузера  */
function filePick(accept, multiple, label) {
  const inp = h('input', { type: 'file', accept, multiple, hidden: true });
  const name = h('span', { class: 'note', style: { margin: 0, wordBreak: 'break-all' } }, 'файл не вибрано');
  inp.addEventListener('change', () => { const n = [...inp.files].map(f => f.name); name.textContent = n.length ? n.join(', ') : 'файл не вибрано'; });
  const el = h('div', { class: 'bar fld' }, btn(label || 'Вибрати файл', 'file', () => inp.click()), name, inp);
  el.input = inp;
  return el;
}
function card(title, ...kids) { return h('section', { class: 'card' }, title ? h('h2', null, title) : null, ...kids); }
function btn(label, icon, onclick, cls) { return h('button', { type: 'button', class: 'btn ' + (cls || ''), onclick }, icon ? ic(icon) : null, label); }
function ibtn(icon, title, onclick, cls) { return h('button', { type: 'button', class: 'btn ico ' + (cls || ''), title, 'aria-label': title, onclick }, ic(icon)); }

function dialog(title, body, buttons, opts) {
  const box = h('div', { class: 'box', role: 'dialog', 'aria-modal': 'true', 'aria-label': title });
  const d = h('div', { class: 'dlg' }, box);
  const close = () => { d.remove(); document.removeEventListener('keydown', esc); opts && opts.onclose && opts.onclose(); };
  const esc = e => { if (e.key === 'Escape') close(); };
  document.addEventListener('keydown', esc);
  d.addEventListener('mousedown', e => { if (e.target === d) close(); });
  box.append(h('h3', null, title), body, h('div', { class: 'foot' }, buttons.map(b => b === '|' ? h('span', { class: 'sp' }) : b)));
  document.body.append(d);
  const f = box.querySelector('input:not([type=checkbox]),select,textarea'); if (f) setTimeout(() => f.focus(), 30);
  d.close = close;
  return d;
}
function confirmBox(title, text, okLabel, danger) {
  return new Promise(res => {
    let done = false;
    const d = dialog(title, h('p', { style: { color: 'var(--dim)', margin: 0 } }, text), [
      btn('Скасувати', null, () => { done = true; d.close(); res(false); }, 'ghost'),
      btn(okLabel || 'Так', null, () => { done = true; d.close(); res(true); }, danger ? 'acc bad' : 'acc'),
    ], { onclose: () => { if (!done) res(false); } });
  });
}
function download(name, text, type) {
  const a = h('a', { href: URL.createObjectURL(new Blob([text], { type: type || 'text/plain' })), download: name });
  document.body.append(a); a.click(); setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
}

/* ---------------------------------------------------------------- зв'язок */
async function api(path, opts) {
  const r = await fetch(path, Object.assign({ cache: 'no-store' }, opts));
  const t = await r.text();
  let j = {};
  try { j = t ? JSON.parse(t) : {}; } catch (e) { j = { raw: t }; }
  if (!r.ok) throw new Error(j.err || ('помилка ' + r.status));
  return j;
}
function setx(obj) {
  const b = new URLSearchParams();
  for (const k in obj) b.append(k, obj[k]);
  return api('/api/set', { method: 'POST', body: b }).catch(e => toast('Радіо не прийняло команду: ' + e.message, true));
}

/*  Стан: W — websocket yoRadio, S — /api/state, C — налаштування yoRadio  */
const W = { name: '', title: '', vol: 0, playing: false, cur: 0, bitrate: 0, fmt: '', mode: 0, bass: 0, middle: 0, trebble: 0, balance: 0, sdpos: 0, sdmin: 0, sdmax: 0, sdtpos: 0, sdtend: 0, snuffle: 0 };
let S = null;
const C = {};
let ws = null, wsOk = false;
const wsQueue = [];

function wsConnect() {
  try { ws = new WebSocket(`ws://${location.host}/ws`); } catch (e) { setTimeout(wsConnect, 3000); return; }
  ws.onopen = () => { wsOk = true; ws.send('getindex=1'); while (wsQueue.length) ws.send(wsQueue.shift()); topStat(); };
  ws.onclose = () => { wsOk = false; topStat(); setTimeout(wsConnect, 2500); };
  ws.onerror = () => { try { ws.close(); } catch (e) {} };
  ws.onmessage = e => onWs(e.data);
}
function send(cmd) {
  if (ws && ws.readyState === 1) ws.send(cmd);
  else { wsQueue.push(cmd); if (wsQueue.length > 20) wsQueue.shift(); }
}
/*  Назву пісні плата шле як є — лапки всередині ламають JSON. Лагодимо.  */
function wsParse(t) {
  try { return JSON.parse(t); } catch (e) {}
  const m = t.match(/^\{"payload":\[\{"id":"(\w+)",\s*"value":\s*"([\s\S]*)"\}\]\}$/);
  if (m) return { payload: [{ id: m[1], value: m[2].replace(/\\(?!["\\/bfnrtu])/g, '\\\\') }] };
  return null;
}
function onWs(t) {
  const d = wsParse(t);
  if (!d) return;
  let what = 'misc';
  if (d.payload) {
    for (const { id, value } of d.payload) {
      if (id === 'nameset') { W.name = value; what = 'np'; }
      else if (id === 'meta') { W.title = value; what = 'np'; }
      else if (id === 'volume') { W.vol = +value; what = 'vol'; }
      else if (id === 'playerwrap') { W.playing = value === 'playing'; what = 'np'; }
      else if (id === 'bitrate') { W.bitrate = +value; what = 'np'; }
      else if (id === 'fmt') { W.fmt = value; what = 'np'; }
      else if (id in W) { W[id] = +value; what = 'eq'; }
    }
  } else if ('current' in d) { W.cur = +d.current; what = 'cur'; }
  else if ('file' in d) { what = 'playlist'; }
  else if ('playermode' in d) { W.mode = d.playermode === 'modesd' ? 1 : 0; what = 'mode'; }
  else if ('sdpos' in d) { Object.assign(W, { sdpos: +d.sdpos, sdtpos: +d.sdtpos, sdtend: +d.sdtend }); what = 'sd'; }
  else if ('sdmin' in d) { W.sdmin = +d.sdmin; W.sdmax = +d.sdmax; what = 'sd'; }
  else if ('snuffle' in d) { W.snuffle = +d.snuffle; what = 'sd'; }
  else if ('pong' in d || 'act' in d || 'dspontrue' in d) return;
  else { Object.assign(C, d); what = 'cfg'; }
  bus(what);
}

const listeners = new Set();
function bus(what) {
  if (what === 'playlist' || what === 'mode') loadPlayList();
  miniUpdate(); topStat();
  for (const f of listeners) try { f(what); } catch (e) { console.error(e); }
}

let stT = 0, stFails = 0, webStamp = 0, webWarned = false;
async function pollState() {
  clearTimeout(stT);
  try {
    S = await api('/api/state'); stFails = 0;
    /*  на радіо залили нові файли сторінки — перезавантажитись самим,
        щоб ніде не лишалось старої сторінки (незбережений список не губимо)  */
    if (S.web) {
      if (!webStamp) webStamp = S.web;
      else if (S.web !== webStamp) { if (!edDirty()) { location.reload(); return; } if (!webWarned) { webWarned = true; toast('Сторінку на радіо оновлено. Збережіть список станцій і оновіть сторінку.', true); } }
    }
    W.mode = S.mode;
    if (!wsOk) { W.name = S.name; W.title = S.title; W.vol = S.vol; W.playing = !!S.play; W.cur = S.idx; }
    if (S.logo !== LOGO.ver && LOGO.ver !== -1) loadLogos();
    if (S.msg) { toast(S.msg, true); setx({ msgClr: 1 }); }
    bus('state');
  } catch (e) { stFails++; if (stFails === 3) toast('Радіо не відповідає', true); topStat(); }
  stT = setTimeout(pollState, document.hidden ? 8000 : 2000);
}
document.addEventListener('visibilitychange', () => { if (!document.hidden) pollState(); });

/* ---------------------------------------------------------------- логотипи */
const CRC_T = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
function crc32(str) { let c = 0xFFFFFFFF; for (const b of new TextEncoder().encode(str)) c = CRC_T[(c ^ b) & 255] ^ (c >>> 8); return ((c ^ 0xFFFFFFFF) >>> 0).toString(16).padStart(8, '0'); }
const PAL = [0x3A8D, 0x5A4B, 0x2C6A, 0x6A28, 0x2B0F, 0x7A6C, 0x4B09, 0x31CC].map(v => `rgb(${((v >> 11) & 31) * 255 / 31 | 0},${((v >> 5) & 63) * 255 / 63 | 0},${(v & 31) * 255 / 31 | 0})`);
const LOGO = { ver: -1, have: new Set(), url: new Map(), q: [], busy: 0 };
async function loadLogos() {
  try { const j = await api('/api/logos'); const changed = j.v !== LOGO.ver; LOGO.ver = j.v; LOGO.have = new Set(j.f); if (changed) { LOGO.url.clear(); bus('logos'); } } catch (e) {}
}
function initials(name) {
  const w = (name || '').split(/\s+/).map(x => x.replace(/^[^\p{L}\p{N}]+/u, '')).filter(Boolean);
  return w.slice(0, 2).map(x => [...x][0]).join('').toUpperCase() || '?';
}
function logoEl(name, url, cls) {
  const crc = crc32(url || '');
  const el = h('div', { class: 'logo ' + (cls || '') });
  const put = src => { el.textContent = ''; el.style.background = '#fff'; el.append(h('img', { src, alt: '' })); };
  const tile = () => { el.style.background = PAL[parseInt(crc.slice(-1), 16) & 7]; el.textContent = initials(name); };
  if (LOGO.url.has(crc)) { put(LOGO.url.get(crc)); return el; }
  tile();
  if (LOGO.have.has(crc + '.565')) {
    LOGO.q.push({ crc, cb: put }); logoPump();
  } else if (LOGO.have.has(crc + '.jpg')) {
    const u = `/logo/${crc}.jpg?v=${LOGO.ver}`; LOGO.url.set(crc, u); put(u);
  }
  return el;
}
function logoPump() {
  while (LOGO.busy < 3 && LOGO.q.length) {
    const { crc, cb } = LOGO.q.shift();
    if (LOGO.url.has(crc)) { cb(LOGO.url.get(crc)); continue; }
    LOGO.busy++;
    fetch(`/logo/${crc}.565?v=${LOGO.ver}`).then(r => r.ok ? r.arrayBuffer() : null).then(buf => {
      if (!buf || buf.byteLength !== 45 * 45 * 2) return;
      const cv = h('canvas', { width: 45, height: 45 }), cx = cv.getContext('2d'), im = cx.createImageData(45, 45), dv = new DataView(buf);
      for (let i = 0; i < 2025; i++) { const v = dv.getUint16(i * 2, true); im.data[i * 4] = ((v >> 11) & 31) * 255 / 31; im.data[i * 4 + 1] = ((v >> 5) & 63) * 255 / 63; im.data[i * 4 + 2] = (v & 31) * 255 / 31; im.data[i * 4 + 3] = 255; }
      cx.putImageData(im, 0, 0);
      const u = cv.toDataURL(); LOGO.url.set(crc, u); cb(u);
    }).catch(() => {}).finally(() => { LOGO.busy--; logoPump(); });
  }
}
/*  Свій логотип: картинку з комп'ютера зменшуємо до 45x45 тут, у браузері,
    і шлемо плати готові пікселі — тими ж, що вона зберігає сама.  */
function uploadLogo(url, file) {
  return new Promise((res, rej) => {
    const img = new Image();
    img.onload = async () => {
      const cv = h('canvas', { width: 45, height: 45 }), cx = cv.getContext('2d');
      cx.fillStyle = '#fff'; cx.fillRect(0, 0, 45, 45);
      const k = Math.min(45 / img.width, 45 / img.height), w = img.width * k, hh = img.height * k;
      cx.imageSmoothingQuality = 'high';
      cx.drawImage(img, (45 - w) / 2, (45 - hh) / 2, w, hh);
      const d = cx.getImageData(0, 0, 45, 45).data, out = new DataView(new ArrayBuffer(4050));
      for (let i = 0; i < 2025; i++) out.setUint16(i * 2, ((d[i * 4] & 0xF8) << 8) | ((d[i * 4 + 1] & 0xFC) << 3) | (d[i * 4 + 2] >> 3), true);
      const fd = new FormData(); fd.append('logo', new Blob([out.buffer]), crc32(url) + '.565');
      try { await api('/api/logo', { method: 'POST', body: fd }); await loadLogos(); res(); } catch (e) { rej(e); }
      URL.revokeObjectURL(img.src);
    };
    img.onerror = () => rej(new Error('це не картинка'));
    img.src = URL.createObjectURL(file);
  });
}

/* ---------------------------------------------------------------- станції */
/*  PL — те, що бачить плеєр (у режимі картки — треки картки);
    ED — список радіостанцій у редакторі, зі своїми незбереженими змінами.  */
let PL = [];
const ED = { list: [], orig: '', loaded: false };
function parseCSVText(t) {
  const out = [];
  for (let line of t.replace(/^﻿/, '').split(/\r?\n/)) {
    if (!line.trim()) continue;
    const p = line.split('\t');
    if (p.length >= 2) out.push({ name: p[0].trim(), url: p[1].trim(), ovol: parseInt(p[2]) || 0 });
  }
  return out;
}
const serialize = l => l.map(s => `${s.name}\t${s.url}\t${s.ovol | 0}`).join('\n') + (l.length ? '\n' : '');
const edDirty = () => ED.loaded && serialize(ED.list) !== ED.orig;
async function loadPlayList() {
  try { const t = await (await fetch('/data/playlist.csv', { cache: 'no-store' })).text(); PL = parseCSVText(t); bus('pl'); } catch (e) {}
}
async function loadEditor(force) {
  if (ED.loaded && !force) return;
  const t = await (await fetch('/api/stations', { cache: 'no-store' })).text();
  ED.list = parseCSVText(t); ED.orig = serialize(ED.list); ED.loaded = true;
}
window.addEventListener('beforeunload', e => { if (edDirty()) { e.preventDefault(); e.returnValue = ''; } });

/*  Імпорт: CSV плати, M3U/M3U8, PLS, JSON старих версій, просто адреси.  */
function parseAny(text, fname) {
  text = text.replace(/^﻿/, '');
  const out = [], isUrl = u => /^https?:\/\/\S+$/i.test(u);
  const nameFromUrl = u => { try { return new URL(u).hostname.replace(/^www\./, ''); } catch (e) { return u; } };
  const t = text.trim();
  if (/^\[playlist\]/i.test(t) || /\.pls$/i.test(fname)) {
    const f = {}, ti = {};
    t.split(/\r?\n/).forEach(l => { let m = l.match(/^File(\d+)=(.+)$/i); if (m) f[m[1]] = m[2].trim(); m = l.match(/^Title(\d+)=(.+)$/i); if (m) ti[m[1]] = m[2].trim(); });
    Object.keys(f).sort((a, b) => a - b).forEach(k => { if (isUrl(f[k])) out.push({ name: ti[k] || nameFromUrl(f[k]), url: f[k], ovol: 0 }); });
    return out;
  }
  if (/^\[\s*\{/.test(t) || /^\{/.test(t)) {
    let arr = [];
    try { const j = JSON.parse(t); arr = Array.isArray(j) ? j : [j]; }
    catch (e) { t.split(/\r?\n/).forEach(l => { try { arr.push(JSON.parse(l)); } catch (e2) {} }); }
    for (const o of arr) { const u = (o.url_resolved || o.url || '').trim(); if (isUrl(u)) out.push({ name: (o.name || nameFromUrl(u)).trim(), url: u, ovol: parseInt(o.ovol) || 0 }); }
    return out;
  }
  if (/#EXTM3U|#EXTINF/i.test(t) || /\.m3u8?$/i.test(fname)) {
    let nm = '';
    t.split(/\r?\n/).forEach(l => {
      l = l.trim();
      if (/^#EXTINF/i.test(l)) { nm = l.replace(/^#EXTINF:[^,]*,/i, '').trim(); }
      else if (isUrl(l)) { out.push({ name: nm || nameFromUrl(l), url: l, ovol: 0 }); nm = ''; }
    });
    return out;
  }
  t.split(/\r?\n/).forEach(l => {
    l = l.trim(); if (!l || l[0] === '#') return;
    let p = l.split('\t');
    if (p.length >= 2 && isUrl(p[1].trim())) { out.push({ name: p[0].trim() || nameFromUrl(p[1].trim()), url: p[1].trim(), ovol: parseInt(p[2]) || 0 }); return; }
    const m = l.match(/^(.*?)[;,|]\s*(https?:\/\/[^\s;,|]+)\s*(?:[;,|]\s*(-?\d+))?\s*$/i);
    if (m) { out.push({ name: m[1].trim().replace(/^"|"$/g, '') || nameFromUrl(m[2]), url: m[2], ovol: parseInt(m[3]) || 0 }); return; }
    if (isUrl(l)) out.push({ name: nameFromUrl(l), url: l, ovol: 0 });
  });
  return out;
}
function cleanSt(s) {
  let name = (s.name || '').replace(/[\t\r\n]+/g, ' ').trim(), url = (s.url || '').replace(/\s+/g, '');
  while (bytes(name) > 160) name = [...name].slice(0, -1).join('');
  return { name, url, ovol: Math.max(-64, Math.min(64, parseInt(s.ovol) || 0)) };
}

/* ---------------------------------------------------------------- каркас */
const PAGES = [
  { id: 'player', t: 'Плеєр', d: 'що грає, гучність, керування', i: 'play' },
  { id: 'stations', t: 'Станції', d: 'список, додати, змінити, імпорт і експорт', i: 'radio' },
  { id: 'fav', t: 'Обране', d: 'шість швидких кнопок', i: 'star' },
  { id: 'sermons', t: 'Проповіді', d: 'архів проповідей із сайту церкви', i: 'book' },
  { id: 'records', t: 'Запис і картка', d: 'запис ефіру, файли на картці', i: 'sd' },
  { g: 'Налаштування' },
  { id: 'alarm', t: 'Будильник і сон', d: 'розбудити й приспати', i: 'alarm' },
  { id: 'screen', t: 'Екран', d: 'яскравість, ніч, економія, світлодіод', i: 'screen' },
  { id: 'sound', t: 'Звук', d: 'еквалайзер, баланс, кроки гучності', i: 'eq' },
  { id: 'wifi', t: 'Wi-Fi', d: 'мережі, пошук, підключення', i: 'wifi' },
  { id: 'time', t: 'Час і погода', d: 'часовий пояс, сервери, погода', i: 'clock' },
  { id: 'system', t: 'Система', d: 'поведінка радіо, перезавантаження', i: 'gear' },
  { id: 'dev', t: 'Розробник', d: 'батарея, IP, картка, аудіовихід, логотипи', i: 'code' },
  { id: 'update', t: 'Оновлення', d: 'прошивка й файли сторінки', i: 'upload' },
  { id: 'about', t: 'Про пристрій', d: 'версія, пам\'ять, мережа, батарея', i: 'info' },
];
const APPAGES = ['wifi', 'update', 'about'];
let curPage = '', pageLive = null;

function shell() {
  const nav = h('nav', { class: 'nav', 'aria-label': 'Розділи' });
  for (const p of PAGES) {
    if (p.g) { if (!AP) nav.append(h('div', { class: 'grp' }, p.g)); continue; }
    if (AP && !APPAGES.includes(p.id)) continue;
    nav.append(h('a', { href: '#/' + p.id, 'data-id': p.id }, ic(p.i), h('span', null, p.t), h('small', null, p.d)));
  }
  const logo = h('span', { html: '<svg width="30" height="30" viewBox="0 0 24 24" fill="none" stroke="#e6d25a" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="11" r="2"/><path d="M8.5 7.5a5 5 0 0 0 0 7M15.5 7.5a5 5 0 0 1 0 7M5.6 4.6a9 9 0 0 0 0 12.8M18.4 4.6a9 9 0 0 1 0 12.8M12 13v8"/></svg>' });
  const app = h('div', { class: 'shell', id: 'shell' },
    h('aside', { class: 'side' }, h('div', { class: 'brand' }, logo, h('div', null, h('b', null, 'ПОТУЖНЕ РАДІО'), h('small', null, VER ? `версія ${VER}${BUILD ? ' · ' + BUILD.slice(0, 10) : ''}` : ''))), nav),
    h('div', { class: 'scrim', onclick: () => $('#shell').classList.remove('open') }),
    h('div', { class: 'main' },
      h('header', { class: 'top' },
        h('button', { class: 'burger', type: 'button', 'aria-label': 'Розділи', onclick: () => $('#shell').classList.toggle('open') }, ic('menu')),
        h('h1', { id: 'ptitle' }, ''),
        h('div', { class: 'stat', id: 'stat' })),
      h('div', { id: 'page' })),
    AP ? null : mini());
  $('#app').replaceWith(app);
}

function route() {
  let id = (location.hash.match(/^#\/(\w+)/) || [])[1];
  if (!id) id = AP ? 'wifi' : (location.pathname.includes('update') ? 'update' : location.pathname.includes('settings') ? 'wifi' : 'player');
  if (AP && !APPAGES.includes(id)) id = 'wifi';
  const p = PAGES.find(x => x.id === id) || PAGES[0];
  curPage = p.id;
  document.querySelectorAll('.nav a').forEach(a => a.classList.toggle('on', a.dataset.id === p.id));
  $('#ptitle').textContent = p.t;
  document.title = p.t + ' · ПОТУЖНЕ РАДІО';
  $('#shell').classList.remove('open');
  if (pageLive) listeners.delete(pageLive);
  pageLive = null;
  const root = $('#page');
  root.textContent = '';
  const page = h('div', { class: 'page' });
  root.append(page);
  (VIEWS[p.id] || VIEWS.player)(page);
  window.scrollTo(0, 0);
}
function live(fn) { pageLive = fn; listeners.add(fn); fn('init'); }

/* ---------------------------------------------------------------- шапка */
function batSvg(pct, chg) {
  const col = pct < 10 ? '#ef5b5b' : pct < 25 ? '#f0a53a' : pct < 50 ? '#e6d25a' : '#5ccf6a';
  const w = Math.max(1, Math.round(18 * Math.min(100, Math.max(0, pct)) / 100));
  return `<svg viewBox="0 0 26 14"><rect x="1" y="1" width="21" height="12" rx="2.5" fill="none" stroke="#8c8c8c" stroke-width="1.5"/><rect x="23" y="4.5" width="2" height="5" rx="1" fill="#8c8c8c"/><rect x="2.8" y="2.8" width="${w}" height="8.4" rx="1.2" fill="${col}"/>${chg ? '<path d="M12.5 2.5L8.5 7.6h3l-1 4 4-5.1h-3z" fill="#fff" stroke="#000" stroke-width=".6"/>' : ''}</svg>`;
}
function sigSvg(rssi) {
  const n = !rssi ? 0 : rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
  let s = '<svg viewBox="0 0 18 14">';
  for (let i = 0; i < 4; i++) s += `<rect x="${i * 4.5}" y="${11 - i * 3}" width="3" height="${3 + i * 3}" rx=".8" fill="${i < n ? '#f2f2f2' : '#444'}"/>`;
  return s + '</svg>';
}
function topStat() {
  const el = $('#stat'); if (!el) return;
  el.textContent = '';
  if (S) {
    if (S.rec && S.rec.on) el.append(h('span', { title: 'Іде запис' }, h('i', { class: 'recdot' }), mmss(S.rec.sec)));
    if (S.bat && !S.dev.noBat && S.bat.pct >= 0) {
      const t = S.bat.chg ? 'заряджається' : S.bat.full ? 'заряджено' : '';
      el.append(h('span', { class: 'batt', title: `Батарея ${S.bat.pct}% · ${(S.bat.mv / 1000).toFixed(2)} В${t ? ' · ' + t : ''}` }, h('span', { html: batSvg(S.bat.pct, S.bat.chg) }), S.bat.pct + '%'));
    }
    if (S.rssi) el.append(h('span', { class: 'sig', title: `${S.ssid} · ${S.rssi} дБм`, html: sigSvg(S.rssi) }));
    if (S.time) el.append(h('span', { class: 'clock' }, S.time));
  }
  el.append(h('i', { class: 'conn' + (wsOk && stFails < 3 ? ' ok' : ''), title: wsOk ? 'Радіо на зв\'язку' : 'Немає зв\'язку з радіо' }));
}

/* ---------------------------------------------------------------- плеєр */
function isSerm() { return S && S.serm && S.serm.on; }
function srcInfo() {
  if (isSerm()) return { i: 'book', t: 'Проповідь' };
  if (W.mode === 1) return { i: 'sd', t: 'Картка пам\'яті' };
  return { i: 'radio', t: 'Радіо' };
}
function coverEl(big) {
  if (isSerm() && S.serm.c) {
    const site = S.sermSite || 'https://site.roman-home.keenetic.pro';
    const src = /^https?:/.test(S.serm.c) ? S.serm.c : site + S.serm.c;
    return h('div', { class: 'cover wide' }, h('img', { src, alt: '' }));
  }
  if (W.mode === 1) return h('div', { class: 'cover' + (big ? '' : ' logo') }, ic('sd'));
  const st = PL[W.cur - 1];
  if (!st) return h('div', { class: 'cover' }, ic('radio'));
  /*  той самий елемент, що й у списку: логотип довантажується в нього сам  */
  return logoEl(st.name, st.url, big ? 'cover' : '');
}
function mini() {
  return h('div', { class: 'mini', id: 'mini' });
}
let miniSig = '';
function miniUpdate() {
  const m = $('#mini'); if (!m) return;
  const serm = isSerm();
  const name = serm ? S.serm.t : (W.name || 'Нічого не грає');
  const sub = serm ? S.serm.p : W.title;
  const sig = [name, sub, W.playing, W.mode, W.cur, serm, LOGO.url.size, S && S.rec && S.rec.on].join('|');
  if (sig !== miniSig) {
    miniSig = sig;
    m.textContent = '';
    const c = coverEl(false); c.classList.add('logo');
    m.append(c,
      h('div', { class: 'tx', onclick: () => { location.hash = '#/player'; } }, h('b', null, S && S.rec && S.rec.on ? h('i', { class: 'recdot', title: 'Іде запис' }) : null, name), h('small', null, sub || '')),
      ibtn('prev', 'Попередня', () => serm ? setx({ sermRel: -1 }) : send('prev=1'), 'ghost'),
      ibtn(W.playing ? 'pause' : 'play', W.playing ? 'Пауза' : 'Грати', () => send('toggle=1'), 'acc'),
      ibtn('next', 'Наступна', () => serm ? setx({ sermRel: 1 }) : send('next=1'), 'ghost'),
      h('div', { class: 'v' }, ic('vol'), volRange('mv')));
  }
  const r = $('#mv'); if (r && document.activeElement !== r) { r.value = W.vol; fillRange(r); }
}
let volT = 0;
function volRange(id) {
  const r = range(0, 254, W.vol, v => { W.vol = v; clearTimeout(volT); volT = setTimeout(() => send('volume=' + v), 60); const o = $('#vo'); if (o) o.textContent = Math.round(v / 2.54) + '%'; });
  r.id = id; r.setAttribute('aria-label', 'Гучність');
  return r;
}

const VIEWS = {};
VIEWS.player = page => {
  const np = h('div', { class: 'np' });
  const ctl = h('div', { class: 'ctl' });
  const seek = h('div', { class: 'seek' });
  const vol = h('div', { class: 'vol' });
  const top = card(null, np, ctl, seek, vol);
  const src = h('div');
  const rec = h('div');
  const sleep = h('div');
  const favs = h('div', { class: 'favs' });
  page.append(top,
    card('Джерело', src),
    card('Обране', favs, h('p', { class: 'note' }, 'Торкніться, щоб увімкнути. Змінити кнопки — у розділі ', h('a', { href: '#/fav' }, 'Обране'), '.')),
    h('div', { class: 'grid2' }, card('Таймер сну', sleep), card('Запис ефіру', rec)));

  let sig = '', seekDrag = false;
  const vr = volRange('pv');
  vol.append(ic('vol0'), vr, h('output', { id: 'vo' }, Math.round(W.vol / 2.54) + '%'));
  const sleepSeg = seg([[0, 'Вимк'], [15, '15'], [30, '30'], [60, '60'], [90, '90']], -1, v => setx({ sleep: v }));
  const sleepNote = h('p', { class: 'note' });
  sleep.append(sleepSeg, sleepNote);

  live(what => {
    const serm = isSerm();
    const s2 = [W.name, W.title, W.playing, W.mode, W.cur, W.bitrate, W.fmt, serm, serm && S.serm.t, LOGO.url.size, PL.length].join('|');
    if (s2 !== sig) {
      sig = s2;
      const si = srcInfo();
      np.textContent = '';
      np.append(coverEl(true), h('div', null,
        h('div', { class: 'src' }, ic(si.i), si.t, W.playing ? '' : ' · пауза'),
        h('h3', null, serm ? S.serm.t : (W.name || 'Нічого не грає')),
        h('div', { class: 'ttl' }, serm ? S.serm.p : (W.title && W.title !== W.name ? W.title : '')),
        h('div', { class: 'meta' }, W.bitrate && W.playing ? `${W.bitrate} кбіт/с${W.fmt && W.fmt !== 'bitrate' ? ' · ' + W.fmt : ''}` : '')));
      ctl.textContent = '';
      ctl.append(
        ibtn('prev', serm ? 'Попередня проповідь' : 'Попередня', () => serm ? setx({ sermRel: -1 }) : send('prev=1')),
        ibtn(W.playing ? 'pause' : 'play', W.playing ? 'Пауза' : 'Грати', () => send('toggle=1'), 'acc'),
        ibtn('next', serm ? 'Наступна проповідь' : 'Наступна', () => serm ? setx({ sermRel: 1 }) : send('next=1')));
    }
    /*  перемотка: проповідь — секунди з сайту; картка — байти файлу  */
    if (!seekDrag) {
      seek.textContent = '';
      if (serm && S.serm.dur) {
        const r = range(0, S.serm.dur, S.serm.pos, v => { seekDrag = true; t1.textContent = mmss(v); }, v => { seekDrag = false; setx({ seek: Math.max(1, v) }); });
        const t1 = h('span', null, mmss(S.serm.pos));
        seek.append(r, h('div', { class: 't' }, t1, h('span', null, mmss(S.serm.dur))));
      } else if (W.mode === 1 && W.sdmax > W.sdmin) {
        const r = range(W.sdmin, W.sdmax, W.sdpos || W.sdmin, () => { seekDrag = true; }, v => { seekDrag = false; send('sdpos=' + v); });
        seek.append(r, h('div', { class: 't' }, h('span', null, mmss(W.sdtpos)), h('span', null, mmss(W.sdtend))));
      }
    }
    if (document.activeElement !== vr) { vr.value = W.vol; fillRange(vr); $('#vo').textContent = Math.round(W.vol / 2.54) + '%'; }
    if (!S) return;
    /*  джерело  */
    const ssig = [W.mode, S.dev.noSd, W.snuffle, serm].join('|');
    if (src.dataset.sig !== ssig) {
      src.dataset.sig = ssig; src.textContent = '';
      const items = [[0, 'Радіо']]; if (!S.dev.noSd) items.push([1, 'Картка пам\'яті']);
      src.append(h('div', { class: 'bar' }, seg(items, W.mode, v => { send('newmode=' + v); }),
        h('span', { class: 'sp' }),
        W.mode === 1 ? btn(W.snuffle ? 'Перемішано' : 'Підряд', 'shuffle', () => send('snuffle=' + (W.snuffle ? 'false' : 'true')), W.snuffle ? 'acc sm' : 'sm') : null,
        h('a', { class: 'btn sm', href: '#/sermons' }, ic('book'), 'Проповіді')));
      if (S.dev.noSd) src.append(h('p', { class: 'note' }, 'Функції картки вимкнено в розділі «Розробник».'));
    }
    /*  таймер сну  */
    sleepSeg.set(S.sleep.set);
    sleepNote.textContent = S.sleep.set ? `Вимкнеться через ${mmss(S.sleep.left)}. Останні 20 секунд звук плавно стихає.` : 'Радіо гратиме, доки його не вимкнуть.';
    /*  запис  */
    const rsig = [S.rec.on, S.rec.sec, S.rec.err, W.mode, serm, S.dev.noSd].join('|');
    if (rec.dataset.sig !== rsig) {
      rec.dataset.sig = rsig; rec.textContent = '';
      if (S.rec.on) {
        rec.append(h('div', { class: 'bar' }, h('span', null, h('i', { class: 'recdot' }), `Записую ${mmss(S.rec.sec)} · ${size(S.rec.bytes)}`), h('span', { class: 'sp' }), btn('Зупинити', 'stop', () => setx({ rec: 0 }), 'sm')));
        rec.append(h('p', { class: 'note' }, S.rec.file));
      } else {
        const can = W.mode === 0 && !serm && W.playing && !S.dev.noSd;
        rec.append(h('div', { class: 'bar' }, btn('Записати ефір', 'rec', () => setx({ rec: 1 }), can ? 'sm' : 'sm'), h('span', { class: 'sp' }), h('a', { href: '#/records', class: 'btn sm ghost' }, 'Записи')));
        rec.append(h('p', { class: 'note' }, S.dev.noSd ? 'Функції картки вимкнено.' : !W.playing ? 'Спершу увімкніть радіостанцію.' : W.mode ? 'Записати можна лише радіо.' : 'Потік пишеться на картку як є, без втрати якості.'));
      }
    }
    /*  обране  */
    const fsig = JSON.stringify(S.fav) + S.favOn;
    if (favs.dataset.sig !== fsig) {
      favs.dataset.sig = fsig; favs.textContent = '';
      S.fav.forEach((f, i) => favs.append(f.n
        ? h('button', { type: 'button', class: 'fav' + (S.favOn === i ? ' on' : ''), onclick: () => setx({ favPlay: i }) }, h('span', { class: 'n' }, f.n), h('span', { class: 'k' }, S.favOn === i ? 'грає' : String(i + 1)))
        : h('div', { class: 'fav empty' }, h('span', { class: 'n' }, 'порожньо'), h('span', { class: 'k' }, String(i + 1)))));
    }
  });
};

/* ---------------------------------------------------------------- станції */
/*  Вкладки розділу «Станції»: радіостанції (редактор) і треки картки.  */
let stTab = null;
function stTabs(page, cur) {
  if (S && S.dev.noSd) return null;
  return h('div', { style: { marginBottom: '14px' } }, seg([['radio', 'Радіостанції'], ['sd', 'Картка пам\'яті']], cur, v => {
    stTab = v; page.textContent = ''; (v === 'sd' ? VIEWS.sdlist : VIEWS.stations)(page);
  }));
}

/*  Треки картки — як список станцій: пошук, поточний підсвічено, дотик — грати.  */
VIEWS.sdlist = page => {
  if (pageLive) { listeners.delete(pageLive); pageLive = null; }
  const tabs = stTabs(page, 'sd');
  const q = h('input', { type: 'search', placeholder: 'Пошук за назвою чи текою', 'aria-label': 'Пошук' });
  const info = h('span', { class: 'note', style: { margin: 0 } });
  const bar = h('div', { class: 'bar' });
  const list = h('div', { class: 'list' });
  page.append(tabs, h('div', { class: 'tools' }, h('div', { class: 'bar' }, h('div', { class: 'search' }, ic('search'), q), bar), h('div', { style: { marginTop: '8px' } }, info)), list);
  let limit = 100, lastSig = '';
  const dir = p => { const k = p.lastIndexOf('/'); return k > 0 ? p.slice(0, k) : ''; };
  function draw() {
    bar.textContent = '';
    list.textContent = '';
    if (W.mode !== 1) {
      info.textContent = '';
      list.append(h('div', { class: 'none' }, h('p', { style: { marginTop: 0 } }, 'Зараз радіо грає не з картки. Перейдіть на картку — і тут з\'явиться список треків.'),
        btn('Перейти на картку', 'sd', () => { send('newmode=1'); list.textContent = ''; list.append(h('div', { class: 'none' }, 'Перемикаю й читаю картку…')); }, 'acc')));
      return;
    }
    bar.append(btn(W.snuffle ? 'Перемішано' : 'Підряд', 'shuffle', () => send('snuffle=' + (W.snuffle ? 'false' : 'true')), W.snuffle ? 'acc sm' : 'sm'),
      btn('Радіо', 'radio', () => send('newmode=0'), 'sm ghost'));
    const f = q.value.trim().toLowerCase();
    const shown = [];
    PL.forEach((t, i) => { if (!f || t.name.toLowerCase().includes(f) || t.url.toLowerCase().includes(f)) shown.push(i); });
    info.textContent = shown.length === PL.length ? `${PL.length} ${plural(PL.length, 'трек', 'треки', 'треків')} на картці` : `Знайдено ${shown.length} з ${PL.length}`;
    /*  поточний трек — завжди видно: з нього й починаємо показ, якщо він далеко  */
    let start = 0;
    const curPos = shown.indexOf(W.cur - 1);
    if (!f && curPos > limit - 10) start = Math.max(0, curPos - 20);
    shown.slice(start, start + limit).forEach(i => {
      const t = PL[i], on = i === W.cur - 1;
      list.append(h('div', { class: 'st' + (on ? ' on' : ''), style: { minHeight: '48px' } },
        h('span', { class: 'no', style: { display: 'block' } }, i + 1),
        h('div', { class: 'logo', style: { background: 'var(--pan2)', color: on ? 'var(--acc)' : 'var(--dim)' } }, ic(on && W.playing ? 'play' : 'file')),
        h('div', { class: 'tx', onclick: () => send('play=' + (i + 1)), title: 'Грати на радіо' },
          h('div', { class: 'nm' }, t.name), h('div', { class: 'ur' }, dir(t.url) || '/'))));
    });
    if (start > 0) list.prepend(h('div', { class: 'none' }, btn('Показати з початку', 'up', () => { limit = start + limit; draw(); }, 'sm')));
    if (shown.length > start + limit) list.append(h('div', { class: 'none' }, btn(`Показати ще (${shown.length - start - limit})`, 'down', () => { limit += 200; draw(); }, 'sm')));
    if (!shown.length) list.append(h('div', { class: 'none' }, PL.length ? 'Нічого не знайдено.' : 'На картці немає треків (або вона ще читається).'));
  }
  let qt = 0;
  q.addEventListener('input', () => { clearTimeout(qt); qt = setTimeout(() => { limit = 100; draw(); }, 150); });
  live(what => { const s = [W.mode, W.cur, W.playing, W.snuffle, PL.length].join('|'); if (s !== lastSig || what === 'pl') { lastSig = s; draw(); } });
};

VIEWS.stations = async page => {
  if (stTab === null) stTab = W.mode === 1 ? 'sd' : 'radio';
  if (stTab === 'sd' && !(S && S.dev.noSd)) { VIEWS.sdlist(page); return; }
  const tabs = stTabs(page, 'radio');
  if (tabs) page.append(tabs);
  const q = h('input', { type: 'search', placeholder: 'Пошук за назвою чи адресою', 'aria-label': 'Пошук' });
  const tools = h('div', { class: 'tools' }, h('div', { class: 'bar' },
    h('div', { class: 'search' }, ic('search'), q),
    btn('Додати', 'plus', () => editStation(-1), 'acc'),
    btn('Каталог', 'globe', catalog),
    btn('Імпорт', 'upload', importFile),
    btn('Експорт', 'download', exportMenu)));
  const dirty = h('div', { class: 'dirty', hidden: true });
  const sdNote = h('div');
  const list = h('div', { class: 'list' }, h('div', { class: 'none' }, 'Завантажую список…'));
  page.append(h('p', { class: 'lead' }, 'Натисніть на станцію, щоб увімкнути її на радіо. Зміни в списку набудуть сили після «Зберегти».'), tools, sdNote, dirty, list);
  try { await loadEditor(); } catch (e) { list.textContent = ''; list.append(h('div', { class: 'none' }, 'Не вдалося прочитати список: ' + e.message)); return; }

  let dragFrom = -1;
  function draw() {
    const d = edDirty();
    dirty.hidden = !d;
    dirty.textContent = '';
    if (d) dirty.append(h('b', null, 'Список змінено, але ще не збережено на радіо.'), btn('Скасувати зміни', null, async () => { if (await confirmBox('Скасувати зміни?', 'Список повернеться до того, що зараз на радіо.', 'Скасувати зміни')) { ED.list = parseCSVText(ED.orig); draw(); } }, 'ghost sm'), btn('Зберегти', 'check', saveStations, 'acc sm'));
    sdNote.textContent = '';
    if (W.mode === 1) sdNote.append(h('div', { class: 'dirty' }, h('b', null, 'Зараз грає картка пам\'яті. Список станцій можна правити, а зберегти — коли грає радіо.'), btn('Перейти на радіо', 'radio', () => send('newmode=0'), 'sm')));
    const f = q.value.trim().toLowerCase();
    list.textContent = '';
    const playingUrl = S && S.url;
    let shown = 0;
    ED.list.forEach((s, i) => {
      if (f && !(s.name.toLowerCase().includes(f) || s.url.toLowerCase().includes(f))) return;
      shown++;
      const on = W.mode === 0 && playingUrl && s.url === playingUrl && !isSerm();
      const favI = S ? S.fav.findIndex(x => x.u === s.url) : -1;
      const r = h('div', { class: 'st' + (on ? ' on' : ''), 'data-i': i },
        f ? null : h('span', { class: 'hd', title: 'Перетягніть, щоб змінити порядок' }, ic('grip')),
        h('span', { class: 'no' }, i + 1),
        logoEl(s.name, s.url),
        h('div', { class: 'tx', onclick: () => playStation(i), title: 'Увімкнути на радіо' },
          h('div', { class: 'nm' }, s.name, s.ovol ? h('span', { class: 'badge' }, (s.ovol > 0 ? '+' : '') + s.ovol) : null),
          h('div', { class: 'ur' }, s.url)),
        h('div', { class: 'act' },
          ibtn(favI >= 0 ? 'starf' : 'star', favI >= 0 ? `В обраному (${favI + 1})` : 'Додати в обране', () => pickFavSlot(s), 'ghost sm'),
          ibtn('edit', 'Змінити', () => editStation(i), 'ghost sm')));
      list.append(r);
    });
    if (!shown) list.append(h('div', { class: 'none' }, ED.list.length ? 'Нічого не знайдено.' : 'Список порожній. Додайте станцію вручну, з каталогу або з файлу.'));
  }
  /*  перетягування за ручку — і мишею, і пальцем  */
  list.addEventListener('pointerdown', e => {
    const hd = e.target.closest('.hd'); if (!hd) return;
    const r = hd.closest('.st'); dragFrom = +r.dataset.i; r.classList.add('drag');
    hd.setPointerCapture(e.pointerId); e.preventDefault();
    let over = -1;
    const move = ev => {
      const el = document.elementFromPoint(ev.clientX, ev.clientY), t = el && el.closest('.st');
      list.querySelectorAll('.over').forEach(x => x.classList.remove('over'));
      if (t) { over = +t.dataset.i; t.classList.add('over'); }
      if (ev.clientY < 90) window.scrollBy(0, -12); else if (ev.clientY > innerHeight - 90) window.scrollBy(0, 12);
    };
    const up = () => {
      hd.removeEventListener('pointermove', move); hd.removeEventListener('pointerup', up); hd.removeEventListener('pointercancel', up);
      if (over >= 0 && over !== dragFrom) { const [it] = ED.list.splice(dragFrom, 1); ED.list.splice(over, 0, it); }
      dragFrom = -1; draw();
    };
    hd.addEventListener('pointermove', move); hd.addEventListener('pointerup', up); hd.addEventListener('pointercancel', up);
  });
  q.addEventListener('input', draw);
  VIEWS.stations.redraw = draw;
  let lastSig = '';
  live(what => { const s = [W.mode, S && S.url, S && JSON.stringify(S.fav), LOGO.url.size, isSerm()].join('|'); if (what === 'logos' || s !== lastSig) { lastSig = s; draw(); } });
};
function redrawStations() { if (curPage === 'stations' && VIEWS.stations.redraw) VIEWS.stations.redraw(); }

async function playStation(i) {
  const s = ED.list[i];
  if (W.mode === 1) { toast('Спершу перейдіть на радіо', true); return; }
  /*  плата грає за номером у збереженому списку — шукаємо станцію там  */
  const k = PL.findIndex(x => x.url === s.url);
  if (k < 0) { toast('Станції ще немає на радіо — збережіть список', true); return; }
  send('play=' + (k + 1));
}

function editStation(i) {
  const isNew = i < 0;
  const s = isNew ? { name: '', url: '', ovol: 0 } : ED.list[i];
  const nm = h('input', { type: 'text', value: s.name, maxlength: 120, placeholder: 'Наприклад: Радіо Промінь' });
  const ur = h('input', { type: 'url', value: s.url, placeholder: 'http://…', spellcheck: 'false' });
  const ovOut = h('output', null, (s.ovol > 0 ? '+' : '') + s.ovol);
  const ov = range(-30, 30, s.ovol, v => { ovOut.textContent = (v > 0 ? '+' : '') + v; });
  const pos = h('input', { type: 'number', min: 1, max: ED.list.length + (isNew ? 1 : 0), value: isNew ? ED.list.length + 1 : i + 1 });
  const err = h('p', { class: 'note', style: { color: 'var(--bad)' } });
  const audio = h('audio', { preload: 'none' });
  let testing = false;
  const testBtn = btn('Послухати тут', 'headph', () => {
    if (testing) { audio.pause(); audio.removeAttribute('src'); audio.load(); testing = false; testBtn.lastChild.textContent = 'Послухати тут'; return; }
    if (!/^https?:\/\//i.test(ur.value.trim())) { err.textContent = 'Адреса має починатися з http:// або https://'; return; }
    audio.src = ur.value.trim(); audio.play().then(() => { err.textContent = ''; }).catch(() => { err.textContent = 'Браузер не зміг відкрити потік. Радіо може впоратися — перевірте на ньому.'; });
    testing = true; testBtn.lastChild.textContent = 'Зупинити';
  }, 'sm');
  const logoBox = h('div', { class: 'bar' });
  const drawLogo = () => {
    logoBox.textContent = '';
    if (!ur.value.trim()) { logoBox.append(h('span', { class: 'note', style: { margin: 0 } }, 'Логотип можна задати після того, як буде адреса.')); return; }
    const file = h('input', { type: 'file', accept: 'image/*', hidden: true });
    file.onchange = async () => { if (!file.files[0]) return; try { await uploadLogo(ur.value.trim(), file.files[0]); toast('Логотип збережено'); drawLogo(); redrawStations(); } catch (e) { toast('Логотип не збережено: ' + e.message, true); } };
    const crc = crc32(ur.value.trim()), has = LOGO.have.has(crc + '.565') || LOGO.have.has(crc + '.jpg');
    logoBox.append(logoEl(nm.value, ur.value.trim()), file,
      btn('Свій логотип', 'image', () => file.click(), 'sm'),
      has ? btn('Знайти заново', 'refresh', async () => { await setx({ logoDel: crc }); setTimeout(async () => { await loadLogos(); drawLogo(); redrawStations(); }, 600); }, 'sm ghost') : h('span', { class: 'note', style: { margin: 0 } }, 'Радіо саме шукає логотип у каталозі, коли станція заграє.'));
  };
  ur.addEventListener('change', drawLogo);
  drawLogo();
  const body = h('div', null,
    h('label', { class: 'fld' }, h('span', null, 'Назва'), nm),
    h('label', { class: 'fld' }, h('span', null, 'Адреса потоку'), ur),
    h('div', { class: 'bar', style: { marginBottom: '12px' } }, testBtn, audio),
    h('div', { class: 'fld' }, h('span', null, 'Корекція гучності станції'), h('div', { class: 'rng' }, ov, ovOut)),
    h('label', { class: 'fld inl' }, h('span', null, 'Місце у списку'), h('div', { style: { width: '110px' } }, pos)),
    h('div', { class: 'fld' }, h('span', null, 'Логотип'), logoBox),
    err);
  const stopAudio = () => { audio.pause(); audio.removeAttribute('src'); };
  const d = dialog(isNew ? 'Нова станція' : 'Станція', body, [
    isNew ? null : btn('Видалити', 'trash', async () => {
      if (!await confirmBox('Видалити станцію?', `«${s.name}» зникне зі списку після збереження.`, 'Видалити', true)) return;
      ED.list.splice(i, 1); stopAudio(); d.close(); redrawStations();
    }, 'ghost bad'),
    '|',
    btn('Скасувати', null, () => { stopAudio(); d.close(); }, 'ghost'),
    btn(isNew ? 'Додати' : 'Готово', 'check', () => {
      const c = cleanSt({ name: nm.value, url: ur.value, ovol: ov.value });
      if (!c.name) { err.textContent = 'Вкажіть назву.'; nm.focus(); return; }
      if (!/^https?:\/\/[^\s]+$/i.test(c.url)) { err.textContent = 'Адреса має починатися з http:// або https://'; ur.focus(); return; }
      if (bytes(c.url) > 160) { err.textContent = 'Задовга адреса: радіо вміщує до 160 знаків.'; return; }
      const dup = ED.list.findIndex((x, k) => x.url === c.url && k !== i);
      if (dup >= 0) { err.textContent = `Така адреса вже є: «${ED.list[dup].name}» (№${dup + 1}).`; return; }
      let p = Math.max(1, Math.min(ED.list.length + (isNew ? 1 : 0), parseInt(pos.value) || 1)) - 1;
      if (isNew) ED.list.splice(p, 0, c);
      else { ED.list.splice(i, 1); ED.list.splice(p, 0, c); }
      stopAudio(); d.close(); redrawStations();
    }, 'acc'),
  ], { onclose: stopAudio });
}

async function saveStations() {
  if (W.mode === 1) { toast('Зберегти список можна лише в режимі радіо', true); return; }
  if (!ED.list.length && !await confirmBox('Порожній список', 'На радіо не лишиться жодної станції. Зберегти так?', 'Зберегти', true)) return;
  const wasUrl = S && S.url, wasPlaying = W.playing && !isSerm();
  const text = serialize(ED.list.map(cleanSt));
  try {
    await api('/api/stations', { method: 'POST', body: new Blob([text], { type: 'application/octet-stream' }) });
  } catch (e) { toast('Не збережено: ' + e.message, true); return; }
  ED.orig = text; ED.list = parseCSVText(text);
  toast('Список збережено на радіо');
  setTimeout(async () => {
    await loadPlayList();
    /*  станцію, що грала, у новому списку могли пересунути — нехай плата знає її новий номер  */
    if (wasPlaying && wasUrl) { const k = PL.findIndex(x => x.url === wasUrl); if (k >= 0 && k + 1 !== W.cur) send('play=' + (k + 1)); }
    redrawStations();
  }, 900);
}

function exportMenu() {
  const list = edDirty() ? ED.list : parseCSVText(ED.orig);
  const d = dialog('Експорт списку', h('div', null,
    h('p', { class: 'note', style: { marginTop: 0 } }, `${list.length} ${plural(list.length, 'станція', 'станції', 'станцій')}${edDirty() ? ', разом із незбереженими змінами' : ''}.`),
    h('div', { class: 'bar', style: { marginTop: '12px' } },
      btn('CSV для радіо', 'download', () => { download('playlist.csv', serialize(list), 'text/csv'); d.close(); }),
      btn('M3U для програвачів', 'download', () => { download('potuzhne-radio.m3u', '#EXTM3U\n' + list.map(s => `#EXTINF:-1,${s.name}\n${s.url}`).join('\n') + '\n', 'audio/x-mpegurl'); d.close(); }))),
    [btn('Закрити', null, () => d.close(), 'ghost')]);
}

function importFile() {
  const f = h('input', { type: 'file', accept: '.csv,.m3u,.m3u8,.pls,.txt,.json,text/*' });
  f.onchange = async () => {
    const file = f.files[0]; if (!file) return;
    const text = await file.text();
    const found = parseAny(text, file.name).map(cleanSt).filter(s => s.name && s.url);
    if (!found.length) { toast('У файлі не знайшлося жодної станції', true); return; }
    importReview(found, file.name);
  };
  f.click();
}
function importReview(found, from) {
  const have = new Set(ED.list.map(s => s.url));
  const boxes = found.map(s => { const dup = have.has(s.url); return { s, dup, cb: h('input', { type: 'checkbox', checked: !dup }) }; });
  let mode = 'add';
  const cnt = h('span', { class: 'note', style: { margin: 0 } });
  const upd = () => { const n = boxes.filter(b => b.cb.checked).length; cnt.textContent = `Вибрано ${n} з ${boxes.length}`; };
  boxes.forEach(b => b.cb.addEventListener('change', upd));
  const body = h('div', null,
    h('p', { class: 'note', style: { marginTop: 0 } }, `У «${from}» знайдено ${found.length} ${plural(found.length, 'станцію', 'станції', 'станцій')}. Ті, що вже є у списку, не позначено.`),
    seg([['add', 'Додати в кінець'], ['replace', 'Замінити весь список']], 'add', v => { mode = v; }),
    h('div', { class: 'imp' }, boxes.map(b => h('label', null, b.cb, h('div', null, b.s.name, b.dup ? h('span', { class: 'badge' }, 'вже є') : null, h('small', null, b.s.url))))),
    h('div', { class: 'bar' }, btn('Усі', null, () => { boxes.forEach(b => b.cb.checked = true); upd(); }, 'sm ghost'), btn('Жодної', null, () => { boxes.forEach(b => b.cb.checked = false); upd(); }, 'sm ghost'), h('span', { class: 'sp' }), cnt));
  upd();
  const d = dialog('Імпорт станцій', body, [
    btn('Скасувати', null, () => d.close(), 'ghost'),
    btn('Перенести в список', 'check', () => {
      const pick = boxes.filter(b => b.cb.checked).map(b => b.s);
      if (mode === 'replace') ED.list = pick; else ED.list.push(...pick.filter(s => !have.has(s.url)));
      d.close(); redrawStations();
      toast(`Перенесено ${pick.length}. Перегляньте список і натисніть «Зберегти».`);
    }, 'acc')]);
}

/*  Каталог radio-browser.info — той самий, де плата шукає логотипи.  */
function catalog() {
  const q = h('input', { type: 'search', placeholder: 'Назва станції, наприклад «Промінь»' });
  let ua = true;
  const res = h('div', { class: 'imp', style: { maxHeight: '52vh' } }, h('div', { class: 'none' }, 'Введіть назву й натисніть «Шукати».'));
  const audio = h('audio', { preload: 'none' });
  let playingBtn = null;
  const have = () => new Set(ED.list.map(s => s.url));
  async function run() {
    const t = q.value.trim();
    res.textContent = ''; res.append(h('div', { class: 'none' }, 'Шукаю…'));
    const qs = new URLSearchParams({ limit: 60, hidebroken: 'true', order: 'votes', reverse: 'true' });
    if (t) qs.set('name', t);
    if (ua) qs.set('countrycode', 'UA');
    let items = null;
    for (const host of ['de1.api.radio-browser.info', 'fi1.api.radio-browser.info', 'de2.api.radio-browser.info']) {
      try { const r = await fetch(`https://${host}/json/stations/search?${qs}`); if (r.ok) { items = await r.json(); break; } } catch (e) {}
    }
    res.textContent = '';
    if (!items) { res.append(h('div', { class: 'none' }, 'Каталог не відповідає. Перевірте інтернет на цьому комп\'ютері.')); return; }
    if (!items.length) { res.append(h('div', { class: 'none' }, 'Нічого не знайдено.')); return; }
    const hv = have();
    for (const it of items) {
      const url = (it.url_resolved || it.url || '').trim();
      if (!/^https?:\/\//.test(url) || bytes(url) > 160) continue;
      const added = hv.has(url);
      const add = btn(added ? 'Є' : 'Додати', added ? 'check' : 'plus', () => {
        ED.list.push(cleanSt({ name: it.name, url, ovol: 0 })); add.disabled = true; add.lastChild.textContent = 'Додано'; redrawStations();
      }, 'sm' + (added ? ' ghost' : ''));
      if (added) add.disabled = true;
      const pl = ibtn('play', 'Послухати', () => {
        if (playingBtn === pl) { audio.pause(); pl.replaceChildren(ic('play')); playingBtn = null; return; }
        if (playingBtn) playingBtn.replaceChildren(ic('play'));
        audio.src = url; audio.play().catch(() => toast('Браузер не відкрив цей потік', true));
        pl.replaceChildren(ic('pause')); playingBtn = pl;
      }, 'sm ghost');
      const fav = h('div', { class: 'logo', style: { background: '#fff', width: '36px', height: '36px' } });
      if (it.favicon && /^https?:/.test(it.favicon)) { const im = h('img', { src: it.favicon, alt: '', loading: 'lazy', referrerpolicy: 'no-referrer' }); im.onerror = () => { fav.style.background = PAL[0]; fav.textContent = initials(it.name); }; fav.append(im); }
      else { fav.style.background = PAL[0]; fav.textContent = initials(it.name); }
      res.append(h('div', { class: 'st', style: { minHeight: '52px' } }, fav,
        h('div', { class: 'tx' }, h('div', { class: 'nm' }, it.name.trim()), h('div', { class: 'ur' }, [it.codec, it.bitrate ? it.bitrate + ' кбіт/с' : '', it.country || '', (it.tags || '').split(',').slice(0, 3).join(', ')].filter(Boolean).join(' · '))),
        h('div', { class: 'act' }, pl, add)));
    }
  }
  q.addEventListener('keydown', e => { if (e.key === 'Enter') run(); });
  const body = h('div', null,
    h('div', { class: 'bar' }, h('div', { class: 'search' }, ic('search'), q), btn('Шукати', 'search', run, 'acc')),
    h('div', { class: 'bar', style: { marginTop: '10px' } }, sw(ua, v => { ua = v; }), h('span', null, 'Лише українські станції')),
    res, audio,
    h('p', { class: 'note' }, 'Додані станції з\'являться в списку; щоб вони потрапили на радіо, натисніть «Зберегти».'));
  const d = dialog('Каталог станцій', body, [btn('Готово', null, () => d.close(), 'acc')], { onclose: () => audio.pause() });
  run();
}

function pickFavSlot(s) {
  if (!S) return;
  const grid = h('div', { class: 'favs' });
  const d = dialog('В обране', h('div', null, h('p', { class: 'note', style: { marginTop: 0 } }, `Куди поставити «${s.name}»?`), grid), [btn('Скасувати', null, () => d.close(), 'ghost')]);
  S.fav.forEach((f, i) => grid.append(h('button', { type: 'button', class: 'fav' + (f.n ? '' : ' empty'), style: { cursor: 'pointer' }, onclick: async () => {
    await setx({ favPut: `${i}\t${s.name}\t${s.url}` }); d.close(); toast(`«${s.name}» — у кнопці ${i + 1}`); pollState();
  } }, h('span', { class: 'n' }, f.n || 'порожньо'), h('span', { class: 'k' }, String(i + 1)))));
}

/* ---------------------------------------------------------------- обране */
VIEWS.fav = page => {
  const grid = h('div', { class: 'list' });
  const mainSw = sw(true, v => setx({ favHide: v ? 0 : 1 }));
  page.append(h('p', { class: 'lead' }, 'Шість кнопок швидкого вибору — ті самі, що в меню радіо «обране». Станцію можна поставити з цього розділу, зі списку станцій (зірочка) або на самому радіо.'),
    card(null, row('На головному екрані радіо', 'рядок із шести логотипів під годинником: дотик — і станція грає', mainSw)),
    grid);
  live(() => {
    if (!S) return;
    mainSw.firstChild.checked = !S.favHide;
    const sig = JSON.stringify(S.fav) + S.favOn + W.name;
    if (grid.dataset.sig === sig) return;
    grid.dataset.sig = sig; grid.textContent = '';
    S.fav.forEach((f, i) => {
      const sel = h('select', { 'aria-label': 'Станція зі списку' }, h('option', { value: '' }, 'Вибрати зі списку…'), PL.map((s, k) => h('option', { value: k }, `${k + 1}. ${s.name}`)));
      sel.onchange = async () => { const s = PL[+sel.value]; if (!s) return; await setx({ favPut: `${i}\t${s.name}\t${s.url}` }); pollState(); };
      grid.append(h('div', { class: 'st' + (S.favOn === i ? ' on' : '') },
        h('span', { class: 'no', style: { display: 'block', fontSize: '15px', color: 'var(--acc)' } }, i + 1),
        f.n ? logoEl(f.n, f.u) : h('div', { class: 'logo', style: { border: '1px dashed #444' } }),
        h('div', { class: 'tx', onclick: () => f.n && setx({ favPlay: i }) }, h('div', { class: 'nm' }, f.n || 'порожньо'), h('div', { class: 'ur' }, f.u || 'кнопка вільна')),
        h('div', { class: 'act', style: { alignItems: 'center', gap: '6px', flexWrap: 'wrap', justifyContent: 'flex-end' } },
          f.n ? ibtn('play', 'Увімкнути', () => setx({ favPlay: i }), 'ghost sm') : null,
          W.mode === 0 && W.name && !isSerm() ? btn('Поточна', null, async () => { await setx({ favSet: i }); pollState(); }, 'sm ghost') : null,
          h('div', { style: { width: '170px' } }, sel),
          f.n ? ibtn('x', 'Звільнити кнопку', async () => { await setx({ favClear: i }); pollState(); }, 'ghost sm') : null)));
    });
  });
};

/* ---------------------------------------------------------------- проповіді */
VIEWS.sermons = page => {
  const list = h('div', { class: 'list' }, h('div', { class: 'none' }, 'Завантажую з сайту…'));
  const q = h('input', { type: 'search', placeholder: 'Пошук за назвою, місцем Писання', 'aria-label': 'Пошук' });
  const who = h('select', { 'aria-label': 'Проповідник' }, h('option', { value: '' }, 'Усі проповідники'));
  const info = h('span', { class: 'note', style: { margin: 0 } });
  page.append(
    h('p', { class: 'lead' }, 'Увесь архів проповідей із сайту церкви, від найновіших. Натисніть, щоб слухати на радіо.'),
    h('div', { class: 'tools' }, h('div', { class: 'bar' }, h('div', { class: 'search' }, ic('search'), q), h('div', { style: { width: '240px', maxWidth: '100%' } }, who),
      btn('Оновити', 'refresh', () => { setx({ sermLoad: 1 }); data = null; list.textContent = ''; list.append(h('div', { class: 'none' }, 'Оновлюю…')); setTimeout(load, 2000); }, 'sm')),
      h('div', { style: { marginTop: '8px' } }, info)),
    list);
  let data = null, t = 0, limit = 60;
  async function load() {
    clearTimeout(t);
    try { data = await api('/api/sermons'); } catch (e) { list.textContent = ''; list.append(h('div', { class: 'none' }, 'Радіо не відповіло: ' + e.message)); t = setTimeout(load, 4000); return; }
    if (S) S.sermSite = data.site;
    if (data.load && !data.items.length) {
      list.textContent = ''; list.append(h('div', { class: 'none' }, data.err ? 'Сайт не відповідає: ' + data.err : `Радіо завантажує архів із сайту…${data.got ? ' ' + data.got : ''}`));
      t = setTimeout(load, 1500); data = null; return;
    }
    /*  проповідники — від тих, у кого найбільше проповідей  */
    const cnt = {};
    data.items.forEach(it => { const p = it.p || ''; cnt[p] = (cnt[p] || 0) + 1; });
    const keep = who.value;
    who.textContent = '';
    who.append(h('option', { value: '' }, `Усі проповідники (${data.items.length})`),
      Object.keys(cnt).sort((x, y) => cnt[y] - cnt[x]).map(p => h('option', { value: p || '—' }, `${p || 'без імені'} (${cnt[p]})`)));
    who.value = keep;
    draw();
  }
  function draw() {
    if (!data) return;
    const cur = isSerm() ? S.serm.i : -1;
    const f = q.value.trim().toLowerCase(), w = who.value;
    const shown = [];
    data.items.forEach((it, i) => {
      if (w && (it.p || '—') !== w) return;
      if (f && !(it.t.toLowerCase().includes(f) || (it.p || '').toLowerCase().includes(f) || (it.d || '').includes(f))) return;
      shown.push(i);
    });
    info.textContent = shown.length === data.items.length ? `${data.items.length} ${plural(data.items.length, 'проповідь', 'проповіді', 'проповідей')}` : `Знайдено ${shown.length} з ${data.items.length}`;
    list.textContent = '';
    shown.slice(0, limit).forEach(i => {
      const it = data.items[i];
      const cv = h('div', { class: 'cv' });
      if (it.c) cv.append(h('img', { src: (/^https?:/.test(it.c) ? '' : data.site) + it.c, alt: '', loading: 'lazy' }));
      const date = it.d ? it.d.split('-').reverse().join('.') : '';
      list.append(h('div', { class: 'srm' + (i === cur ? ' on' : ''), style: { cursor: 'pointer' }, onclick: () => { setx({ serm: i }); toast('Вмикаю проповідь…'); } },
        cv,
        h('div', null, h('div', { class: 't' }, it.t), h('div', { class: 'p' }, [it.p, date, it.dur ? mmss(it.dur) : ''].filter(Boolean).join(' · '))),
        h('div', { class: 'go' }, i === cur ? h('span', { class: 'pill acc' }, W.playing ? 'грає' : 'пауза') : ibtn('play', 'Слухати на радіо', null, 'ghost'))));
    });
    if (shown.length > limit) list.append(h('div', { class: 'none' }, btn(`Показати ще (${shown.length - limit})`, 'down', () => { limit += 100; draw(); })));
    if (!shown.length) list.append(h('div', { class: 'none' }, data.items.length ? 'Нічого не знайдено.' : (data.err ? 'Сайт не відповідає: ' + data.err : 'Проповідей немає.')));
  }
  let qt = 0;
  q.addEventListener('input', () => { clearTimeout(qt); qt = setTimeout(() => { limit = 60; draw(); }, 150); });
  who.addEventListener('change', () => { limit = 60; draw(); });
  load();
  let last = '';
  live(() => { const s = (isSerm() ? S.serm.i : -1) + '|' + W.playing; if (s !== last) { last = s; draw(); } });
};

/* ---------------------------------------------------------------- запис і картка */
VIEWS.records = page => {
  const recBox = h('div');
  const list = h('div', { class: 'list' }, h('div', { class: 'none' }, 'Читаю картку…'));
  const info = h('p', { class: 'note' });
  const audio = h('audio', { controls: true, preload: 'none', style: { width: '100%', marginTop: '10px' }, hidden: true });
  page.append(card('Запис ефіру', recBox),
    h('section', { class: 'card', style: { padding: 0, overflow: 'hidden' } },
      h('h2', { style: { padding: '16px 16px 0' } }, 'Записи на картці', h('span', { class: 'sp' }), btn('Оновити', 'refresh', load, 'sm ghost')),
      h('div', { style: { padding: '0 16px' } }, info, audio), list),
    card('Картка як джерело', h('div', { class: 'bar' }, h('span', { style: { flex: 1 } }, 'Слухати музику з картки пам\'яті на радіо. Записи з\'являються там само.'), btn('Грати з картки', 'sd', () => { send('newmode=1'); location.hash = '#/player'; }))));
  async function load() {
    let j;
    try { j = await api('/api/records'); } catch (e) { list.textContent = ''; list.append(h('div', { class: 'none' }, e.message)); return; }
    list.textContent = '';
    if (j.err) { info.textContent = ''; list.append(h('div', { class: 'none' }, j.err)); return; }
    info.textContent = `Вільно ${size(j.free)} з ${size(j.total)}.`;
    const items = (j.items || []).sort((a, b) => b.f.localeCompare(a.f));
    if (!items.length) { list.append(h('div', { class: 'none' }, 'Записів ще немає.')); return; }
    for (const it of items) {
      const m = it.f.match(/^(\d{4})(\d\d)(\d\d)-(\d\d)(\d\d)(\d\d)_(.*)\.(\w+)$/);
      const title = m ? m[7].replace(/_/g, ' ') : it.f;
      const when = m ? `${m[3]}.${m[2]}.${m[1]} ${m[4]}:${m[5]}` : '';
      const busy = S && S.rec.on && S.rec.file === it.f;
      list.append(h('div', { class: 'st' },
        h('div', { class: 'logo', style: { background: 'var(--pan2)', color: 'var(--dim)' } }, ic('file')),
        h('div', { class: 'tx' }, h('div', { class: 'nm' }, title, busy ? h('span', { class: 'badge' }, 'пишеться') : null), h('div', { class: 'ur' }, [when, size(it.s), m ? m[8].toUpperCase() : ''].filter(Boolean).join(' · '))),
        h('div', { class: 'act' },
          ibtn('play', 'Послухати в браузері', () => { audio.hidden = false; audio.src = '/api/rec?f=' + encodeURIComponent(it.f); audio.play().catch(() => toast('Браузер не відтворює цей формат — завантажте файл', true)); }, 'ghost sm'),
          h('a', { class: 'btn ico sm ghost', href: '/api/rec?dl=1&f=' + encodeURIComponent(it.f), download: it.f, title: 'Завантажити', 'aria-label': 'Завантажити' }, ic('download')),
          busy ? null : ibtn('trash', 'Видалити', async () => {
            if (!await confirmBox('Видалити запис?', `${title} ${when} (${size(it.s)}) буде стерто з картки назавжди.`, 'Видалити', true)) return;
            audio.pause(); await setx({ recDel: it.f }); setTimeout(load, 600);
          }, 'ghost sm'))));
    }
  }
  load();
  let lastOn = null;
  live(() => {
    if (!S) return;
    const serm = isSerm();
    recBox.textContent = '';
    if (S.rec.on) {
      recBox.append(h('div', { class: 'bar' }, h('span', { style: { fontSize: '18px' } }, h('i', { class: 'recdot' }), mmss(S.rec.sec)), h('span', { class: 'pill' }, size(S.rec.bytes)), h('span', { class: 'sp' }), btn('Зупинити запис', 'stop', () => setx({ rec: 0 }), 'acc')),
        h('p', { class: 'note' }, 'Файл: ' + S.rec.file + '. Запис зупиниться сам, якщо перемкнути станцію.'));
    } else {
      recBox.append(h('div', { class: 'bar' }, h('span', { style: { flex: 1 } }, W.playing && W.mode === 0 && !serm ? 'Записати те, що грає: ' + W.name : 'Увімкніть радіостанцію, щоб записати ефір.'), btn('Почати запис', 'rec', () => setx({ rec: 1 }), 'acc')),
        h('p', { class: 'note' }, 'Потік пишеться на картку як є — MP3 лишається MP3, AAC — AAC. Проповіді й музику з картки не записуємо.'));
    }
    if (lastOn !== null && lastOn !== S.rec.on) setTimeout(load, 800);
    lastOn = S.rec.on;
  });
};

/* ---------------------------------------------------------------- будильник і сон */
const HALF = Array.from({ length: 48 }, (_, i) => [i, `${pad2(i >> 1)}:${i & 1 ? '30' : '00'}`]);
function selectEl(items, cur, onchange) {
  const s = h('select', null, items.map(([v, l]) => h('option', { value: v, selected: String(v) === String(cur) }, l)));
  s.onchange = () => onchange(s.value);
  return s;
}
VIEWS.alarm = page => {
  const sl = h('div'), al = h('div');
  page.append(card('Таймер сну', sl), card('Будильник', al));
  const sleepSeg = seg([[0, 'Вимк'], [15, '15 хв'], [30, '30 хв'], [60, '1 год'], [90, '1,5 год']], -1, v => setx({ sleep: v }));
  const own = h('input', { type: 'number', min: 1, max: 600, placeholder: 'хв', style: { width: '90px' } });
  const sleepNote = h('p', { class: 'note' });
  sl.append(sleepSeg, h('div', { class: 'bar', style: { marginTop: '12px' } }, h('span', null, 'Або своє:'), own, btn('Поставити', null, () => { const v = parseInt(own.value); if (v > 0) setx({ sleep: Math.min(600, v) }); }, 'sm')), sleepNote);
  const time = h('input', { type: 'time', style: { width: '130px' } });
  const onSw = sw(false, v => setx({ alarmOn: v ? 1 : 0 }));
  const days = seg([[0, 'Щодня'], [1, 'Будні']], 0, v => setx({ alarmDays: v }));
  const aNote = h('p', { class: 'note' });
  time.addEventListener('change', () => { const [hh, mm] = time.value.split(':').map(Number); if (!isNaN(hh)) setx({ alarmH: hh, alarmM: mm, alarmOn: 1 }); });
  al.append(row('Будильник', 'грає остання радіостанція, гучність наростає 30 секунд', onSw),
    row('Час', null, time), row('Дні', null, days), aNote);
  live(() => {
    if (!S) return;
    sleepSeg.set(S.sleep.set);
    sleepNote.textContent = S.sleep.set ? `Радіо вимкнеться через ${mmss(S.sleep.left)}; останні 20 секунд звук плавно стихає, екран гасне.` : 'Таймер не стоїть.';
    const a = S.alarm;
    onSw.firstChild.checked = !!a.on;
    if (document.activeElement !== time) time.value = `${pad2(a.h)}:${pad2(a.m)}`;
    days.set(a.days);
    aNote.textContent = !a.on ? 'Будильник вимкнено.' : a.ring ? 'Будильник дзвонить зараз.' : a.in < 0 ? 'Радіо ще не знає точного часу.' : `Задзвонить через ${Math.floor(a.in / 60)} год ${pad2(a.in % 60)} хв${W.mode === 0 && W.name ? '. Заграє «' + W.name + '»' : ''}.`;
  });
};

/* ---------------------------------------------------------------- екран */
VIEWS.screen = page => {
  let brT = 0, nlT = 0;
  const brOut = h('output');
  const br = range(0, 100, 50, v => { brOut.textContent = v + '%'; clearTimeout(brT); brT = setTimeout(() => setx({ bright: v }), 120); });
  const nOn = sw(false, v => setx({ nightOn: v ? 1 : 0 }));
  const nFrom = selectEl(HALF, 44, v => setx({ nightFrom: v }));
  const nTo = selectEl(HALF, 14, v => setx({ nightTo: v }));
  const nlOut = h('output');
  const nl = range(0, 100, 10, v => { nlOut.textContent = v ? v + '%' : 'гасне'; clearTimeout(nlT); nlT = setTimeout(() => setx({ nightLevel: v }), 150); });
  const save = seg([[0, 'Вимк'], [1, '10 с'], [2, '15 с'], [3, '30 с'], [4, '60 с']], 0, v => setx({ batSave: v }));
  const led = seg([[0, 'Вимк'], [1, 'Стан'], [2, 'Музика']], 1, v => setx({ ledMode: v }));
  const nNote = h('p', { class: 'note' });
  page.append(
    card('Яскравість', h('div', { class: 'rng' }, br, brOut), h('p', { class: 'note' }, 'Денна яскравість екрана. Змінюється одразу.')),
    card('Нічний режим', row('Приглушувати вночі', 'за розкладом; дотик ненадовго повертає денну яскравість', nOn),
      row('Від', null, h('div', { style: { width: '110px' } }, nFrom)), row('До', null, h('div', { style: { width: '110px' } }, nTo)),
      h('div', { class: 'fld', style: { marginTop: '8px' } }, h('span', null, 'Нічна яскравість (0 — екран гасне)'), h('div', { class: 'rng' }, nl, nlOut)), nNote),
    card('Економія батареї', save, h('p', { class: 'note' }, 'Коли радіо працює від акумулятора, екран після стількох секунд без дотиків плавно пригасає. Перший дотик лише будить його.')),
    card('Світлодіод', led, h('p', { class: 'note' }, '«Стан» — колір показує, що робить радіо; «Музика» — блимає в такт. Під час запису — червоний.')),
    card('Додатково', h('div', { id: 'scrx' })));
  send('getscreen=1');
  const x = $('#scrx');
  let xs = '';
  live(what => {
    if (S) {
      if (document.activeElement !== br) { br.value = S.bright; fillRange(br); brOut.textContent = S.bright + '%'; }
      nOn.firstChild.checked = !!S.night.on;
      if (document.activeElement !== nFrom) nFrom.value = S.night.from;
      if (document.activeElement !== nTo) nTo.value = S.night.to;
      if (document.activeElement !== nl) { nl.value = S.night.level; fillRange(nl); nlOut.textContent = S.night.level ? S.night.level + '%' : 'гасне'; }
      nNote.textContent = S.night.on ? (S.night.act ? 'Зараз діє нічний режим.' : 'Зараз денний режим.') : '';
      save.set(S.save); led.set(S.led);
    }
    if ('flip' in C) {
      const s = ['flip', 'inv', 'tsf', 'scre', 'scrt', 'scrb', 'scrpe', 'scrpt', 'scrpb'].map(k => C[k]).join('|');
      if (s === xs) return; xs = s;
      x.textContent = '';
      const num = (k, cmd, min, max) => { const i = h('input', { type: 'number', min, max, value: C[k], style: { width: '90px' } }); i.onchange = () => send(`${cmd}=${i.value}`); return i; };
      x.append(
        row('Перевернути екран', null, sw(+C.flip, v => send('flipscreen=' + (v ? 1 : 0)))),
        row('Інверсія кольорів', null, sw(+C.inv, v => send('invertdisplay=' + (v ? 1 : 0)))),
        row('Перевернути сенсор', 'якщо дотики потрапляють не туди', sw(+C.tsf, v => send('fliptouch=' + (v ? 1 : 0)))),
        row('Заставка, коли не грає', 'годинник на весь екран', sw(+C.scre, v => send('screensaverenabled=' + (v ? 1 : 0)))),
        row('— через, секунд', null, num('scrt', 'screensavertimeout', 5, 65520)),
        row('— порожній екран замість годинника', null, sw(+C.scrb, v => send('screensaverblank=' + (v ? 1 : 0)))),
        row('Заставка під час відтворення', null, sw(+C.scrpe, v => send('screensaverplayingenabled=' + (v ? 1 : 0)))),
        row('— через, хвилин', null, num('scrpt', 'screensaverplayingtimeout', 1, 1080)),
        row('— порожній екран', null, sw(+C.scrpb, v => send('screensaverplayingblank=' + (v ? 1 : 0)))));
    }
  });
};

/* ---------------------------------------------------------------- звук */
VIEWS.sound = page => {
  const eq = h('div');
  const bands = [['bass', 'Низькі'], ['middle', 'Середні'], ['trebble', 'Високі'], ['balance', 'Баланс']];
  const els = {};
  for (const [k, l] of bands) {
    const o = h('output');
    let t = 0;
    const r = range(-16, 16, W[k], v => { o.textContent = (v > 0 ? '+' : '') + v; clearTimeout(t); t = setTimeout(() => send(`${k}=${v}`), 80); });
    els[k] = { r, o };
    eq.append(h('div', { class: 'fld' }, h('span', null, l), h('div', { class: 'rng' }, r, o)));
  }
  const ctl = h('div');
  page.append(card('Еквалайзер', eq, h('div', { class: 'bar' }, btn('Скинути', 'refresh', () => { for (const [k] of bands) send(`${k}=0`); }, 'sm'))),
    card('Гучність', ctl),
    card('Аудіовихід', h('div', { class: 'bar' }, h('span', { id: 'dacnow', style: { flex: 1 } }), h('a', { class: 'btn sm', href: '#/dev' }, 'Змінити')), h('p', { class: 'note' }, 'Зовнішній ЦАП чи підсилювач під\'єднують у розділі «Розробник» — там і схеми.')));
  send('getcontrols=1'); send('getsystem=1'); send('getindex=1');
  let cs = '';
  live(() => {
    for (const [k] of bands) { const e = els[k]; if (document.activeElement !== e.r) { e.r.value = W[k]; fillRange(e.r); e.o.textContent = (W[k] > 0 ? '+' : '') + W[k]; } }
    if (S) $('#dacnow').textContent = 'Зараз: ' + DACS[S.dev.dac].n + ' — ' + DACS[S.dev.dac].k;
    const s = [C.vols, C.sst].join('|');
    if ('vols' in C && s !== cs) {
      cs = s; ctl.textContent = '';
      const v = h('input', { type: 'number', min: 1, max: 10, value: C.vols, style: { width: '90px' } }); v.onchange = () => send('volsteps=' + v.value);
      ctl.append(row('Крок гучності', 'на скільки змінюється гучність одним натисканням', v));
      if ('sst' in C) ctl.append(row('Грати після ввімкнення', 'якщо радіо грало, коли його вимкнули', sw(+C.sst, x => send('smartstart=' + (x ? 1 : 0)))));
    }
  });
};

/* ---------------------------------------------------------------- Wi-Fi */
VIEWS.wifi = page => {
  const cur = h('div'), saved = h('div'), scan = h('div');
  page.append(
    AP ? h('div', { class: 'dirty' }, h('b', null, 'Радіо не підключене до мережі й працює як точка доступу. Виберіть свою мережу Wi-Fi нижче.')) : null,
    card('Зараз', cur),
    card('Збережені мережі', saved, h('p', { class: 'note' }, 'До п\'яти мереж. Після ввімкнення радіо пробує їх по черзі, починаючи з першої.')),
    h('section', { class: 'card' }, h('h2', null, 'Доступні мережі', h('span', { class: 'sp' }), btn('Шукати', 'refresh', doScan, 'sm')), scan,
      h('div', { class: 'bar', style: { marginTop: '12px' } }, btn('Прихована мережа…', 'plus', () => join('', true), 'sm ghost'))));
  let data = null, poll = 0;
  async function load() {
    clearTimeout(poll);
    try { data = await api('/api/wifi'); } catch (e) { return; }
    draw();
    if (data.busy) poll = setTimeout(load, 1000);
  }
  function doScan() { setx({ wifiScan: 1 }); scan.textContent = ''; scan.append(h('div', { class: 'none' }, 'Шукаю мережі…')); setTimeout(load, 1200); }
  function draw() {
    if (!data) return;
    cur.textContent = '';
    cur.append(h('dl', { class: 'kv' }, h('dt', null, 'Мережа'), h('dd', null, data.cur || (AP ? 'точка доступу' : 'не підключено')),
      S && S.ip ? [h('dt', null, 'Адреса'), h('dd', null, S.ip)] : null, S && S.rssi ? [h('dt', null, 'Сигнал'), h('dd', null, S.rssi + ' дБм')] : null));
    if (data.fail) cur.append(h('p', { class: 'note', style: { color: 'var(--warn)' } }, `Минулого разу не вдалося підключитися до «${data.fail}».`));
    saved.textContent = '';
    if (!data.saved.length) saved.append(h('div', { class: 'none' }, 'Немає збережених мереж.'));
    data.saved.forEach((s, i) => saved.append(h('div', { class: 'row' },
      h('span', { class: 'pill' + (i === 0 ? ' acc' : '') }, i + 1),
      h('div', { class: 'lbl' }, h('b', null, s), s === data.cur ? h('small', null, 'підключено') : null),
      i > 0 ? btn('Першою', 'up', async () => { await setx({ wifiFirst: s }); load(); }, 'sm ghost') : null,
      ibtn('trash', 'Забути', async () => { if (await confirmBox('Забути мережу?', data.saved.length > 1 ? `«${s}» і її пароль буде видалено з радіо.` : `«${s}» і її пароль буде видалено з радіо. Це остання збережена мережа: після перезавантаження радіо лишиться без мережі, і вибирати її доведеться на самому радіо.`, 'Забути', true)) { await setx({ wifiForget: s }); load(); } }, 'ghost sm'))));
    scan.textContent = '';
    if (data.busy) scan.append(h('div', { class: 'none' }, 'Шукаю мережі…'));
    else if (!data.scan.length) scan.append(h('div', { class: 'none' }, 'Натисніть «Шукати».'));
    data.scan.forEach(n => scan.append(h('div', { class: 'row', style: { cursor: 'pointer' }, onclick: () => join(n.s, n.e) },
      h('span', { html: sigSvg(n.r), class: 'sig' }),
      h('div', { class: 'lbl' }, h('b', null, n.s), h('small', null, [n.e ? 'з паролем' : 'відкрита', n.s === data.cur ? 'підключено' : data.saved.includes(n.s) ? 'збережена' : ''].filter(Boolean).join(' · '))),
      n.e ? ic('lock', 'rowic') : null)));
  }
  function join(ssid, enc) {
    const ss = h('input', { type: 'text', value: ssid, maxlength: 32, placeholder: 'Назва мережі' });
    const pw = h('input', { type: 'password', maxlength: 64, placeholder: enc ? 'Пароль' : 'без пароля', autocomplete: 'new-password' });
    const show = h('label', { class: 'bar', style: { marginTop: '8px', color: 'var(--dim)', fontSize: '13px', cursor: 'pointer' } }, h('input', { type: 'checkbox', onchange: e => { pw.type = e.target.checked ? 'text' : 'password'; } }), 'показати пароль');
    const err = h('p', { class: 'note', style: { color: 'var(--bad)' } });
    const d = dialog('Підключення до мережі', h('div', null,
      ssid ? null : h('label', { class: 'fld' }, h('span', null, 'Назва мережі'), ss),
      ssid ? h('p', { style: { marginTop: 0 } }, h('b', null, ssid)) : null,
      enc ? h('label', { class: 'fld' }, h('span', null, 'Пароль'), pw, show) : h('p', { class: 'note' }, 'Відкрита мережа — пароль не потрібен.'),
      h('p', { class: 'note' }, 'Радіо збереже мережу першою в списку й перезавантажиться. Якщо підключитися не вийде, воно спробує інші збережені мережі й скаже про це на екрані.'), err), [
      btn('Скасувати', null, () => d.close(), 'ghost'),
      btn('Підключитися', 'wifi', async () => {
        const s = (ssid || ss.value).trim(), p = enc ? pw.value : '';
        if (!s) { err.textContent = 'Вкажіть назву мережі.'; return; }
        if (enc && p.length && p.length < 8) { err.textContent = 'Пароль Wi-Fi має бути не коротшим за 8 знаків.'; return; }
        const b = new URLSearchParams({ ssid: s, pass: p });
        try { await api('/api/wifi/join', { method: 'POST', body: b }); } catch (e) { err.textContent = e.message; return; }
        d.close();
        page.textContent = '';
        page.append(card('Підключаюся', h('p', null, `Радіо перезавантажується й підключається до «${s}».`), h('p', { class: 'note' }, AP ? 'Під\'єднайте цей комп\'ютер чи телефон до тієї самої мережі й відкрийте адресу, яку радіо покаже на екрані.' : 'Якщо адреса не зміниться, сторінка оновиться сама за хвилину.')));
        if (!AP) setTimeout(() => location.reload(), 45000);
      }, 'acc')]);
  }
  load();
  if (!data || !data.scan.length) doScan();
  live(() => {});
};

/* ---------------------------------------------------------------- час і погода */
VIEWS.time = page => {
  const tz = h('div'), we = h('div'), now = h('p', { class: 'note' });
  page.append(card('Час', now, tz), card('Погода', we));
  send('gettimezone=1'); send('getweather=1');
  let ts = '', wsig = '';
  live(() => {
    if (S) now.textContent = S.time ? `На радіо зараз ${S.time}, ${S.date}.` : 'Радіо ще не отримало точний час.';
    if ('tzh' in C && ts !== [C.tzh, C.tzm, C.sntp1, C.sntp2, C.timeint].join('|')) {
      ts = [C.tzh, C.tzm, C.sntp1, C.sntp2, C.timeint].join('|');
      tz.textContent = '';
      const hs = selectEl(Array.from({ length: 27 }, (_, i) => [i - 12, (i - 12 >= 0 ? '+' : '') + (i - 12)]), C.tzh, () => {});
      const ms = selectEl([[0, ':00'], [15, ':15'], [30, ':30'], [45, ':45']], C.tzm, () => {});
      const s1 = h('input', { type: 'text', value: C.sntp1, maxlength: 34 }), s2 = h('input', { type: 'text', value: C.sntp2, maxlength: 34 });
      const iv = h('input', { type: 'number', min: 15, max: 1440, value: C.timeint, style: { width: '100px' } });
      tz.append(
        h('div', { class: 'fld' }, h('span', null, 'Часовий пояс (UTC)'), h('div', { class: 'bar' }, h('div', { style: { width: '100px' } }, hs), h('div', { style: { width: '100px' } }, ms), h('span', { class: 'note', style: { margin: 0 } }, 'Україна: +2 взимку, +3 влітку'))),
        h('div', { class: 'grid2' }, h('label', { class: 'fld' }, h('span', null, 'Сервер часу'), s1), h('label', { class: 'fld' }, h('span', null, 'Запасний сервер'), s2)),
        h('label', { class: 'fld inl' }, h('span', null, 'Звіряти час кожні, хв'), iv),
        btn('Застосувати', 'check', () => { send('tzh=' + hs.value); send('tzm=' + ms.value); send('sntp2=' + s2.value.trim()); send('sntp1=' + s1.value.trim()); send('timeint=' + iv.value); toast('Збережено'); }, 'acc'));
    }
    if ('wen' in C && wsig !== [C.wen, C.wlat, C.wlon, C.wkey, C.wint].join('|')) {
      wsig = [C.wen, C.wlat, C.wlon, C.wkey, C.wint].join('|');
      we.textContent = '';
      const lat = h('input', { type: 'text', value: C.wlat, inputmode: 'decimal' }), lon = h('input', { type: 'text', value: C.wlon, inputmode: 'decimal' });
      const key = h('input', { type: 'text', value: C.wkey, spellcheck: 'false', placeholder: 'ключ OpenWeatherMap' });
      const iv = h('input', { type: 'number', min: 15, max: 1440, value: C.wint, style: { width: '100px' } });
      we.append(row('Показувати погоду', 'на екрані плеєра', sw(+C.wen, v => send('showweather=' + (v ? 1 : 0)))),
        h('div', { class: 'grid2', style: { marginTop: '10px' } }, h('label', { class: 'fld' }, h('span', null, 'Широта'), lat), h('label', { class: 'fld' }, h('span', null, 'Довгота'), lon)),
        h('label', { class: 'fld' }, h('span', null, 'Ключ API'), key),
        h('label', { class: 'fld inl' }, h('span', null, 'Оновлювати кожні, хв'), iv),
        h('div', { class: 'bar' }, btn('Застосувати', 'check', () => { send('lat=' + lat.value.trim()); send('lon=' + lon.value.trim()); send('wint=' + iv.value); if (key.value.trim()) send('key=' + key.value.trim()); toast('Збережено'); }, 'acc'),
          btn('Моє місце', 'globe', () => { if (!navigator.geolocation) return toast('Браузер не знає місця', true); navigator.geolocation.getCurrentPosition(p => { lat.value = p.coords.latitude.toFixed(4); lon.value = p.coords.longitude.toFixed(4); }, () => toast('Місце не визначилось (потрібен дозвіл браузера)', true)); }, 'ghost')),
        h('p', { class: 'note' }, 'Безкоштовний ключ дає openweathermap.org після реєстрації.'));
    }
  });
};

/* ---------------------------------------------------------------- система */
VIEWS.system = page => {
  const box = h('div');
  page.append(card('Поведінка', box),
    card('Живлення',
      row('Перезавантажити', 'звук перерветься приблизно на 10 секунд; налаштування й списки лишаються',
        btn('Перезавантажити', 'refresh', async () => { if (await confirmBox('Перезавантажити радіо?', 'Звук перерветься приблизно на 10 секунд.', 'Перезавантажити')) { setx({ power: 'reboot' }); toast('Перезавантажується…'); setTimeout(() => location.reload(), 12000); } }, 'sm')),
      row('Вимкнути', 'глибокий сон: радіо майже не бере заряд; увімкнути — дотиком до екрана радіо',
        btn('Вимкнути', 'x', async () => { if (await confirmBox('Вимкнути радіо?', 'Радіо засне й зникне з мережі. Увімкнути його звідси вже не вийде — лише дотиком до екрана.', 'Вимкнути', true)) { await setx({ power: 'off' }); toast('Радіо вимикається. Щоб увімкнути — торкніться його екрана.'); } }, 'sm bad'))));
  send('getsystem=1'); send('getcontrols=1');
  let s = '';
  live(() => {
    if (!('sst' in C)) return;
    const sig = ['sst', 'aif', 'vu', 'softr', 'mdns', 'telnet', 'watchdog'].map(k => C[k]).join('|');
    if (sig === s) return; s = sig;
    box.textContent = '';
    const sa = h('input', { type: 'number', min: 0, max: 30, value: C.softr, style: { width: '90px' } }); sa.onchange = () => send('softap=' + sa.value);
    const md = h('input', { type: 'text', value: C.mdns, maxlength: 24, style: { width: '180px' } });
    box.append(
      row('Грати після ввімкнення', 'продовжити станцію, якщо радіо грало, коли його вимкнули', sw(+C.sst, v => send('smartstart=' + (v ? 1 : 0)))),
      row('Показник рівня', 'стрілки VU на екрані плеєра', sw(+C.vu, v => send('vumeter=' + (v ? 1 : 0)))),
      row('Подробиці потоку в журналі', 'для діагностики через USB', sw(+C.aif, v => send('audioinfo=' + (v ? 1 : 0)))),
      row('Точка доступу, якщо мережі немає', 'через стільки хвилин (0 — одразу)', sa),
      row('Ім\'я в мережі', `відкривається як http://${C.mdns || 'potuzhne'}.local`, h('div', { class: 'bar' }, md, btn('Змінити', null, () => { send('mdnsname=' + md.value.trim()); send('rebootmdns=1'); toast('Радіо перезавантажується з новим ім\'ям'); }, 'sm'))),
      row('Telnet', 'керування через telnet у локальній мережі', sw(+C.telnet, v => send('telnet=' + (v ? 1 : 0)))),
      row('Сторож', 'перезапуск, якщо звук завис', sw(+C.watchdog, v => send('watchdog=' + (v ? 1 : 0)))));
  });
};

/* ---------------------------------------------------------------- розробник */
const DACS = [
  { n: 'ES8311', k: 'вбудований кодек і підсилювач', d: ['Кодек ES8311 і підсилювач SC8002B на динамік плати.', 'Нічого під\'єднувати не треба. (ES8388 на цій платі немає.)'], p: [] },
  { n: 'PCM5102A', k: 'стерео ЦАП, лінійний вихід', d: ['Стерео ЦАП із лінійним виходом 3,5 мм — для підсилювача чи колонок.', 'SCK модуля — на GND, XSMT — на 3,3 В, інакше звуку не буде.'],
    p: [['5V UART', 'VIN'], ['GND UART', 'GND'], ['IO14', 'BCK'], ['IO21', 'LCK'], ['IO2', 'DIN'], ['GND I2C', 'SCK'], ['3V3 I2C', 'XSMT']] },
  { n: 'UDA1334A', k: 'стерео ЦАП, навушники й лінія', d: ['Стерео ЦАП із виходом на навушники й лінію.', 'Живлення від 3 до 5 В.'],
    p: [['5V UART', 'VIN'], ['GND UART', 'GND'], ['IO14', 'BCLK'], ['IO21', 'WSEL'], ['IO2', 'DIN']] },
  { n: 'MAX98357A', k: 'моно підсилювач 3 Вт', d: ['Моно підсилювач класу D на динамік 4–8 Ом, до 3 Вт.', 'Виводи GAIN і SD не під\'єднувати.'],
    p: [['5V UART', 'VIN'], ['GND UART', 'GND'], ['IO14', 'BCLK'], ['IO21', 'LRC'], ['IO2', 'DIN']] },
  { n: 'VS1053B', k: 'окремий декодер, інша прошивка', d: ['Апаратний декодер MP3/AAC/FLAC зі своїм виходом.', 'Під\'єднується без пайки — лише роз\'єми на звороті плати. Потрібна окрема прошивка: з цією він не заграє.'],
    p: [['5V UART', '5V'], ['GND UART', 'GND'], ['IO14', 'SCK'], ['IO21', 'MOSI'], ['IO3', 'MISO'], ['IO2', 'XCS'], ['TXD0', 'XDCS'], ['RXD0', 'DREQ'], ['3V3 I2C', 'XRST']], alt: true },
];
VIEWS.dev = page => {
  const tg = h('div'), dac = h('div'), lg = h('div');
  page.append(h('p', { class: 'lead' }, 'Для тих, хто перебудовує радіо: що показувати на екрані і куди йде звук.'),
    card('На екрані', tg), card('Аудіовихід', dac), card('Логотипи станцій', lg));
  let sel = null, s1 = '', s2 = '';
  live(() => {
    if (!S) return;
    const d = S.dev;
    const sig1 = [d.noBat, d.noIp, d.noSd].join('|');
    if (sig1 !== s1) {
      s1 = sig1; tg.textContent = '';
      tg.append(row('Батарея', 'значок заряду й попередження про низький заряд', sw(!d.noBat, v => setx({ noBat: v ? 0 : 1 }))),
        row('IP-адреса', 'на головному екрані — щоб знати, куди заходити з браузера', sw(!d.noIp, v => setx({ noIp: v ? 0 : 1 }))),
        row('Функції картки пам\'яті', 'плеєр картки, запис ефіру, кнопка джерела в шапці', sw(!d.noSd, v => setx({ noSd: v ? 0 : 1 }))));
    }
    if (sel === null) sel = d.dac;
    const sig2 = sel + '|' + d.dac;
    if (sig2 !== s2) {
      s2 = sig2; dac.textContent = '';
      const cards = h('div', { class: 'dacs' });
      DACS.forEach((x, i) => cards.append(h('button', { type: 'button', class: 'dac' + (i === sel ? ' sel' : '') + (i === d.dac ? ' cur' : ''), onclick: () => { sel = i; s2 = ''; pageLive('x'); } },
        h('b', null, x.n), h('small', null, i === d.dac ? 'зараз звук іде сюди' : x.k))));
      const x = DACS[sel];
      const det = h('div', { style: { marginTop: '14px' } }, h('p', { style: { margin: '0 0 4px' } }, x.d[0]), h('p', { class: 'note', style: { marginTop: 0 } }, x.d[1]));
      if (x.p.length) {
        det.append(h('table', { class: 'wire' }, h('tbody', null, h('tr', null, h('td', { class: 'gd' }, 'Плата (роз\'єм)'), h('td'), h('td', { class: 'gd' }, x.n)),
          x.p.map(([a, b]) => h('tr', null, h('td', { class: /^(5V|3V3)/.test(a) ? 'pw' : /^GND/.test(a) ? 'gd' : 'io' }, a), h('td', null, '→'), h('td', null, b))))));
        const used = new Set(x.p.map(p => p[0].replace(/ .*/, '')));
        const HD = [['Роз\'єм розширення (Expand)', ['IO2', 'IO3', 'IO14', 'IO21']], ['Роз\'єм UART', ['TXD0', 'RXD0', 'GND', '5V']], ['Роз\'єм I2C', ['3.3V', 'GND', 'SCL', 'SDA']]];
        const u = (hn, pin) => (pin === '3.3V' && x.p.some(p => p[0] === '3V3 I2C')) || (pin === 'GND' && x.p.some(p => p[0] === (hn.includes('UART') ? 'GND UART' : 'GND I2C'))) || (pin === '5V' && x.p.some(p => p[0] === '5V UART')) || used.has(pin);
        det.append(h('div', { class: 'hdrs' }, HD.map(([n, pins]) => h('div', { class: 'hdr' }, h('b', null, n), h('ol', null, pins.map(p => h('li', { class: u(n, p) ? 'u' : '' }, p)))))),
          h('p', { class: 'note' }, 'Усі три роз\'єми — на звороті плати, паяти не треба. Жовтим позначено виводи, які використовує цей модуль. Дроти — 1,25 мм (Expand) і 2,54 мм.'));
      }
      det.append(h('div', { class: 'bar', style: { marginTop: '12px' } },
        sel === d.dac ? h('span', { class: 'pill ok' }, 'Звук іде сюди') :
        x.alt ? h('span', { class: 'pill bad' }, 'Потрібна окрема прошивка') :
        btn('Увімкнути ' + x.n, 'check', async () => { if (sel !== 0 && !await confirmBox('Перемкнути звук?', `Звук піде на ${x.n}. Вбудований динамік замовкне. Повернути можна тут або в меню радіо «розробник».`, 'Перемкнути')) return; await setx({ dac: sel }); setTimeout(pollState, 500); }, 'acc')));
      dac.append(cards, det);
    }
    if (!lg.dataset.ok) {
      lg.dataset.ok = 1;
      lg.append(h('div', { class: 'bar' }, h('span', { style: { flex: 1 } }, 'Радіо саме шукає логотипи в каталозі radio-browser.info, коли станція заграє. Не знайдені позначаються, щоб не питати щоразу.'),
        btn('Шукати знову', 'refresh', async () => { await setx({ logoForget: 1 }); toast('Позначки скинуто — логотипи знайдуться, коли станції заграють'); }, 'sm')),
        h('p', { class: 'note' }, 'Свій логотип для станції можна поставити в розділі «Станції» → олівець → «Свій логотип».'));
    }
  });
};

/* ---------------------------------------------------------------- оновлення */
function xhrUpload(url, fd, bar, note) {
  return new Promise((res, rej) => {
    const x = new XMLHttpRequest();
    x.open('POST', url);
    x.upload.onprogress = e => { if (e.lengthComputable) { bar.style.width = (e.loaded / e.total * 100) + '%'; note.textContent = `Надіслано ${size(e.loaded)} з ${size(e.total)}`; } };
    x.onload = () => (x.status >= 200 && x.status < 400) ? res(x.responseText) : rej(new Error(x.responseText || 'помилка ' + x.status));
    x.onerror = () => rej(new Error('зв\'язок перервався'));
    x.send(fd);
  });
}
function waitBack(note, was) {
  let n = 0;
  const t = setInterval(async () => {
    n++; note.textContent = `Радіо перезавантажується… ${n * 2} с`;
    try {
      const r = await fetch('/api/state', { cache: 'no-store' });
      if (r.ok) {
        const s = await r.json(); clearInterval(t);
        note.textContent = s.build && s.build !== was ? `Готово: тепер ПОТУЖНЕ РАДІО ${s.v}, зібрано ${s.build}.` : 'Радіо знову на зв\'язку.';
        setTimeout(() => location.reload(), 2500);
      }
    } catch (e) {}
    if (n > 60) { clearInterval(t); note.textContent = 'Радіо довго не відповідає. Оновіть сторінку вручну.'; }
  }, 2000);
}
VIEWS.update = page => {
  const now = h('dl', { class: 'kv' });
  const drawNow = () => { now.textContent = ''; now.append(h('dt', null, 'Версія'), h('dd', null, (S && S.v) || VER || '—'), h('dt', null, 'Зібрано'), h('dd', null, (S && S.build) || BUILD || '—')); };
  drawNow();

  const bar1 = h('i'), note1 = h('p', { class: 'note' }), err1 = h('p', { class: 'note', style: { color: 'var(--bad)' } });
  const what = h('div');
  const fwp = filePick('.bin', false, 'Вибрати файл прошивки'), fw = fwp.input;
  let fwInfo = null;
  const go1 = btn('Оновити радіо', 'upload', async () => {
    const f = fw.files[0];
    if (!f || !fwInfo) { err1.textContent = 'Спершу виберіть файл PotuzhneRadio-ES3C28P-update.bin.'; return; }
    const same = S && S.build === fwInfo.build;
    if (!await confirmBox('Оновити прошивку?', `ПОТУЖНЕ РАДІО ${fwInfo.ver}, зібрано ${fwInfo.build}.${same ? ' Це та сама збірка, що вже стоїть.' : ''} Звук зупиниться; не вимикайте радіо близько хвилини, доки йде оновлення.`, 'Оновити')) return;
    err1.textContent = ''; go1.disabled = true; fwp.querySelector('button').disabled = true;
    const was = S && S.build;
    try {
      const fd = new FormData(); fd.append('updatetarget', 'fw'); fd.append('update', f, f.name);
      const r = await xhrUpload('/update', fd, bar1, note1);
      if (!/^OK/.test(r)) throw new Error(r.replace(/<[^>]+>/g, ' '));
      note1.textContent = 'Прошивку записано. Радіо перезавантажується…';
      waitBack(note1, was);
    } catch (e) { err1.textContent = 'Не вдалося: ' + e.message; go1.disabled = false; fwp.querySelector('button').disabled = false; }
  }, 'acc');
  go1.disabled = true;
  fw.addEventListener('change', async () => {
    fwInfo = null; go1.disabled = true; err1.textContent = ''; what.textContent = '';
    const f = fw.files[0]; if (!f) return;
    what.append(h('p', { class: 'note' }, 'Перевіряю файл…'));
    const r = await readFirmware(f);
    what.textContent = '';
    if (r.err) { err1.textContent = r.err; return; }
    fwInfo = r;
    const a = parseBuild(r.build), b = parseBuild(S && S.build);
    const cmp = !b || !a ? '' : a > b ? 'новіша за ту, що на радіо' : a < b ? 'старіша за ту, що на радіо — радіо повернеться до неї' : 'та сама, що вже на радіо';
    what.append(h('dl', { class: 'kv', style: { margin: '4px 0 12px' } },
      h('dt', null, 'У файлі'), h('dd', null, `ПОТУЖНЕ РАДІО ${r.ver}`),
      h('dt', null, 'Зібрано'), h('dd', null, r.build, cmp ? h('span', { class: 'pill ' + (a > b ? 'ok' : a < b ? 'bad' : ''), style: { marginLeft: '8px' } }, cmp) : null),
      h('dt', null, 'Розмір'), h('dd', null, size(f.size))));
    go1.disabled = false;
  });

  const bar2 = h('i'), note2 = h('p', { class: 'note' }), err2 = h('p', { class: 'note', style: { color: 'var(--bad)' } });
  const wfp = filePick('.gz,.js,.css', true, 'Вибрати файли сторінки'), wf = wfp.input;
  const go2 = btn('Оновити сторінку', 'upload', async () => {
    const fs = [...wf.files];
    if (!fs.length) { err2.textContent = 'Виберіть app.js.gz і app.css.gz.'; return; }
    const bad = fs.filter(f => !WEB_OK.test(f.name));
    if (bad.length) { err2.textContent = 'Це не файли сторінки: ' + bad.map(f => f.name).join(', ') + '. Потрібні app.js.gz і app.css.gz.'; return; }
    err2.textContent = ''; go2.disabled = true;
    try {
      const fd = new FormData(); fs.forEach(f => fd.append('www', f, f.name));
      await xhrUpload('/webboard', fd, bar2, note2);
      note2.textContent = 'Готово. Оновлюю сторінку…'; setTimeout(() => location.reload(), 1500);
    } catch (e) { err2.textContent = 'Не вдалося: ' + e.message; go2.disabled = false; }
  }, 'acc');

  /*  резервна копія: обране живе в пам'яті налаштувань, тож віддаємо його файлом тут  */
  const favFile = h('input', { type: 'file', accept: '.json', hidden: true });
  favFile.onchange = async () => {
    const f = favFile.files[0]; if (!f) return;
    let arr; try { arr = JSON.parse(await f.text()); } catch (e) { toast('Це не файл обраного', true); return; }
    if (!Array.isArray(arr)) { toast('Це не файл обраного', true); return; }
    for (let i = 0; i < 6; i++) { const x = arr[i] || {}; if (x.n && /^https?:\/\//.test(x.u || '')) await setx({ favPut: `${i}\t${x.n}\t${x.u}` }); }
    toast('Обране відновлено'); pollState();
  };
  const wifiFile = h('input', { type: 'file', accept: '.csv,.txt', hidden: true });
  wifiFile.onchange = async () => {
    const f = wifiFile.files[0]; if (!f) return;
    const lines = (await f.text()).split(/\r?\n/).filter(l => /^[^\t]+\t.*$/.test(l));
    if (!lines.length) { toast('У файлі немає жодної мережі (формат: назва, табуляція, пароль)', true); return; }
    if (!await confirmBox('Відновити мережі?', `${lines.length} ${plural(lines.length, 'мережа', 'мережі', 'мереж')} із файлу замінять збережені. Нові набудуть сили після перезавантаження радіо.`, 'Відновити')) return;
    const fd = new FormData(); fd.append('data', new Blob([lines.slice(0, 5).join('\n') + '\n']), 'wifi.csv');
    try { await api('/webboard', { method: 'POST', body: fd }); toast('Мережі записано. Перезавантажте радіо в розділі «Система».'); } catch (e) { toast('Не вдалося: ' + e.message, true); }
  };

  page.append(
    card('Зараз на радіо', now),
    card('Прошивка',
      h('ol', { style: { margin: '0 0 12px', paddingLeft: '20px', lineHeight: 1.6 } },
        h('li', null, 'Зберіть прошивку: ', h('code', null, 'firmware/rebuild.sh'), ' (або візьміть готову).'),
        h('li', null, 'Виберіть файл ', h('b', null, 'firmware/PotuzhneRadio-ES3C28P-update.bin'), ' — сторінка покаже його версію й дату збірки.'),
        h('li', null, 'Натисніть «Оновити радіо». Станції, мережі, обране й налаштування лишаються.')),
      fwp, what, h('div', { class: 'prog' }, bar1), h('div', { class: 'bar' }, go1), err1, note1),
    card('Сторінка', h('p', { style: { marginTop: 0 } }, 'Файли ', h('b', null, 'app.js.gz'), ' і ', h('b', null, 'app.css.gz'), ' з теки ', h('b', null, 'firmware/web/'), '. Прошивка й сторінка оновлюються окремо; на час надсилання звук зупиниться.'),
      wfp, h('div', { class: 'prog' }, bar2), h('div', { class: 'bar' }, go2), err2, note2),
    card('Резервна копія',
      row('Список станцій', 'playlist.csv — повернути можна в «Станції → Імпорт» із переглядом', h('a', { class: 'btn sm', href: '/api/stations', download: 'playlist.csv' }, ic('download'), 'Зберегти')),
      row('Обране', 'шість кнопок швидкого вибору', h('div', { class: 'bar' }, btn('Зберегти', 'download', () => download('obrane.json', JSON.stringify((S && S.fav) || [], null, 1), 'application/json'), 'sm'), btn('Відновити', 'upload', () => favFile.click(), 'sm ghost'), favFile)),
      row('Мережі Wi-Fi', 'wifi.csv — з паролями відкритим текстом, зберігайте надійно', h('div', { class: 'bar' }, h('a', { class: 'btn sm', href: '/data/wifi.csv', download: 'wifi.csv' }, ic('download'), 'Зберегти'), btn('Відновити', 'upload', () => wifiFile.click(), 'sm ghost'), wifiFile))),
    card('Якщо сторінка не відкривається', h('p', { style: { margin: 0 } }, 'Під\'єднайте радіо кабелем USB до Mac і в теці ', h('b', null, 'firmware'), ' виконайте ', h('code', null, './flash.sh --app-only'), ' — оновиться лише програма, списки лишаться. ', h('code', null, './flash.sh'), ' без параметрів пише повний образ і стирає станції та мережі.')));
  live(() => drawNow());
};

/* ---------------------------------------------------------------- про пристрій */
VIEWS.about = page => {
  const kv = h('dl', { class: 'kv' });
  page.append(card('ПОТУЖНЕ РАДІО', kv), card(null, h('p', { class: 'note', style: { margin: 0 } }, 'Плата ES3C28P: ESP32-S3, екран 2,8″ ILI9341 із сенсором, кодек ES8311, картка пам\'яті, 16 МБ флеш, 8 МБ PSRAM.')));
  live(() => {
    if (!S) return;
    const up = S.up, d = Math.floor(up / 86400);
    const rows = [
      ['Версія', `${S.v}, збірка ${S.build || '—'}`],
      ['Мережа', S.ssid || 'точка доступу'], ['Адреса', S.ip], ['Сигнал', S.rssi ? S.rssi + ' дБм' : '—'],
      ['Час на радіо', S.time ? `${S.time}, ${S.date}` : 'ще невідомий'],
      ['Працює', (d ? d + ' д ' : '') + mmss(up % 86400)],
      ['Потік', W.playing && W.bitrate ? `${W.bitrate} кбіт/с${S.codec ? ', ' + S.codec : ''}` : '—'],
      ['Батарея', S.dev.noBat ? 'вимкнено' : S.bat.pct >= 0 ? `${S.bat.pct}% · ${(S.bat.mv / 1000).toFixed(2)} В${S.bat.chg ? ' · заряджається' : S.bat.full ? ' · заряджено' : S.bat.pwr ? ' · від USB' : ''}` : 'немає'],
      ['Пам\'ять', `${size(S.heap)} вільно, PSRAM ${size(S.psram)}`],
      ['Файлова система', `${size(S.fs.u)} з ${size(S.fs.t)}`],
      ['Аудіовихід', DACS[S.dev.dac].n],
    ];
    kv.textContent = '';
    rows.forEach(([a, b]) => kv.append(h('dt', null, a), h('dd', null, b)));
  });
};

/* ---------------------------------------------------------------- старт */
function start() {
  shell();
  window.addEventListener('hashchange', route);
  route();
  if (!AP) { wsConnect(); loadPlayList(); loadLogos(); }
  pollState();
  /*  гучність із клавіатури: стрілки вгору/вниз, пробіл — пауза  */
  document.addEventListener('keydown', e => {
    if (AP || e.target.closest('input,select,textarea,.dlg') || e.metaKey || e.ctrlKey || e.altKey) return;
    if (e.key === ' ') { e.preventDefault(); send('toggle=1'); }
  });
}
start();
