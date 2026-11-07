// Elenca le frasi di ui/Loc.h (chiave, italiano, inglese) in un JSON: e' la
// base da cui si traducono i file assets/lang/<lingua>.json.
//   node tools/dump_loc_keys.mjs > fuori.json
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const src = readFileSync(join(root, "src/ui/Loc.h"), "utf8");
const a = src.indexOf("static const std::unordered_map<std::string, LocEntry>& table()");
const b = src.indexOf("return t;", a);
const body = src.slice(a, b);

// Una voce: {"chiave", {"it" ["it2"...], "en" ["en2"...]}} con stringhe C adiacenti.
const out = {};
let i = 0;
const str = () => {   // legge una stringa C da body[i] == '"'
  let s = "";
  i++;
  while (i < body.length && body[i] !== '"') {
    if (body[i] === "\\") { s += JSON.parse('"' + body[i] + body[i + 1] + '"'); i += 2; continue; }
    s += body[i++];
  }
  i++;
  return s;
};
const ws = () => { while (i < body.length && /\s/.test(body[i])) i++; };
const strs = () => { let s = ""; ws(); while (body[i] === '"') { s += str(); ws(); } return s; };
while ((i = body.indexOf('{"', i)) >= 0) {
  i++;
  const key = str();
  ws();
  if (body[i] !== ",") continue;
  i++; ws();
  if (body[i] !== "{") continue;
  i++;
  const it = strs();
  if (body[i] !== ",") continue;
  i++;
  const en = strs();
  out[key] = [it, en];
}
process.stdout.write(JSON.stringify(out, null, 1));
