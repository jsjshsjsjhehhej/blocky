import { readFileSync } from "node:fs";
const dump = JSON.parse(readFileSync(process.argv[2], "utf8"));
const seen = new Map();
for (const c of dump.Classes) for (const m of c.Members) {
  if (m.MemberType !== "Property") continue;
  const k = m.ValueType.Category + ":" + m.ValueType.Name;
  if (!seen.has(k)) seen.set(k, { n: 0, ex: [] });
  const s = seen.get(k); s.n++;
  if (s.ex.length < 2 && m.Default !== undefined) s.ex.push(String(m.Default).slice(0, 50));
}
for (const [k, v] of [...seen].sort((a, b) => b[1].n - a[1].n).slice(0, 40)) console.log(v.n, k, JSON.stringify(v.ex));
