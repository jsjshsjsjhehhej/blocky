// Dall'API dump di Roblox (tools/roblox_api_dump/Full-API-Dump.json) la lista
// di cosa provare dentro Blocky: per ogni classe le proprieta', i metodi e gli
// eventi usabili dagli script, e come ottenere un oggetto su cui provarli.
//
//   node tools/roblox_api/gen_probe.mjs
//
// Esce tools/roblox_api/api_probe_data.lua, letto da test_api_coverage.
// Solo nomi e tipi (l'interfaccia), nessun codice di Roblox.
// Le proprieta' che puntano a un altro oggetto hanno "&" davanti: possono
// valere nil anche quando ci sono, quindi non si verificano leggendole.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const dump = JSON.parse(readFileSync(join(here, "../roblox_api_dump/Full-API-Dump.json"), "utf8"));
const tags = (x) => (x.Tags || []).map((t) => (typeof t === "string" ? t : Object.keys(t)[0]));
const scriptable = (m) => {
  const t = tags(m);
  if (t.includes("Deprecated") || t.includes("NotScriptable") || t.includes("Hidden")) return false;
  const sec = m.Security;
  const r = typeof sec === "string" ? sec : sec?.Read;
  return r === "None";
};

const byName = new Map(dump.Classes.map((c) => [c.Name, c]));
const children = new Map();
for (const c of dump.Classes) {
  if (!children.has(c.Superclass)) children.set(c.Superclass, []);
  children.get(c.Superclass).push(c.Name);
}
const isCreatable = (c) => !tags(c).includes("NotCreatable") && !tags(c).includes("Service");
const isService = (c) => tags(c).includes("Service");

// Oggetti da cui partire per classi che non si creano con Instance.new.
// Forma: "special:<chiave>" | "service:<Classe>" | "new:<Classe>" (la sonda in Luau sa cosa fare).
const special = {
  DataModel: "special:game",
  Workspace: "special:workspace",
  Player: "special:localplayer",
  Camera: "special:camera",
  Terrain: "special:terrain",
};
// Per le classi astratte: il discendente piu' comune su cui provarle.
const prefer = { BasePart: "Part", FormFactorPart: "Part", PVInstance: "Part", GuiObject: "Frame", GuiBase2d: "Frame",
  GuiButton: "TextButton", GuiLabel: "TextLabel", LayerCollector: "ScreenGui", ValueBase: "IntValue",
  LuaSourceContainer: "Script", BaseScript: "Script", Constraint: "RopeConstraint", JointInstance: "Weld",
  Instance: "Folder", GuiBase3d: "SelectionBox", PartAdornment: "BoxHandleAdornment", BasePlayerGui: "PlayerGui",
  TriangleMeshPart: "MeshPart", BaseWrap: "WrapLayer", Light: "PointLight", PostEffect: "BlurEffect",
  BevelMesh: "BlockMesh", DataModelMesh: "SpecialMesh", FaceInstance: "Decal", Feature: "Hole",
  Controller: "ControllerPart", UIBase: "UIListLayout", UIComponent: "UIListLayout", UIConstraint: "UISizeConstraint",
  UIGridStyleLayout: "UIListLayout", UILayout: "UIListLayout", BodyMover: "BodyVelocity", SoundEffect: "ReverbSoundEffect",
  HandlesBase: "Handles", SelectionLasso: "SelectionPartLasso", PolicyService: null };

function probeFor(name, depth = 0) {
  const c = byName.get(name);
  if (!c || depth > 8) return null;
  if (special[name]) return special[name];
  if (isService(c)) return `service:${name}`;
  if (isCreatable(c)) return `new:${name}`;
  if (prefer[name] && byName.get(prefer[name]) && isCreatable(byName.get(prefer[name]))) return `new:${prefer[name]}`;
  for (const sub of (children.get(name) || []).sort()) {
    const p = probeFor(sub, depth + 1);
    if (p) return p;
  }
  return null;
}

const q = (s) => JSON.stringify(s);
const lines = ["-- Generato da tools/roblox_api/gen_probe.mjs: NON modificare a mano.",
  `-- API di Roblox ${readFileSync(join(here, "../roblox_api_dump/version.txt"), "utf8").trim()}: solo nomi (interfaccia).`,
  "return {", "  classes = {"];
let n = 0;
for (const c of dump.Classes) {
  const t = tags(c);
  if (t.includes("Deprecated")) continue;
  const props = [], funcs = [], events = [];
  for (const m of c.Members) {
    if (!scriptable(m)) continue;
    if (m.MemberType === "Property") props.push(m.ValueType && m.ValueType.Category === "Class" ? "&" + m.Name : m.Name);
    else if (m.MemberType === "Function") funcs.push(m.Name);
    else if (m.MemberType === "Event") events.push(m.Name);
  }
  if (!props.length && !funcs.length && !events.length) continue;
  const probe = probeFor(c.Name);
  // Dove lo mette uno script vero prima di usarlo: le Part nel workspace, la GUI in una ScreenGui.
  const probeCls = probe && probe.startsWith("new:") ? probe.slice(4) : null;
  const inherits = (name, base) => { for (let k = byName.get(name); k; k = byName.get(k.Superclass)) if (k.Name === base) return true; return false; };
  const place = !probeCls ? "nil" : inherits(probeCls, "BasePart") || inherits(probeCls, "Model") ? q("workspace")
    : inherits(probeCls, "GuiObject") ? q("gui") : "nil";
  lines.push(`    { name = ${q(c.Name)}, probe = ${probe ? q(probe) : "nil"}, place = ${place}, service = ${isService(c)}, ` +
    `props = {${props.map(q).join(", ")}}, funcs = {${funcs.map(q).join(", ")}}, events = {${events.map(q).join(", ")}} },`);
  n++;
}
lines.push("  },", "  enums = {");
for (const e of dump.Enums) {
  if (tags(e).includes("Deprecated")) continue;
  lines.push(`    ${q(e.Name)},`);
}
lines.push("  },", "}", "");
writeFileSync(join(here, "api_probe_data.lua"), lines.join("\n"));
console.log("classi con API per gli script:", n);
