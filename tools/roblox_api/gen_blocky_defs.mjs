// Tipi e guida per scrivere giochi Blocky con VS Code (luau-lsp) e con un'AI.
//
//   node tools/roblox_api/gen_blocky_defs.mjs
//
// luau-lsp conosce gia' l'API di Roblox (piattaforma "roblox"). Qui si aggiunge
// quello che Blocky ha IN PIU': le globali del motore (Backpack, Camera,
// Humanoid, Events...) con le loro funzioni, lette da registerApi in
// src/engine/LuaRuntime.h. Escono:
//   tools/blocky_luau/blocky.d.luau     definizioni per luau-lsp
//   src/engine/BlockyLuauDefs.h         le stesse, dentro il motore (export Rojo)
//   docs/BLOCKY_PER_AI.md               guida per chi scrive codice (persone o AI)
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "../..");
const runtime = readFileSync(join(root, "src/engine/LuaRuntime.h"), "utf8").split(/\r?\n/);

// Globali di Luau / Roblox che luau-lsp conosce gia': non si ridichiarano.
const standard = new Set(["game", "workspace", "script", "Instance", "Vector3", "Vector2", "CFrame", "Color3", "BrickColor",
  "UDim", "UDim2", "Enum", "Ray", "Region3", "TweenInfo", "NumberRange", "NumberSequence", "ColorSequence", "Rect",
  "PhysicalProperties", "DateTime", "task", "wait", "delay", "spawn", "tick", "time", "typeof", "require", "print", "warn",
  "error", "os", "math", "string", "table", "coroutine", "debug", "utf8", "bit32", "buffer", "shared", "_G", "settings",
  "UserSettings", "version", "elapsedTime", "Random", "collectgarbage", "dofile", "load", "loadfile", "getmetatable",
  "setmetatable", "rawset", "rawget", "pairs", "ipairs", "select", "tostring", "tonumber", "type", "pcall", "xpcall",
  "assert", "next", "unpack"]);

// Mini-lettore di registerApi: segue lua_newtable / setfield / setglobal.
const start = runtime.findIndex((l) => /bool registerApi\(\)/.test(l));
const stack = [];
let last = null;   // "function" | "value" | "table"
const globals = new Map();
for (let i = start; i >= 0 && i < runtime.length; i++) {
  const l = runtime[i];
  if (i > start && /^    bool [a-zA-Z]+\(/.test(l)) break;   // fine della funzione
  if (/lua_newtable\(L\)/.test(l)) { stack.push({ fields: [] }); last = "table"; continue; }
  if (/blockyPushCFunction\(L,|pushSignalMethod\(L,/.test(l)) { last = "function"; continue; }
  if (/\bpush[A-Z]\w*\(L\b/.test(l)) { last = "value"; continue; }   // pushVec3, pushServiceContainer...
  if (/lua_getglobal\(L,|lua_getfield\(L,|lua_pushvalue\(L,/.test(l)) { last = "value"; continue; }
  if (/lua_push(string|number|integer|boolean|lstring|nil)\(L/.test(l)) { last = "value"; continue; }
  let m = l.match(/lua_setfield\(L, -2, "([A-Za-z_][A-Za-z0-9_]*)"\)/);
  if (m) {
    if (last === "table" && stack.length >= 2) {
      const child = stack.pop();
      stack[stack.length - 1].fields.push({ name: m[1], kind: "table", fields: child.fields });
    } else if (stack.length >= 1) {
      stack[stack.length - 1].fields.push({ name: m[1], kind: last === "function" ? "function" : "value" });
    }
    last = "table";
    continue;
  }
  if (/lua_setmetatable\(L, -2\)/.test(l)) { if (stack.length) stack.pop(); last = "table"; continue; }
  m = l.match(/lua_setglobal\(L, "([A-Za-z_][A-Za-z0-9_]*)"\)/);
  if (m) {
    const name = m[1];
    if (last === "table" && stack.length) globals.set(name, { kind: "table", fields: stack.pop().fields });
    else globals.set(name, { kind: last === "function" ? "function" : "value" });
    last = null;
  }
}

const blocky = [...globals].filter(([n]) => !standard.has(n) && !n.startsWith("__"));
const typeOf = (f, indent) => {
  if (f.kind === "function") return "(...any) -> ...any";
  if (f.kind === "table" && f.fields && f.fields.length) {
    const inner = f.fields.filter((x, i, a) => a.findIndex((y) => y.name === x.name) === i)
      .map((x) => `${indent}    ${x.name}: ${typeOf(x, indent + "    ")},`).join("\n");
    return `{\n${inner}\n${indent}}`;
  }
  return "any";
};
const defs = [
  "-- Generato da tools/roblox_api/gen_blocky_defs.mjs: NON modificare a mano.",
  "-- Quello che Blocky ha in PIU' rispetto a Roblox (l'API di Roblox luau-lsp la conosce gia').",
  "-- In VS Code: luau-lsp.types.definitionFiles = [\"blocky.d.luau\"].",
  "",
  ...blocky.map(([n, g]) => `declare ${n}: ${typeOf(g, "")}\n`),
].join("\n");

mkdirSync(join(root, "tools/blocky_luau"), { recursive: true });
writeFileSync(join(root, "tools/blocky_luau/blocky.d.luau"), defs);
writeFileSync(join(root, "src/engine/BlockyLuauDefs.h"), [
  "#pragma once",
  "// Generato da tools/roblox_api/gen_blocky_defs.mjs: NON modificare a mano.",
  "// Definizioni luau-lsp delle API proprie di Blocky (le scrive l'export Rojo).",
  "#include <string>",
  "namespace blk {",
  "inline const std::string& blockyLuauDefs() {",
  `    static const std::string s = R"DEFS(${defs})DEFS";`,
  "    return s;",
  "}",
  "} // namespace blk",
  "",
].join("\n"));

// Guida per chi scrive codice.
const list = blocky.map(([n, g]) => {
  const fns = (g.fields || []).filter((f) => f.kind === "function").map((f) => "`" + f.name + "`");
  return `- **${n}**` + (fns.length ? `: ${fns.join(", ")}` : "");
}).join("\n");
const guide = `# Scrivere giochi per Blocky (guida per persone e AI)

Generata da \`tools/roblox_api/gen_blocky_defs.mjs\`. Blocky esegue **Luau** con l'API di
Roblox: si scrive come su Roblox. Questa pagina dice cosa c'e', cosa no, e cosa ha in piu'.

## Regole
1. Scrivi come su Roblox (\`game:GetService\`, \`Instance.new\`, \`:Connect\`, \`task.wait\`...).
2. Prima di usare un'API poco comune guarda \`docs/api_coverage.md\`:
   - **funzionante** = fa davvero la cosa;
   - **presente** = esiste e non fa crashare lo script, ma non fa ancora niente
     (valore iniziale di Roblox; eventi che non scattano; i metodi danno l'errore
     "non e' ancora in Blocky").
3. Non usare \`loadstring\`, \`setfenv\`, \`getfenv\`: sono bloccati (sandbox).
4. Gli Enum hanno i numeri di Roblox (\`Enum.KeyCode.E\`, \`Enum.EasingStyle.Quad\`...).

## Progetti Rojo
Lo Studio legge i progetti Rojo (menu Script -> Rojo): \`default.project.json\` con
\`tree\` / \`$path\` / \`$className\`.
- \`nome.server.luau\` -> Script, \`nome.client.luau\` -> LocalScript, \`nome.luau\` -> ModuleScript
- cartella con \`init.server.luau\` / \`init.client.luau\` / \`init.luau\` -> la cartella e' quello script
- \`nome.json\` -> ModuleScript che restituisce i dati
- servizi: ReplicatedStorage, ServerStorage, ServerScriptService, StarterPlayer.StarterPlayerScripts, StarterGui
- non ancora: modelli (\`.rbxm\`, \`.model.json\`), Workspace con parti, Lighting
I file si sincronizzano da soli ogni mezzo secondo mentre lo Studio e' aperto.

## API in piu' di Blocky (globali)
${list}

Tipi per VS Code: \`tools/blocky_luau/blocky.d.luau\` (luau-lsp, piattaforma "roblox").
`;
writeFileSync(join(root, "docs/BLOCKY_PER_AI.md"), guide);
console.log("globali di Blocky:", blocky.map(([n]) => n).join(", "));
