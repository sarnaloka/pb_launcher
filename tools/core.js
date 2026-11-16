/* Bus AMB — lògica (GTFS estàtic, GTFS-RT, horaris, PDF). Sense dependències de DOM. */
(function (root) {
'use strict';
const C = {};

/* ───────────── ZIP (lector mínim; descompressió amb DecompressionStream) ───────────── */
async function zipEntries(blob) {
  const size = blob.size, tailLen = Math.min(size, 65557);
  const tail = new Uint8Array(await blob.slice(size - tailLen).arrayBuffer());
  const dv = new DataView(tail.buffer);
  let eocd = -1;
  for (let i = tail.length - 22; i >= 0; i--) if (dv.getUint32(i, true) === 0x06054b50) { eocd = i; break; }
  if (eocd < 0) throw new Error('El fitxer no és un ZIP vàlid');
  const n = dv.getUint16(eocd + 10, true), cdSize = dv.getUint32(eocd + 12, true), cdOff = dv.getUint32(eocd + 16, true);
  const cd = new Uint8Array(await blob.slice(cdOff, cdOff + cdSize).arrayBuffer());
  const cv = new DataView(cd.buffer), td = new TextDecoder(), out = {};
  let p = 0;
  for (let k = 0; k < n; k++) {
    if (p + 46 > cd.length || cv.getUint32(p, true) !== 0x02014b50) break;
    const method = cv.getUint16(p + 10, true), csize = cv.getUint32(p + 20, true), usize = cv.getUint32(p + 24, true),
      nl = cv.getUint16(p + 28, true), el = cv.getUint16(p + 30, true), cl = cv.getUint16(p + 32, true), lho = cv.getUint32(p + 42, true);
    const name = td.decode(cd.subarray(p + 46, p + 46 + nl));
    out[name.split('/').pop().toLowerCase()] = { method, csize, usize, lho };
    p += 46 + nl + el + cl;
  }
  return out;
}
async function zipStream(blob, e) {
  const lh = new DataView(await blob.slice(e.lho, e.lho + 30).arrayBuffer());
  const start = e.lho + 30 + lh.getUint16(26, true) + lh.getUint16(28, true);
  const raw = blob.slice(start, start + e.csize).stream();
  if (e.method === 0) return raw;
  if (e.method === 8) {
    if (typeof DecompressionStream === 'undefined') throw new Error('Aquest navegador no pot descomprimir ZIP (cal iOS 16.4 o posterior)');
    return raw.pipeThrough(new DecompressionStream('deflate-raw'));
  }
  throw new Error('Mètode de compressió ZIP no suportat');
}

/* ───────────── CSV ───────────── */
function csvLine(line) {
  if (line.indexOf('"') < 0) return line.split(',');
  const out = []; let cur = '', q = false;
  for (let i = 0; i < line.length; i++) {
    const ch = line[i];
    if (q) {
      if (ch === '"') { if (line[i + 1] === '"') { cur += '"'; i++; } else q = false; }
      else cur += ch;
    } else if (ch === '"') q = true;
    else if (ch === ',') { out.push(cur); cur = ''; }
    else cur += ch;
  }
  out.push(cur);
  return out;
}
/* Llegeix una taula del zip i crida onRow(camps, índexDeColumnes). Retorna false si no existeix. */
async function readTable(blob, ent, name, onRow, onProgress) {
  const e = ent[name];
  if (!e) return false;
  const reader = (await zipStream(blob, e)).getReader();
  const dec = new TextDecoder('utf-8');
  let rest = '', H = null, bytes = 0, lastP = 0;
  const handle = (line) => {
    if (line.charCodeAt(line.length - 1) === 13) line = line.slice(0, -1);
    if (!line) return;
    if (!H) {
      if (line.charCodeAt(0) === 0xFEFF) line = line.slice(1);
      H = {}; csvLine(line).forEach((h, i) => { H[h.trim()] = i; });
      return;
    }
    onRow(csvLine(line), H);
  };
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    bytes += value.length;
    const text = rest + dec.decode(value, { stream: true });
    let s = 0, i;
    while ((i = text.indexOf('\n', s)) >= 0) { handle(text.slice(s, i)); s = i + 1; }
    rest = text.slice(s);
    if (onProgress && bytes - lastP > 1 << 20) { lastP = bytes; onProgress(Math.min(1, bytes / (e.usize || bytes))); }
  }
  rest += dec.decode();
  if (rest) handle(rest);
  return true;
}

function tsec(s) {
  if (!s) return -1;
  const a = s.indexOf(':');
  if (a < 0) return -1;
  const h = parseInt(s, 10), m = parseInt(s.substr(a + 1, 2), 10), x = parseInt(s.substr(a + 4, 2), 10) || 0;
  if (isNaN(h) || isNaN(m)) return -1;
  return h * 3600 + m * 60 + x;
}
const f = (c, i) => (i === undefined || c[i] === undefined ? '' : c[i].trim());

/* ───────────── Importació del GTFS ───────────── */
const PFX = /^([A-Za-z0-9]{2,5})_/;
const pfx = (id) => { const m = PFX.exec(id); return m ? m[1] : ''; };
/* Operadors d'un feed combinat (ids amb prefix tipus AMB_, TMB_...). [] si el feed és d'un sol operador. */
C.listOperators = async function (blob) {
  const ent = await zipEntries(blob), m = new Map();
  let total = 0, withP = 0;
  await readTable(blob, ent, 'routes.txt', (c, H) => {
    const k = pfx(f(c, H.route_id)); total++;
    if (!k) return; withP++;
    let e = m.get(k); if (!e) m.set(k, e = { prefix: k, routes: 0, name: '' }); e.routes++;
  });
  if (!total || withP / total < 0.9 || m.size < 2) return [];
  await readTable(blob, ent, 'agency.txt', (c, H) => { const e = m.get(pfx(f(c, H.agency_id))); if (e && !e.name) e.name = f(c, H.agency_name); });
  return [...m.values()].sort((a, b) => b.routes - a.routes);
};

C.importGtfs = async function (blob, progress, opts) {
  progress = progress || function () {};
  const keep = opts && opts.prefixes && opts.prefixes.length ? new Set(opts.prefixes) : null;
  const ent = await zipEntries(blob);
  for (const need of ['routes.txt', 'trips.txt', 'stops.txt', 'stop_times.txt'])
    if (!ent[need]) throw new Error('Al ZIP hi falta ' + need + ' (és el google_transit.zip d\'AMB?)');

  progress('Línies', 0.01);
  const routes = [], routeIdx = new Map();
  await readTable(blob, ent, 'routes.txt', (c, H) => {
    const id = f(c, H.route_id);
    if (keep && !keep.has(pfx(id))) return;
    routeIdx.set(id, routes.length);
    routes.push({ id, short: f(c, H.route_short_name), long: f(c, H.route_long_name),
      color: f(c, H.route_color).replace('#', ''), text: f(c, H.route_text_color).replace('#', '') });
  });

  progress('Parades', 0.03);
  const stops = [], stopIdx = new Map();
  await readTable(blob, ent, 'stops.txt', (c, H) => {
    const id = f(c, H.stop_id);
    if (keep && !keep.has(pfx(id))) return;
    stopIdx.set(id, stops.length);
    stops.push({ id, code: f(c, H.stop_code), name: f(c, H.stop_name),
      lat: parseFloat(f(c, H.stop_lat)) || 0, lon: parseFloat(f(c, H.stop_lon)) || 0, routes: [], par: f(c, H.parent_station) });
  });
  stops.forEach((st) => { const pi = st.par ? stopIdx.get(st.par) : undefined; st.par = pi === undefined ? -1 : pi; });

  const svcIdx = new Map(), svcDays = [], svcStart = [], svcEnd = [];
  const svc = (id) => {
    let i = svcIdx.get(id);
    if (i === undefined) { i = svcIdx.size; svcIdx.set(id, i); svcDays.push(0); svcStart.push(0); svcEnd.push(0); }
    return i;
  };
  progress('Viatges', 0.08);
  const tripIds = [], tRoute = [], tSvc = [], tDir = [], tHead = [], heads = [], headIdx = new Map(), tripIdx = new Map();
  await readTable(blob, ent, 'trips.txt', (c, H) => {
    const r = routeIdx.get(f(c, H.route_id));
    if (r === undefined) return;
    const id = f(c, H.trip_id), h = f(c, H.trip_headsign);
    let hi = headIdx.get(h);
    if (hi === undefined) { hi = heads.length; heads.push(h); headIdx.set(h, hi); }
    tripIdx.set(id, tripIds.length);
    tripIds.push(id); tRoute.push(r); tSvc.push(svc(f(c, H.service_id)));
    const d = f(c, H.direction_id); tDir.push(d === '' ? 0 : (parseInt(d, 10) || 0)); tHead.push(hi);
  });
  progress('Calendari', 0.05);
  const DAYS = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];
  await readTable(blob, ent, 'calendar.txt', (c, H) => {
    const i = svcIdx.get(f(c, H.service_id));
    if (i === undefined) return;
    let m = 0;
    DAYS.forEach((d, k) => { if (f(c, H[d]) === '1') m |= 1 << k; });
    svcDays[i] = m; svcStart[i] = parseInt(f(c, H.start_date), 10) || 0; svcEnd[i] = parseInt(f(c, H.end_date), 10) || 0;
  });
  const ex = [];
  await readTable(blob, ent, 'calendar_dates.txt', (c, H) => {
    const i = svcIdx.get(f(c, H.service_id));
    if (i === undefined) return;
    ex.push(i, parseInt(f(c, H.date), 10) || 0, parseInt(f(c, H.exception_type), 10) || 0);
  });

  const nT = tripIds.length;

  progress('Horaris de pas', 0.12);
  let cap = 1 << 18, n = 0;
  let rTrip = new Int32Array(cap), rStop = new Int32Array(cap), rTime = new Int32Array(cap), rSeq = new Int32Array(cap);
  const grow = () => {
    cap *= 2;
    const g = (a) => { const b = new Int32Array(cap); b.set(a); return b; };
    rTrip = g(rTrip); rStop = g(rStop); rTime = g(rTime); rSeq = g(rSeq);
  };
  let lastTid = null, lastT = -1;
  await readTable(blob, ent, 'stop_times.txt', (c, H) => {
    const tid = c[H.trip_id];
    let t;
    if (tid === lastTid) t = lastT;
    else { t = tripIdx.get(tid.trim()); lastTid = tid; lastT = t; }
    if (t === undefined) return;
    const s = stopIdx.get(f(c, H.stop_id));
    if (s === undefined) return;
    if (n === cap) grow();
    let tm = tsec(f(c, H.departure_time));
    if (tm < 0) tm = tsec(f(c, H.arrival_time));
    rTrip[n] = t; rStop[n] = s; rTime[n] = tm; rSeq[n] = parseInt(c[H.stop_sequence], 10) || 0;
    n++;
  }, (p) => progress('Horaris de pas', 0.12 + p * 0.7));

  progress('Indexant', 0.85);
  let sorted = true;
  for (let i = 1; i < n; i++)
    if (rTrip[i] < rTrip[i - 1] || (rTrip[i] === rTrip[i - 1] && rSeq[i] < rSeq[i - 1])) { sorted = false; break; }
  let sTrip, sStop, sTime, sSeq;
  if (sorted) {
    sTrip = rTrip.subarray(0, n); sStop = rStop.slice(0, n); sTime = rTime.slice(0, n); sSeq = rSeq.slice(0, n);
  } else {
    const ord = new Uint32Array(n);
    for (let i = 0; i < n; i++) ord[i] = i;
    ord.sort((a, b) => rTrip[a] - rTrip[b] || rSeq[a] - rSeq[b] || a - b);
    sTrip = new Int32Array(n); sStop = new Int32Array(n); sTime = new Int32Array(n); sSeq = new Int32Array(n);
    for (let i = 0; i < n; i++) { const o = ord[i]; sTrip[i] = rTrip[o]; sStop[i] = rStop[o]; sTime[i] = rTime[o]; sSeq[i] = rSeq[o]; }
  }
  const tStart = new Int32Array(nT).fill(-1), tLen = new Int32Array(nT);
  for (let i = 0; i < n; i++) { const t = sTrip[i]; if (tStart[t] < 0) tStart[t] = i; tLen[t]++; }
  // hores buides: interpolació lineal dins del viatge
  for (let t = 0; t < nT; t++) {
    const a = tStart[t], b = a + tLen[t];
    if (a < 0) continue;
    let prev = -1;
    for (let i = a; i < b; i++) {
      if (sTime[i] >= 0) {
        if (prev >= 0 && i - prev > 1) for (let k = prev + 1; k < i; k++) sTime[k] = Math.round(sTime[prev] + (sTime[i] - sTime[prev]) * (k - prev) / (i - prev));
        else if (prev < 0) for (let k = a; k < i; k++) sTime[k] = sTime[i];
        prev = i;
      }
    }
    if (prev >= 0) for (let k = prev + 1; k < b; k++) sTime[k] = sTime[prev];
  }
  // índex per parada
  const nS = stops.length, stopStart = new Int32Array(nS + 1);
  for (let i = 0; i < n; i++) stopStart[sStop[i] + 1]++;
  for (let s = 0; s < nS; s++) stopStart[s + 1] += stopStart[s];
  const fill = stopStart.slice(0, nS), stopRows = new Int32Array(n);
  for (let i = 0; i < n; i++) stopRows[fill[sStop[i]]++] = i;
  const seen = new Set();
  for (let i = 0; i < n; i++) {
    const s = sStop[i], r = tRoute[sTrip[i]], k = s * 65536 + r;
    if (!seen.has(k)) { seen.add(k); stops[s].routes.push(r); }
  }

  const feed = {};
  await readTable(blob, ent, 'feed_info.txt', (c, H) => {
    feed.version = f(c, H.feed_version); feed.start = f(c, H.feed_start_date); feed.end = f(c, H.feed_end_date);
  });
  progress('Fet', 1);
  return {
    v: 2, importedAt: Date.now(), feed, prefixes: keep ? [...keep] : null, routes, stops, heads,
    svcDays: Uint8Array.from(svcDays), svcStart: Int32Array.from(svcStart), svcEnd: Int32Array.from(svcEnd), ex: Int32Array.from(ex),
    tripIds, tRoute: Int32Array.from(tRoute), tSvc: Int32Array.from(tSvc), tDir: Int8Array.from(tDir), tHead: Int32Array.from(tHead),
    tStart, tLen, sStop, sTime, sSeq, stopStart, stopRows
  };
};

/* Índexs en memòria (no es desen) */
C.prepare = function (db) {
  const tripIdx = new Map();
  db.tripIds.forEach((id, i) => tripIdx.set(id, i));
  const exMap = new Map();
  for (let i = 0; i < db.ex.length; i += 3) {
    let e = exMap.get(db.ex[i + 1]);
    if (!e) exMap.set(db.ex[i + 1], e = { add: [], rem: [] });
    (db.ex[i + 2] === 1 ? e.add : e.rem).push(db.ex[i]);
  }
  const routeIdx = new Map();
  db.routes.forEach((r, i) => routeIdx.set(r.id, i));
  db.stops.forEach((s) => { s.q = C.norm(s.name + ' ' + s.code); });
  db._ = { tripIdx, exMap, routeIdx, act: new Map() };
  return db;
};
C.norm = (s) => String(s).normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase();

/* ───────────── Dates i calendari ───────────── */
C.midnight = (d) => new Date(d.getFullYear(), d.getMonth(), d.getDate());
C.dateInt = (d) => d.getFullYear() * 10000 + (d.getMonth() + 1) * 100 + d.getDate();
C.addDays = (d, k) => new Date(d.getFullYear(), d.getMonth(), d.getDate() + k);
C.activeServices = function (db, date) {
  const di = C.dateInt(date), cache = db._.act;
  let a = cache.get(di);
  if (a) return a;
  const bit = 1 << ((date.getDay() + 6) % 7), nS = db.svcDays.length;
  a = new Uint8Array(nS);
  for (let i = 0; i < nS; i++) if ((db.svcDays[i] & bit) && di >= db.svcStart[i] && di <= db.svcEnd[i]) a[i] = 1;
  const e = db._.exMap.get(di);
  if (e) { e.add.forEach((i) => { a[i] = 1; }); e.rem.forEach((i) => { a[i] = 0; }); }
  if (cache.size > 40) cache.clear();
  cache.set(di, a);
  return a;
};

C.tripDest = function (db, t) {
  const h = db.heads[db.tHead[t]];
  if (h) return h;
  return db.stops[db.sStop[db.tStart[t] + db.tLen[t] - 1]].name;
};

/* ───────────── Sortides d'una parada ───────────── */
C.departures = function (db, stop, nowMs, fromMs, toMs) {
  const out = [], today = C.midnight(new Date(nowMs));
  const a = db.stopStart[stop], b = db.stopStart[stop + 1];
  for (let off = -1; off <= 1; off++) {
    const day = C.addDays(today, off), base = day.getTime(), act = C.activeServices(db, day), di = C.dateInt(day);
    if (base > toMs || base + 30 * 3600e3 < fromMs) continue;
    for (let k = a; k < b; k++) {
      const row = db.stopRows[k], t = tripOfRow(db, row);
      if (!act[db.tSvc[t]]) continue;
      if (row === db.tStart[t] + db.tLen[t] - 1) continue; // final de trajecte
      const when = base + db.sTime[row] * 1000;
      if (when < fromMs || when > toMs) continue;
      out.push({ trip: t, row, seq: db.sSeq[row], sched: when, est: when, base, day: di, route: db.tRoute[t], dest: C.tripDest(db, t), rt: false });
    }
  }
  out.sort((x, y) => x.sched - y.sched);
  return out;
};
/* viatge d'una fila: cerca binària sobre tStart (les files estan ordenades per viatge) */
function tripOfRow(db, row) {
  if (!db._.rowTrip) {
    const rt = new Int32Array(db.sStop.length);
    for (let t = 0; t < db.tStart.length; t++) { const a = db.tStart[t]; if (a >= 0) rt.fill(t, a, a + db.tLen[t]); }
    db._.rowTrip = rt;
  }
  return db._.rowTrip[row];
}
C.tripOfRow = tripOfRow;

/* ───────────── GTFS-Realtime (descodificador protobuf mínim) ───────────── */
function PB(u8, pos, end) { this.b = u8; this.p = pos || 0; this.e = end === undefined ? u8.length : end; }
PB.prototype.varint = function () {
  let lo = 0, hi = 0, shift = 0, b;
  do {
    if (this.p >= this.e) throw new Error('Feed en temps real truncat');
    b = this.b[this.p++];
    if (shift < 28) lo |= (b & 0x7f) << shift;
    else if (shift === 28) { lo |= (b & 0x0f) << 28; hi |= (b & 0x7f) >> 4; }
    else hi |= (b & 0x7f) << (shift - 32);
    shift += 7;
  } while (b & 0x80 && shift < 70);
  this.lo = lo; this.hi = hi;
  return (hi >>> 0) * 4294967296 + (lo >>> 0);
};
PB.prototype.int32 = function () { this.varint(); return this.lo | 0; };
PB.prototype.next = function () { if (this.p >= this.e) return 0; const k = this.varint(); this.wt = k & 7; return k >>> 3; };
PB.prototype.sub = function () { const l = this.varint(); const s = new PB(this.b, this.p, this.p + l); this.p += l; return s; };
PB.prototype.str = function () { const l = this.varint(); const s = TD.decode(this.b.subarray(this.p, this.p + l)); this.p += l; return s; };
PB.prototype.skip = function () {
  const w = this.wt;
  if (w === 0) this.varint(); else if (w === 1) this.p += 8; else if (w === 2) { const l = this.varint(); this.p += l; } else if (w === 5) this.p += 4;
  else throw new Error('Feed en temps real il·legible');
};
const TD = new TextDecoder();
function stEvent(r) {
  const e = {}; let fn;
  while ((fn = r.next())) { if (fn === 1) e.delay = r.int32(); else if (fn === 2) e.time = r.varint(); else r.skip(); }
  return e;
}
/* Afegeix el prefix d'operador als ids d'un feed (per casar-lo amb un GTFS combinat). */
C.prefixFeed = function (feed, p) {
  for (const u of feed.updates) {
    if (u.tripId) u.tripId = p + '_' + u.tripId;
    if (u.routeId) u.routeId = p + '_' + u.routeId;
    for (const st of u.stus) if (st.stopId !== null) st.stopId = p + '_' + st.stopId;
  }
  return feed;
};
C.decodeFeed = function (u8) {
  const r = new PB(u8), feed = { timestamp: 0, updates: [], entities: 0 };
  let fn;
  while ((fn = r.next())) {
    if (fn === 1) { const h = r.sub(); let g; while ((g = h.next())) { if (g === 3) feed.timestamp = h.varint(); else h.skip(); } }
    else if (fn === 2) {
      feed.entities++;
      const e = r.sub(); let g;
      while ((g = e.next())) {
        if (g !== 3) { e.skip(); continue; }
        const tu = e.sub(), u = { tripId: '', startDate: 0, routeId: '', rel: 0, delay: null, stus: [] };
        let h;
        while ((h = tu.next())) {
          if (h === 1) {
            const td = tu.sub(); let x;
            while ((x = td.next())) {
              if (x === 1) u.tripId = td.str(); else if (x === 3) u.startDate = parseInt(td.str(), 10) || 0;
              else if (x === 5) u.routeId = td.str(); else if (x === 4) u.rel = td.varint(); else td.skip();
            }
          } else if (h === 2) {
            const s = tu.sub(), st = { seq: null, stopId: null, arr: null, dep: null, rel: 0 }; let x;
            while ((x = s.next())) {
              if (x === 1) st.seq = s.varint(); else if (x === 4) st.stopId = s.str();
              else if (x === 2) st.arr = stEvent(s.sub()); else if (x === 3) st.dep = stEvent(s.sub());
              else if (x === 5) st.rel = s.varint(); else s.skip();
            }
            u.stus.push(st);
          } else if (h === 5) u.delay = tu.int32();
          else tu.skip();
        }
        feed.updates.push(u);
      }
    } else r.skip();
  }
  return feed;
};

/* Aplica el temps real a les sortides teòriques d'una parada. */
C.applyRealtime = function (db, stop, deps, feed, nowMs) {
  if (!feed) return deps;
  const stopId = db.stops[stop].id, byTrip = new Map(), extra = [];
  for (const u of feed.updates) {
    const t = db._.tripIdx.get(u.tripId);
    if (t === undefined) {
      // viatge que no és al GTFS estàtic: només el podem mostrar si porta hora absoluta per a aquesta parada
      for (const s of u.stus) {
        const ev = s.dep && s.dep.time ? s.dep : s.arr;
        if (s.stopId === stopId && ev && ev.time && s.rel !== 1 && u.rel !== 3) {
          const r = db._.routeIdx.get(u.routeId);
          if (r !== undefined) extra.push({ trip: -1, sched: ev.time * 1000, est: ev.time * 1000, route: r, dest: '', rt: true, added: true });
        }
      }
      continue;
    }
    if (!byTrip.has(t)) byTrip.set(t, []);
    byTrip.get(t).push(u);
  }
  const out = [];
  for (const d of deps) {
    const us = byTrip.get(d.trip);
    let u = null;
    if (us) u = us.find((x) => x.startDate === d.day) || us.find((x) => !x.startDate) ||
      (us.length === 1 && Math.abs(d.sched - nowMs) < 3 * 3600e3 ? us[0] : null);
    if (!u) { out.push(d); continue; }
    if (u.rel === 3) { d.cancelled = true; d.rt = true; out.push(d); continue; }
    const a = db.tStart[d.trip], b = a + db.tLen[d.trip];
    const rowOf = (s) => {
      if (s.seq !== null) for (let i = a; i < b; i++) if (db.sSeq[i] === s.seq) return i;
      if (s.stopId !== null) for (let i = a; i < b; i++) if (db.stops[db.sStop[i]].id === s.stopId) return i;
      return -1;
    };
    const delayOf = (s, row) => {
      const ev = s.dep || s.arr;
      if (!ev) return null;
      if (ev.time) return ev.time - (d.base / 1000 + db.sTime[row]);
      return ev.delay === undefined ? null : ev.delay;
    };
    let hit = null, prior = null, priorRow = -1, anyAfter = false;
    for (const s of u.stus) {
      const row = rowOf(s);
      if (row < 0) continue;
      if (row === d.row) { hit = s; break; }
      if (row < d.row) { if (row > priorRow && s.rel !== 2 && delayOf(s, row) !== null) { prior = s; priorRow = row; } }
      else anyAfter = true;
    }
    if (hit) {
      if (hit.rel === 1) { d.cancelled = true; d.rt = true; out.push(d); continue; }
      const dl = hit.rel === 2 ? null : delayOf(hit, d.row);
      if (dl !== null) { d.est = d.sched + dl * 1000; d.rt = true; }
    } else if (prior) {
      d.est = d.sched + delayOf(prior, priorRow) * 1000; d.rt = true;
    } else if (anyAfter) {
      if (d.sched < nowMs + 3 * 60e3) continue; // el bus ja ha passat per aquesta parada
    } else if (u.delay !== null) {
      d.est = d.sched + u.delay * 1000; d.rt = true;
    }
    out.push(d);
  }
  for (const x of extra) out.push(x);
  out.sort((x, y) => x.est - y.est);
  return out;
};

/* ───────────── Horari per al PDF ───────────── */
const patKey = (db, row, t) => {
  const a = db.tStart[t];
  return db.sStop[a + db.tLen[t] - 1] + '|' + db.sStop[row + 1] + '|' + (row > a ? db.sStop[row - 1] : -1);
};
/* Línies i sentits que surten d'una parada. El sentit es dedueix de l'ordre de les parades
   (no de direction_id, que alguns operadors deixen sempre a 0). */
C.linesAtStop = function (db, stop) {
  const byRoute = new Map();
  for (let k = db.stopStart[stop]; k < db.stopStart[stop + 1]; k++) {
    const row = db.stopRows[k], t = tripOfRow(db, row), end = db.tStart[t] + db.tLen[t];
    if (row === end - 1) continue;
    const r = db.tRoute[t];
    let pats = byRoute.get(r);
    if (!pats) byRoute.set(r, pats = new Map());
    const key = patKey(db, row, t);
    let p = pats.get(key);
    if (!p) pats.set(key, p = { key, row, trip: t, n: 0, rem: end - row, dests: new Map() });
    else if (end - row > p.rem) { p.rem = end - row; p.row = row; p.trip = t; }
    p.n++;
    const d = C.tripDest(db, t);
    p.dests.set(d, (p.dests.get(d) || 0) + 1);
  }
  const out = [];
  for (const [route, pm] of byRoute) {
    let left = [...pm.values()], guard = 0;
    while (left.length) {
      left.sort((x, y) => y.rem - x.rem || y.n - x.n);
      const ref = left[0], g = [ref], rest = [];
      if (++guard < 6) {
        const a = db.tStart[ref.trip], e = a + db.tLen[ref.trip], after = new Set(), before = new Set();
        for (let i = ref.row + 1; i < e; i++) after.add(db.sStop[i]);
        for (let i = a; i < ref.row; i++) before.add(db.sStop[i]);
        for (const p of left.slice(1)) {
          const pe = db.tStart[p.trip] + db.tLen[p.trip];
          let score = 0;
          for (let i = p.row + 1; i < pe; i++) { const st = db.sStop[i], A = after.has(st), B = before.has(st); if (A && !B) score++; else if (B && !A) score--; }
          (score > 0 || (score === 0 && db.tDir[p.trip] === db.tDir[ref.trip]) ? g : rest).push(p);
        }
      } else g.push(...left.slice(1));
      const dests = new Map();
      let n = 0;
      g.forEach((p) => { n += p.n; p.dests.forEach((v, d) => dests.set(d, (dests.get(d) || 0) + v)); });
      out.push({ stop, route, n, keys: new Set(g.map((p) => p.key)), dest: [...dests.entries()].sort((x, y) => y[1] - x[1])[0][0] });
      left = rest;
    }
  }
  out.sort((x, y) => C.natCmp(db.routes[x.route].short, db.routes[y.route].short) || y.n - x.n);
  return out;
};
C.natCmp = (a, b) => String(a).localeCompare(String(b), 'ca', { numeric: true, sensitivity: 'base' });

C.buildTimetable = function (db, grp, ref) {
  const stop = grp.stop, route = grp.route;
  const day0 = C.midnight(ref || new Date());
  const depsOn = (date) => {
    const act = C.activeServices(db, date), out = [];
    for (let k = db.stopStart[stop]; k < db.stopStart[stop + 1]; k++) {
      const row = db.stopRows[k], t = tripOfRow(db, row);
      if (db.tRoute[t] !== route || !act[db.tSvc[t]]) continue;
      const last = db.tStart[t] + db.tLen[t] - 1;
      if (row === last || !grp.keys.has(patKey(db, row, t))) continue;
      out.push({ time: db.sTime[row], trip: t, row, last: db.sStop[last] });
    }
    out.sort((x, y) => x.time - y.time);
    return out;
  };
  // feiners: el patró més repetit dels propers 5 dies de dilluns a divendres (evita festius)
  const wk = [];
  let sat = null, sun = null;
  for (let i = 0; i < 14 && (wk.length < 5 || !sat || !sun); i++) {
    const d = C.addDays(day0, i), w = d.getDay();
    if (w === 6) { if (!sat) sat = d; } else if (w === 0) { if (!sun) sun = d; } else if (wk.length < 5) wk.push(d);
  }
  const sigs = new Map();
  let best = null;
  for (const d of wk) {
    const deps = depsOn(d), sig = deps.map((x) => x.time).join(',');
    let e = sigs.get(sig);
    if (!e) sigs.set(sig, e = { n: 0, deps, date: d });
    e.n++;
    if (!best || e.n > best.n) best = e;
  }
  const cols = [
    { label: 'Feiners (dl.-dv.)', short: 'Feiners', date: best.date, deps: best.deps },
    { label: 'Dissabte', short: 'Dissabte', date: sat, deps: depsOn(sat) },
    { label: 'Diumenge i festius', short: 'Festius', date: sun, deps: depsOn(sun) }
  ];
  const all = [].concat(cols[0].deps, cols[1].deps, cols[2].deps);
  const cnt = new Map();
  all.forEach((x) => cnt.set(x.last, (cnt.get(x.last) || 0) + 1));
  const lasts = [...cnt.entries()].sort((x, y) => y[1] - x[1]).map((x) => x[0]);
  const main = lasts[0];
  const notes = lasts.slice(1).map((s, i) => ({ letter: String.fromCharCode(65 + i), stop: s, text: '' }));
  const letterOf = new Map(notes.map((x) => [x.stop, x.letter]));
  let rep = null, hcount = new Map();
  all.forEach((x) => {
    if (x.last !== main) return;
    const rem = db.tStart[x.trip] + db.tLen[x.trip] - x.row;
    if (!rep || rem > rep.rem) rep = { rem, x };
    const h = db.heads[db.tHead[x.trip]];
    if (h) hcount.set(h, (hcount.get(h) || 0) + 1);
  });
  const strip = [];
  if (rep) {
    const t = rep.x.trip, end = db.tStart[t] + db.tLen[t];
    for (let i = rep.x.row; i < end; i++) strip.push({ name: db.stops[db.sStop[i]].name, min: Math.round((db.sTime[i] - db.sTime[rep.x.row]) / 60) });
  }
  const onStrip = new Set();
  if (rep) { const t = rep.x.trip, end = db.tStart[t] + db.tLen[t]; for (let i = rep.x.row; i < end; i++) onStrip.add(db.sStop[i]); }
  notes.forEach((nt) => { nt.text = (onStrip.has(nt.stop) ? 'fins a ' : 'continua fins a ') + db.stops[nt.stop].name; });
  const head = [...hcount.entries()].sort((x, y) => y[1] - x[1])[0];
  let hMin = 99, hMax = -1;
  cols.forEach((c) => {
    c.hours = new Map();
    c.deps.forEach((x) => {
      const h = Math.floor(x.time / 3600), m = Math.floor(x.time % 3600 / 60);
      if (!c.hours.has(h)) c.hours.set(h, []);
      c.hours.get(h).push({ m, note: letterOf.get(x.last) || '' });
      if (h < hMin) hMin = h; if (h > hMax) hMax = h;
    });
  });
  return {
    stop: db.stops[stop], route: db.routes[route], empty: !all.length,
    dest: head ? head[0] : (main !== undefined ? db.stops[main].name : grp.dest || ''),
    cols, notes, strip, hMin, hMax, generated: new Date()
  };
};

/* ───────────── PDF ───────────── */
const NAVY = [16, 42, 84], GREY = [208, 208, 208];
const pdfText = (s) => String(s).replace(/[‘’]/g, "'").replace(/[“”]/g, '"').replace(/[–—]/g, '-')
  .replace(/…/g, '...').replace(/ŀ/g, 'l·').replace(/Ŀ/g, 'L·').replace(/[^\x00-\xff]/g, '?');
const hex = (h, def) => (/^[0-9a-f]{6}$/i.test(h) ? [parseInt(h.slice(0, 2), 16), parseInt(h.slice(2, 4), 16), parseInt(h.slice(4, 6), 16)] : def);
C.routeColors = function (r) {
  const bg = hex(r.color, NAVY);
  const lum = (0.299 * bg[0] + 0.587 * bg[1] + 0.114 * bg[2]) / 255;
  let fg = hex(r.text, null);
  if (!fg || Math.abs((0.299 * fg[0] + 0.587 * fg[1] + 0.114 * fg[2]) / 255 - lum) < 0.35) fg = lum > 0.6 ? [0, 0, 0] : [255, 255, 255];
  return { bg, fg };
};
const fmtDate = (d) => String(d.getDate()).padStart(2, '0') + '/' + String(d.getMonth() + 1).padStart(2, '0') + '/' + d.getFullYear();
C.fmtDate = fmtDate;

function fit(doc, s, maxW) {
  if (doc.getTextWidth(s) <= maxW) return s;
  while (s.length > 2 && doc.getTextWidth(s + '...') > maxW) s = s.slice(0, -1);
  return s.trimEnd() + '...';
}
function clock(doc, x, y, r) {
  doc.setDrawColor(255); doc.setLineWidth(0.9);
  doc.circle(x, y, r, 'S'); doc.line(x, y, x, y - r * 0.6); doc.line(x, y, x + r * 0.45, y + r * 0.3);
}

C.drawTimetable = function (doc, tt) {
  const PW = 595.28, PH = 841.89, L = 24, R = PW - 24, W = R - L;
  const col = C.routeColors(tt.route);
  // ── capçalera
  doc.setTextColor(120); doc.setFont('helvetica', 'bold'); doc.setFontSize(7);
  doc.text('PARADA', L, 34);
  doc.setTextColor(...NAVY); doc.setFontSize(13);
  const nameLines = doc.splitTextToSize(pdfText(tt.stop.name), 190).slice(0, 2);
  doc.text(nameLines, L, 49);
  doc.setFont('helvetica', 'normal'); doc.setFontSize(8.5); doc.setTextColor(90);
  if (tt.stop.code) doc.text('Codi ' + pdfText(tt.stop.code), L, 49 + nameLines.length * 14);
  doc.setDrawColor(190); doc.setLineWidth(0.6); doc.line(L + 200, 26, L + 200, 80);

  const bx = L + 214, bs = 40, by = 32;
  const label = pdfText(tt.route.short || tt.route.long || '?');
  doc.setFont('helvetica', 'bold');
  let fs = 26; doc.setFontSize(fs);
  const bw = Math.max(bs, Math.min(90, doc.getTextWidth(label) + 14));
  while (doc.getTextWidth(label) > bw - 8 && fs > 8) doc.setFontSize(--fs);
  doc.setFillColor(...col.bg); doc.rect(bx, by, bw, bs, 'F');
  doc.setTextColor(...col.fg); doc.text(label, bx + bw / 2, by + bs / 2 + fs * 0.35, { align: 'center' });
  doc.setTextColor(...NAVY);
  const dest = pdfText(tt.dest || '');
  fs = 26; doc.setFontSize(fs);
  const dw = R - (bx + bw + 10);
  while (doc.getTextWidth(dest) > dw && fs > 13) doc.setFontSize(--fs);
  doc.text(fit(doc, dest, dw), bx + bw + 10, by + bs / 2 + fs * 0.35);

  // ── tira de parades
  let y = 96;
  const st = tt.strip, n = st.length;
  if (n > 1) {
    const K = Math.SQRT1_2, fsz = n > 50 ? 5 : n > 38 ? 6 : n > 26 ? 7 : 8, maxLab = 170;
    const labs = st.map((s, i) => {
      doc.setFont('helvetica', i === 0 || i === n - 1 ? 'bold' : 'normal'); doc.setFontSize(fsz);
      const t = fit(doc, pdfText(s.name), maxLab);
      return { t, w: doc.getTextWidth(t) };
    });
    const x0 = L + 8, avail = R - x0;
    let dx = 24;
    for (let i = 1; i < n; i++) dx = Math.min(dx, (avail - labs[i].w * K - fsz) / i);
    dx = Math.max(dx, 4);
    const hTop = Math.max(...labs.map((l) => l.w)) * K + fsz;
    const ly = y + hTop + 4;
    doc.setDrawColor(...NAVY); doc.setLineWidth(1.4); doc.line(x0, ly, x0 + dx * (n - 1), ly);
    doc.setFillColor(...NAVY);
    for (let i = 0; i < n; i++) {
      const x = x0 + dx * i;
      doc.circle(x, ly, Math.min(2.6, dx * 0.32), 'F');
      doc.setFont('helvetica', i === 0 || i === n - 1 ? 'bold' : 'normal'); doc.setFontSize(fsz); doc.setTextColor(20);
      doc.text(labs[i].t, x + fsz * 0.3, ly - 5, { angle: 45 });
      if (i > 0) {
        doc.setFont('helvetica', 'normal'); doc.setFontSize(Math.min(6.5, Math.max(4, dx * 0.62))); doc.setTextColor(40);
        doc.text(String(st[i].min), x, ly + 11, { align: 'center' });
      }
    }
    doc.triangle(x0 - 3, ly + 5.5, x0 - 3, ly + 12.5, x0 + 3, ly + 9, 'F');
    y = ly + 16;
  }
  doc.setFont('helvetica', 'normal'); doc.setFontSize(7.5); doc.setTextColor(40);
  doc.text('Minuts de trajecte des d\'aquesta parada. Horari teòric, sense garantia.', R, y + 6, { align: 'right' });
  y += 10;

  // ── graella
  const HW = 26, avail = W - 2 * HW, hdrH = 20;
  const hours = [];
  for (let h = tt.hMin; h <= tt.hMax; h++) hours.push(h);
  const maxC = tt.cols.map((c) => Math.max(3, ...[...c.hours.values()].map((a) => a.length)));
  const per = maxC.slice();
  while (avail / (per[0] + per[1] + per[2]) < 15.5) {
    let k = 0; if (per[1] > per[k]) k = 1; if (per[2] > per[k]) k = 2;
    if (per[k] <= 3) break;
    per[k]--;
  }
  const slot = avail / (per[0] + per[1] + per[2]);
  const cx = [L + HW]; for (let k = 0; k < 3; k++) cx.push(cx[k] + per[k] * slot);
  const lines = hours.map((h) => Math.max(1, ...tt.cols.map((c, k) => Math.ceil((c.hours.get(h) || []).length / per[k]))));
  const totalLines = lines.reduce((a, b) => a + b, 0);
  const legendH = tt.notes.length ? 14 * Math.ceil(tt.notes.length / 2) + 8 : 0;
  const bottom = PH - 46 - legendH;
  const unit = Math.max(8, Math.min(19.5, (bottom - y - hdrH) / Math.max(1, totalLines)));
  const mfs = Math.min(10.5, unit * 0.6, slot * 0.62), hfs = Math.min(11.5, unit * 0.66);

  doc.setFillColor(...NAVY); doc.rect(L, y, W, hdrH, 'F');
  clock(doc, L + HW / 2, y + hdrH / 2, 5.2); clock(doc, R - HW / 2, y + hdrH / 2, 5.2);
  doc.setTextColor(255); doc.setFont('helvetica', 'bold');
  tt.cols.forEach((c, k) => {
    const w = cx[k + 1] - cx[k];
    let fz = 12.5; doc.setFontSize(fz);
    let lab = c.label;
    if (doc.getTextWidth(lab) > w - 6) lab = c.short;
    while (doc.getTextWidth(lab) > w - 4 && fz > 6) doc.setFontSize(--fz);
    doc.text(lab, cx[k] + w / 2, y + hdrH / 2 + fz * 0.35, { align: 'center' });
  });
  y += hdrH;
  const top = y;
  hours.forEach((h, i) => {
    const rh = lines[i] * unit;
    if (i % 2 === 1) { doc.setFillColor(...GREY); doc.rect(L, y, W, rh, 'F'); }
    doc.setTextColor(0); doc.setFont('helvetica', 'bold'); doc.setFontSize(hfs);
    const base = y + unit / 2 + hfs * 0.36;
    doc.text(String(h % 24), L + HW - 3, base, { align: 'right' });
    doc.text(String(h % 24), R - 3, base, { align: 'right' });
    tt.cols.forEach((c, k) => {
      const arr = c.hours.get(h) || [];
      arr.forEach((e, j) => {
        const x = cx[k] + 3 + (j % per[k]) * slot, yy = y + Math.floor(j / per[k]) * unit + unit / 2 + mfs * 0.36;
        doc.setFont('helvetica', 'normal'); doc.setFontSize(mfs); doc.setTextColor(0);
        const s = String(e.m).padStart(2, '0');
        doc.text(s, x, yy);
        if (e.note) { const w = doc.getTextWidth(s); doc.setFont('helvetica', 'bold'); doc.setFontSize(mfs * 0.62); doc.text(e.note, x + w + 0.6, yy + mfs * 0.18); }
      });
    });
    y += rh;
  });
  doc.setDrawColor(...NAVY); doc.setLineWidth(0.8);
  for (let k = 0; k <= 3; k++) doc.line(cx[k], top, cx[k], y);
  doc.setLineWidth(0.5); doc.line(L, y, R, y);
  if (tt.empty) {
    doc.setFont('helvetica', 'normal'); doc.setFontSize(10); doc.setTextColor(90);
    doc.text('Sense sortides previstes en les dates consultades.', PW / 2, top + 30, { align: 'center' });
  }

  // ── notes
  y += 14;
  tt.notes.forEach((nt, i) => {
    const x = L + (i % 2) * (W / 2), yy = y + Math.floor(i / 2) * 14;
    doc.setFont('helvetica', 'bold'); doc.setFontSize(9.5); doc.setTextColor(0);
    doc.text(nt.letter, x, yy);
    doc.setFont('helvetica', 'normal');
    doc.text(fit(doc, ' = ' + pdfText(nt.text), W / 2 - 16), x + doc.getTextWidth(nt.letter), yy);
  });

  // ── peu
  doc.setFillColor(226, 231, 240); doc.rect(L, PH - 40, W, 20, 'F');
  doc.setFont('helvetica', 'normal'); doc.setFontSize(7); doc.setTextColor(...NAVY);
  const g = tt.generated;
  doc.text('Dates consultades: ' + tt.cols.map((c) => fmtDate(c.date)).join(', ') + '.  Font: dades obertes GTFS.', L + 6, PH - 27.5);
  doc.text('Generat el ' + fmtDate(g) + ' ' + String(g.getHours()).padStart(2, '0') + ':' + String(g.getMinutes()).padStart(2, '0'), R - 6, PH - 27.5, { align: 'right' });
  doc.setFillColor(...NAVY); doc.rect(0, PH - 10, PW, 10, 'F');
};

C.makePdf = function (JsPDF, db, items, ref) {
  const doc = new JsPDF({ unit: 'pt', format: 'a4', compress: true });
  items.forEach((it, i) => {
    if (i) doc.addPage();
    C.drawTimetable(doc, C.buildTimetable(db, it, ref));
  });
  return doc;
};

if (typeof module !== 'undefined' && module.exports) module.exports = C;
root.AMB = C;
})(typeof self !== 'undefined' ? self : globalThis);
