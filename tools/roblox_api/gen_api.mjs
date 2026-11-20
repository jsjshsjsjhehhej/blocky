// Strato "API presente" per il Luau di Blocky, dall'API dump di Roblox.
//
//   node tools/roblox_api/gen_api.mjs
//
// Esce src/engine/RobloxApiData.h: per ogni classe la classe madre, le
// proprieta' con tipo e valore iniziale, i metodi e gli eventi usabili dagli
// script. Il prelude (LuaPrelude.h) lo usa per le classi che il motore non
// implementa da se': proprieta' leggibili/scrivibili con il valore giusto,
// eventi collegabili, metodi che spiegano chiaramente che non ci sono ancora.
// E' "presente", non "funzionante": il comportamento vero si scrive a parte.
//
// Formato, una riga per classe:
//   Classe<Madre|nome=tipo:valore;...|metodo,metodo|evento,evento|S (S = servizio)
// tipi: b bool, n numero, s testo, v3 Vector3, v2 Vector2, c3 Color3, cf CFrame,
//       u UDim, u2 UDim2, nr NumberRange, e Enum (valore = nome della voce)
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const dump = JSON.parse(readFileSync(join(here, "../roblox_api_dump/Full-API-Dump.json"), "utf8"));
const version = readFileSync(join(here, "../roblox_api_dump/version.txt"), "utf8").trim();
const tags = (x) => (x.Tags || []).map((t) => (typeof t === "string" ? t : Object.keys(t)[0]));
const scriptable = (m) => {
  const t = tags(m);
  if (t.includes("Deprecated") || t.includes("NotScriptable") || t.includes("Hidden")) return false;
  const sec = m.Security;
  const r = typeof sec === "string" ? sec : sec?.Read;
  return r === "None";
};
const placeholder = (d) => d === undefined || d === null || String(d).startsWith("__api_dump_");
const nums = (s) => String(s).replace(/[{}]/g, " ").split(/[\s,]+/).filter((x) => x !== "").map(Number);
const okName = (s) => /^[A-Za-z_][A-Za-z0-9_]*$/.test(s);
// Gestite dal prelude per ogni Instance: non si toccano.
const skipProps = new Set(["Name", "ClassName", "Parent", "Archivable"]);

const enumItems = new Map(dump.Enums.map((e) => [e.Name, e.Items.map((i) => i.Name)]));

function encode(vt, def) {
  const ph = placeholder(def);
  const n = vt.Name;
  if (vt.Category === "Class") return null;   // riferimento: nil finche' non lo imposti
  if (vt.Category === "Enum") {
    const items = enumItems.get(n) || [];
    let item = ph ? items[0] : String(def);
    if (!item || !okName(item) || !okName(n)) return null;
    return `e:${n}.${item}`;
  }
  switch (n) {
    case "bool": return `b:${!ph && String(def) === "true" ? 1 : 0}`;
    case "float": case "double": case "int": case "int64": {
      const v = ph ? 0 : Number(def);
      return Number.isFinite(v) ? `n:${v}` : "n:0";
    }
    case "string": case "Content": case "ContentId": {
      const v = ph ? "" : String(def);
      if (/[|;\n\r]/.test(v)) return "s:";
      return `s:${v}`;
    }
    case "Vector3": { const a = ph ? [0, 0, 0] : nums(def); return a.length >= 3 && a.every(Number.isFinite) ? `v3:${a.slice(0, 3).join(",")}` : "v3:0,0,0"; }
    case "Vector2": { const a = ph ? [0, 0] : nums(def); return a.length >= 2 && a.every(Number.isFinite) ? `v2:${a.slice(0, 2).join(",")}` : "v2:0,0"; }
    case "Color3": { const a = ph ? [0, 0, 0] : nums(def); return a.length >= 3 && a.every(Number.isFinite) ? `c3:${a.slice(0, 3).join(",")}` : "c3:0,0,0"; }
    case "CFrame": { const a = ph ? [] : nums(def); return a.length >= 3 && a.every(Number.isFinite) ? `cf:${a.slice(0, 12).join(",")}` : "cf:0,0,0"; }
    case "UDim": { const a = ph ? [0, 0] : nums(def); return a.length >= 2 ? `u:${a.slice(0, 2).join(",")}` : "u:0,0"; }
    case "UDim2": { const a = ph ? [0, 0, 0, 0] : nums(def); return a.length >= 4 ? `u2:${a.slice(0, 4).join(",")}` : "u2:0,0,0,0"; }
    case "NumberRange": { const a = ph ? [0, 0] : nums(def); return a.length >= 2 ? `nr:${a.slice(0, 2).join(",")}` : null; }
    default: return null;   // tipi che Blocky non ha ancora (Font, NumberSequence...): non si inventano
  }
}

const rows = [];
let np = 0, nf = 0, ne = 0;
for (const c of dump.Classes) {
  if (tags(c).includes("Deprecated") || !okName(c.Name)) continue;
  const props = [], funcs = [], events = [];
  for (const m of c.Members) {
    if (!scriptable(m) || !okName(m.Name)) continue;
    if (m.MemberType === "Property") {
      if (skipProps.has(m.Name)) continue;
      const enc = encode(m.ValueType, m.Default);
      if (enc) { props.push(`${m.Name}=${enc}`); np++; }
    } else if (m.MemberType === "Function") { funcs.push(m.Name); nf++; }
    else if (m.MemberType === "Event") { events.push(m.Name); ne++; }
  }
  const sup = c.Superclass && c.Superclass !== "<<<ROOT>>>" ? c.Superclass : "";
  const svc = tags(c).includes("Service") ? "S" : "";
  rows.push(`${c.Name}<${sup}|${props.join(";")}|${funcs.join(",")}|${events.join(",")}|${svc}`);
}
const chunks = [];
let cur = "";
for (const r of rows) {
  if (cur.length + r.length > 12000) { chunks.push(cur); cur = ""; }
  cur += r + "\n";
}
if (cur) chunks.push(cur);
const out = [
  "#pragma once",
  "",
  "// Generato da tools/roblox_api/gen_api.mjs: NON modificare a mano.",
  `// API di Roblox ${version}: ${rows.length} classi, ${np} proprieta', ${nf} metodi, ${ne} eventi`,
  "// (solo nomi, tipi e valori iniziali: l'interfaccia, nessun codice di Roblox).",
  "// Una riga per classe: Classe<Madre|nome=tipo:valore;...|metodi|eventi",
  "",
  "#include <string>",
  "",
  "namespace blk {",
  "inline const std::string& robloxApiData() {",
  "    static const std::string data = std::string()",
  ...chunks.map((c) => `        + R"API(\n${c})API"`),
  "        ;",
  "    return data;",
  "}",
  "} // namespace blk",
  "",
];
writeFileSync(join(here, "../../src/engine/RobloxApiData.h"), out.join("\n"));
console.log(`classi ${rows.length}, proprieta' ${np}, metodi ${nf}, eventi ${ne}, ${chunks.length} pezzi`);
