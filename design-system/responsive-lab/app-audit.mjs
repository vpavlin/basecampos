// app-audit.mjs <W> <H> [rootObjectName]: walks the visible tree under the screen root;
// reports tappables < 44 and any visible item running past the right edge.
// The Logos QML inspector client (logos-qt-mcp test-framework); port from QML_INSPECTOR_PORT.
const FRAMEWORK = process.env.LOGOS_INSPECTOR_FRAMEWORK || "/extra/tmp/mmorpg/result-mcp/test-framework/framework.mjs";
const { Inspector, App } = await import(FRAMEWORK);
const insp = new Inspector(); const app = new App(insp);
const W = +process.argv[2], H = +process.argv[3], ROOT = process.argv[4] || "appManager.view";
const r = await app.findByProperty("objectName", ROOT);
const expr = `(function(){
  var W=${W}, H=${H}, over=[], small=[], seen=0;
  function vis(it){ for (var p=it; p; p=p.parent) { if (!p.visible || p.opacity===0) return false } return true }
  function tappable(it){
    if (typeof it.clicked === "function" && it.enabled !== false && it.interactive !== false) return true;
    var d = it.data || []; for (var i=0;i<d.length;i++){ var o=d[i]; if (o && typeof o.tapped === "function" && o.enabled && (o.acceptedButtons===undefined || (o.acceptedButtons & 1))) return true }
    return false }
  function name(it){ var s=(it.objectName||"") + "|" + String(it).split("(")[0]; if (it.text!==undefined) s += "|" + String(it.text).slice(0,20); return s }
  function walk(it, clipR){
    if (!it.visible || it.opacity===0) return;
    var p = it.mapToItem(null, 0, 0); var x=p.x, y=p.y, w=it.width, h=it.height;
    seen++;
    var inView = y < H && y+h > 0 && x < clipR;
    if (inView && w>0 && h>0) {
      if (x + w > W + 0.5) over.push([name(it), Math.round(x), Math.round(w)]);
      if (tappable(it) && String(it).indexOf("LogosText_") !== 0 && (w < 43.5 || h < 43.5)) small.push([name(it), Math.round(w), Math.round(h), Math.round(x), Math.round(y)]);
    }
    var c = it.children; var cr = clipR;
    if (it.clip) cr = Math.min(clipR, x + w);
    for (var i=0;i<c.length;i++) walk(c[i], cr);
  }
  var root=this; if (!vis(root)) return JSON.stringify({skip:"invisible root"});
  walk(root, 1e9);
  return JSON.stringify({seen:seen, overflow:over, smallTargets:small});
}).call(this)`;
const matches = r.matches || [];
if (!matches.length) console.log(JSON.stringify({ root: ROOT, error: "no object with this objectName" }));
let audited = 0;
for (const m of matches) {
  const e = await insp.send("evaluate", { objectId: m.id, expression: expr });
  let res = e.result; try { res = JSON.parse(res) } catch { res = {raw: e} }
  if (res && res.skip) continue;
  audited++;
  console.log(JSON.stringify({ width: W, root: ROOT, ...res }));
}
if (matches.length && !audited) console.log(JSON.stringify({ root: ROOT, error: `all ${matches.length} matches are invisible: pick a root that is on screen` }));
insp.disconnect();
