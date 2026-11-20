// Quanto e' grande l'API di Roblox usabile dagli script (dal Full-API-Dump).
import { readFileSync } from "node:fs";
const dump = JSON.parse(readFileSync(process.argv[2], "utf8"));
const tags = (x) => (x.Tags || []).map((t) => (typeof t === "string" ? t : Object.keys(t)[0]));
const scriptable = (m) => {
  const t = tags(m);
  if (t.includes("Deprecated") || t.includes("NotScriptable") || t.includes("Hidden")) return false;
  const sec = m.Security;
  const r = typeof sec === "string" ? sec : sec?.Read;
  return r === "None";
};
let classes = 0, creatable = 0, services = 0, props = 0, funcs = 0, events = 0, cbs = 0;
for (const c of dump.Classes) {
  const t = tags(c);
  if (t.includes("Deprecated") || t.includes("NotBrowsable") && t.includes("NotCreatable") && !t.includes("Service")) continue;
  classes++;
  if (!t.includes("NotCreatable")) creatable++;
  if (t.includes("Service")) services++;
  for (const m of c.Members) {
    if (!scriptable(m)) continue;
    if (m.MemberType === "Property") props++;
    else if (m.MemberType === "Function") funcs++;
    else if (m.MemberType === "Event") events++;
    else if (m.MemberType === "Callback") cbs++;
  }
}
console.log({ classi: classes, creabili: creatable, servizi: services, proprieta: props, metodi: funcs, eventi: events, callback: cbs, enum: dump.Enums.length });
