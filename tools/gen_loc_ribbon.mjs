// Copia le frasi della barra dello Studio (assets/lang/_ribbon_base.json,
// italiano + inglese) dentro ui/Loc.h, fra i marcatori RIBBON-BEGIN/END.
//   node tools/gen_loc_ribbon.mjs
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const base = JSON.parse(readFileSync(join(root, "assets/lang/_ribbon_base.json"), "utf8"));
const cstr = (s) => JSON.stringify(s);   // stringa C valida (UTF-8, \" e \\)
const lines = Object.entries(base)
  .filter(([k]) => !k.startsWith("_"))
  .map(([k, [it, en]]) => `            {${cstr(k)}, {${cstr(it)}, ${cstr(en)}}},`);

const locPath = join(root, "src/ui/Loc.h");
let s = readFileSync(locPath, "utf8");
const begin = "            // RIBBON-BEGIN (generato da tools/gen_loc_ribbon.mjs: non modificare a mano)\n";
const end = "            // RIBBON-END\n";
const block = begin + lines.join("\n") + "\n" + end;
if (s.includes(begin)) {
  const a = s.indexOf(begin), b = s.indexOf(end, a);
  s = s.slice(0, a) + block + s.slice(b + end.length);
} else {
  const anchor = '            {"lang.restart",';
  const a = s.indexOf(anchor);
  if (a < 0) throw new Error("ancora lang.restart non trovata in Loc.h");
  const eol = s.indexOf("\n", a) + 1;
  s = s.slice(0, eol) + block + s.slice(eol);
}
writeFileSync(locPath, s);
console.log(`${lines.length} frasi della barra in ui/Loc.h`);
