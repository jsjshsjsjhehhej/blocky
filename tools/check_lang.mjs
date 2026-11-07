// Controlla i file assets/lang/<lingua>.json: JSON valido, chiavi che
// esistono davvero in ui/Loc.h, segnaposto (%s, %d...) uguali all'inglese,
// e quanta parte dell'interfaccia e' tradotta.
//   node tools/check_lang.mjs
import { execFileSync } from "node:child_process";
import { readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const base = JSON.parse(execFileSync(process.execPath, [join(root, "tools/dump_loc_keys.mjs")]).toString());
const keys = Object.keys(base);
const placeholders = (s) => (s.match(/%[-+ #0]*\d*(?:\.\d+)?[sdifuxXc%]/g) || []).sort().join(",");
let bad = 0;
for (const f of readdirSync(join(root, "assets/lang")).filter((n) => /^[a-z]{2,3}\.json$/.test(n)).sort()) {
  let pack;
  try {
    pack = JSON.parse(readFileSync(join(root, "assets/lang", f), "utf8"));
  } catch (e) {
    console.log(`${f}: JSON NON valido: ${e.message}`);
    ++bad;
    continue;
  }
  const unknown = Object.keys(pack).filter((k) => !k.startsWith("_") && !(k in base));
  const wrongFmt = Object.keys(pack).filter((k) => k in base && placeholders(pack[k]) !== placeholders(base[k][1]));
  const done = keys.filter((k) => k in pack).length;
  const ribbon = keys.filter((k) => k.startsWith("rb.")), ribbonDone = ribbon.filter((k) => k in pack).length;
  console.log(`${f.padEnd(8)} ${String(pack._name).padEnd(16)} ${done}/${keys.length} frasi (barra ${ribbonDone}/${ribbon.length})` +
    (unknown.length ? `  CHIAVI SCONOSCIUTE: ${unknown.join(", ")}` : "") +
    (wrongFmt.length ? `  SEGNAPOSTO DIVERSI: ${wrongFmt.join(", ")}` : ""));
  if (!pack._name || unknown.length || wrongFmt.length) ++bad;
}
process.exit(bad ? 1 : 0);
