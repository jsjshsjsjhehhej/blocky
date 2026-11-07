// Genera Laboratorio_Fisica.blocky dal file laboratorio_fisica.lua.
//   node places/genera_laboratorio.mjs
// Il place ha solo la Baseplate e una part invisibile con lo script: tutte le
// prove le costruisce lo script quando parte il Test.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
// Tabulazioni -> spazi: cosi' il file e' anche JSON valido (lo Studio le
// scriverebbe crude, che Blocky accetta ma un lettore JSON normale no).
const script = readFileSync(join(here, "laboratorio_fisica.lua"), "utf8").replace(/\r/g, "").replace(/\t/g, "    ");

// Il caricatore dei place chiude una part alla prima graffa chiusa e conta le
// quadre per trovare la fine dell'elenco: lo script deve rispettarlo.
if (/[{}]/.test(script)) throw new Error("lo script non deve contenere parentesi graffe");
const open = (script.match(/\[/g) || []).length, close = (script.match(/\]/g) || []).length;
if (open !== close) throw new Error(`quadre sbilanciate: ${open} [ contro ${close} ]`);
if (script.includes('"name"') || script.includes('"scriptType"')) throw new Error("testo riservato nello script");

// Come PlaceExporter::escapeScript.
const esc = (s) => s.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\n/g, "\\n");
const f = (n) => Number(n).toFixed(3);
const v = (a) => `[${a.map(f).join(",")}]`;

function part(o) {
  const p = {
    anchored: true, canCollide: true, canTouch: true, canQuery: true, castShadow: true, locked: false,
    transparency: 0, reflectance: 0, density: 0.7, friction: 0.5, elasticity: 0.1,
    linearDamping: 0.05, angularDamping: 0.05, collisionGroup: 0, vehicleSeat: false,
    hitboxScaleX: 1, hitboxScaleY: 1, hitboxScaleZ: 1, script: "", scriptType: 0, ...o,
  };
  const b = (x) => (x ? "true" : "false");
  return `    {"name":"${p.name}","pos":${v(p.pos)},"size":${v(p.size)},"rot":${v(p.rot || [0, 0, 0])},"color":${v(p.color)}` +
    `,"anchored":${b(p.anchored)},"canCollide":${b(p.canCollide)},"canTouch":${b(p.canTouch)},"canQuery":${b(p.canQuery)}` +
    `,"castShadow":${b(p.castShadow)},"locked":${b(p.locked)},"transparency":${f(p.transparency)},"reflectance":${f(p.reflectance)}` +
    `,"density":${f(p.density)},"friction":${f(p.friction)},"elasticity":${f(p.elasticity)},"linearDamping":${f(p.linearDamping)}` +
    `,"angularDamping":${f(p.angularDamping)},"collisionGroup":${f(p.collisionGroup)},"vehicleSeat":${b(p.vehicleSeat)}` +
    `,"hitboxScaleX":${f(p.hitboxScaleX)},"hitboxScaleY":${f(p.hitboxScaleY)},"hitboxScaleZ":${f(p.hitboxScaleZ)}` +
    `,"shape":"Part","material":"${p.material || "Plastic"}","emissive":0.000,"pointLight":false,"lightRange":20.000` +
    `,"lightBrightness":1.250,"canDestroy":false,"health":100.000,"maxHealth":100.000,"parentIndex":-1,"groupId":0` +
    `,"hasScript":${b(p.script.length > 0)},"script":"${esc(p.script)}","scriptType":${p.scriptType}}`;
}

const parts = [
  part({ name: "Baseplate", pos: [0, -1, 0], size: [260, 2, 240], color: [0.55, 0.56, 0.55], locked: true }),
  // LocalScript del laboratorio (gira su ogni giocatore, anche nei giochi
  // pubblicati: BlockyServer non esegue Luau e il client salta gli Script
  // server). Part piccola, invisibile, sotto lo spawn.
  part({ name: "LaboratorioFisica", pos: [0, 0.5, -6], size: [1, 1, 1], color: [1, 1, 1],
         transparency: 1, canCollide: false, canTouch: false, canQuery: false, castShadow: false,
         script, scriptType: 2 }),
];

const json = `{
  "format": "BlockyPlace",
  "version": 2,
  "placeName": "Laboratorio di Fisica",
  "lighting": {"dayNight":0,"dayLengthMinutes":8.000,"fixedDayClock":14.000,"fixedNightClock":0.000,"clockTime":14.000,"brightness":2.000,"ambient":0.450,"globalShadows":true},
  "spawn": {"pos":[0.000,0.000,0.000],"x":0.000,"y":0.000,"z":0.000,"yaw":-90.000},
  "player": {"x":0.000,"y":0.000,"z":0.000,"yaw":180.000},
  "humanoid": {"health":100.000,"maxHealth":100.000,"walkSpeed":16.000,"jumpPower":52.000},
  "groups": [],
  "constraints": [],
  "parts": [
${parts.join(",\n")}
  ],
  "labels": [
  ],
  "instances": [
  ]
}
`;
const out = join(here, "Laboratorio_Fisica.blocky");
writeFileSync(out, json);
console.log(`scritto ${out} (${json.length} byte, script ${script.length} caratteri)`);
