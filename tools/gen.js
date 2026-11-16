// node gen.js <google_transit.zip>  →  amb.dat + afm.h
const fs = require('fs'), C = require('./core.js'), { jsPDF } = require('jspdf');
const fold = (s) => String(s).toLowerCase().replace(/[àáâä]/g, 'a').replace(/[èéêë]/g, 'e').replace(/[ìíîï]/g, 'i').replace(/[òóôö]/g, 'o').replace(/[ùúûü]/g, 'u').replace(/ç/g, 'c').replace(/ñ/g, 'n').replace(/[·.]/g, '');
const nat = (a, b) => String(a).localeCompare(String(b), 'ca', { numeric: true, sensitivity: 'base' });
(async () => {
  const db = await C.importGtfs(await fs.openAsBlob(process.argv[2]), () => {});
  const n = db.sStop.length, nT = db.tripIds.length, nS = db.stops.length, nR = db.routes.length;
  const tT0 = new Int32Array(nT), sDelta = new Uint16Array(n);
  for (let t = 0; t < nT; t++) { const a = db.tStart[t]; if (a < 0) continue; tT0[t] = db.sTime[a];
    for (let i = a + 1; i < a + db.tLen[t]; i++) { const d = db.sTime[i] - db.sTime[i - 1]; if (d < 0 || d > 65535) throw new Error('delta ' + d); sDelta[i] = d; } }
  const order = db.routes.map((r, i) => i).sort((a, b) => nat(db.routes[a].short, db.routes[b].short));
  const rank = new Int32Array(nR); order.forEach((r, k) => { rank[r] = k; });
  const srStart = new Int32Array(nS + 1), srList = [];
  db.stops.forEach((s, i) => { s.routes.slice().sort((a, b) => rank[a] - rank[b]).forEach((r) => srList.push(r)); srStart[i + 1] = srList.length; });
  const strs = [];
  db.routes.forEach((r) => strs.push(r.short, r.long, r.color));
  db.stops.forEach((s) => strs.push(s.code, s.name, fold(s.name + ' ' + s.code)));
  db.heads.forEach((h) => strs.push(h));
  const sbuf = Buffer.from(strs.map((s) => String(s).replace(/\0/g, '')).join('\0') + '\0', 'utf8');
  const parts = [];
  const push = (ta) => { const b = Buffer.from(ta.buffer, ta.byteOffset, ta.byteLength); parts.push(b); const pad = (4 - b.length % 4) % 4; if (pad) parts.push(Buffer.alloc(pad)); };
  const hdr = new Int32Array([0x31424d41, nR, nS, db.svcDays.length, db.ex.length / 3, nT, n, db.heads.length, srList.length, sbuf.length, Math.floor(Date.now() / 1000), 0]);
  push(hdr);
  push(db.svcDays); push(db.svcStart); push(db.svcEnd); push(db.ex);
  push(Uint16Array.from(db.tRoute)); push(Uint16Array.from(db.tSvc)); push(Uint8Array.from(db.tDir)); push(Uint16Array.from(db.tHead));
  push(db.tStart); push(Uint16Array.from(db.tLen)); push(tT0); push(Uint16Array.from(db.sStop)); push(sDelta);
  push(srStart); push(Uint16Array.from(srList)); push(Uint16Array.from(order)); push(new Uint8Array(sbuf));
  fs.writeFileSync('amb.dat', Buffer.concat(parts));
  // amplades Helvetica (WinAnsi 32..255), en mil·lèsimes d'em, tal com les calcula jsPDF
  const doc = new jsPDF({ unit: 'pt' }); let h = '';
  for (const [nm, st] of [['W_REG', 'normal'], ['W_BOLD', 'bold']]) {
    doc.setFont('helvetica', st); doc.setFontSize(1000);
    const w = []; for (let c = 32; c < 256; c++) w.push(Math.round(doc.getTextWidth(String.fromCharCode(c))));
    h += 'static const unsigned short ' + nm + '[224] = {' + w.join(',') + '};\n';
  }
  fs.writeFileSync('afm.h', h);
  console.log('amb.dat', fs.statSync('amb.dat').size, 'rows', n, 'trips', nT, 'stops', nS, 'routes', nR, 'svc', db.svcDays.length, 'heads', db.heads.length);
})().catch((e) => { console.error(e); process.exit(1); });
