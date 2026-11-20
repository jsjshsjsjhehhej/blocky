// Tutti gli Enum di Roblox (nomi e numeri, dall'API dump) per il Luau di Blocky.
//
//   node tools/roblox_api/gen_enums.mjs
//
// Esce src/engine/RobloxEnumsData.h: una stringa compatta, una riga per Enum
//   Nome:Voce=Numero,Voce=Numero,...
// Il prelude (LuaPrelude.h) la legge solo quando uno script usa quell'Enum:
// niente costo all'avvio. Gli Enum che il motore ha gia' (Material,
// UserInputType, ...) restano i suoi: le funzioni del motore li aspettano cosi'.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const dump = JSON.parse(readFileSync(join(here, "../roblox_api_dump/Full-API-Dump.json"), "utf8"));
const version = readFileSync(join(here, "../roblox_api_dump/version.txt"), "utf8").trim();
const tags = (x) => (x.Tags || []).map((t) => (typeof t === "string" ? t : Object.keys(t)[0]));
const ok = (s) => /^[A-Za-z_][A-Za-z0-9_]*$/.test(s);

const rows = [];
let items = 0;
for (const e of dump.Enums) {
  if (tags(e).includes("Deprecated") || !ok(e.Name)) continue;
  const parts = [];
  for (const it of e.Items) {
    if (tags(it).includes("Deprecated") || !ok(it.Name)) continue;
    parts.push(`${it.Name}=${it.Value}`);
    items++;
  }
  if (parts.length) rows.push(`${e.Name}:${parts.join(",")}`);
}
// Spezzata in pezzi: alcuni compilatori non amano stringhe letterali enormi.
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
  "// Generato da tools/roblox_api/gen_enums.mjs: NON modificare a mano.",
  `// Enum di Roblox ${version}: ${rows.length} Enum, ${items} voci (solo nomi e numeri).`,
  "// Una riga per Enum: Nome:Voce=Numero,Voce=Numero,...",
  "",
  "#include <string>",
  "",
  "namespace blk {",
  "inline const std::string& robloxEnumsData() {",
  "    static const std::string data = std::string()",
  ...chunks.map((c) => `        + R"ENUMS(\n${c})ENUMS"`),
  "        ;",
  "    return data;",
  "}",
  "} // namespace blk",
  "",
];
writeFileSync(join(here, "../../src/engine/RobloxEnumsData.h"), out.join("\n"));
console.log(`Enum: ${rows.length}, voci: ${items}, ${chunks.length} pezzi`);
