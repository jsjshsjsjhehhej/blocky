// Genera docs/API_CHECKLIST.md: la lista di controllo delle API Roblox da avere
// in Blocky, pacchetto per pacchetto (vedi docs/PROMPT_API_LUAU.md), presa dal dump
// vero dell'API (tools/roblox_api_dump/Full-API-Dump.json): nomi, tipi, argomenti
// e valori di partenza esatti.
// Salta quello che un gioco non puo' usare: membri deprecati, nascosti, non
// scriptabili, riservati a Roblox o ai plugin, e i vecchi alias in minuscolo.
//
//   node tools/roblox_api/gen_checklist.mjs
//
// Le caselle si spuntano a mano ([x]) man mano che le API funzionano davvero
// (con un test in tests/compat). Rigenerare il file perde le spunte: prima di
// rigenerare, copia le righe gia' spuntate.

import { readFileSync, writeFileSync } from "node:fs";

const dump = JSON.parse(readFileSync(new URL("../roblox_api_dump/Full-API-Dump.json", import.meta.url), "utf8"));
const byName = new Map(dump.Classes.map((c) => [c.Name, c]));

const PACCHETTI = [
  ["P2 - L'albero delle Instance", [
    "Instance", "Folder", "Configuration", "ValueBase", "IntValue", "NumberValue", "StringValue",
    "BoolValue", "ObjectValue", "Vector3Value", "CFrameValue", "Color3Value", "BrickColorValue",
    "RayValue", "BindableEvent", "BindableFunction", "ModuleScript", "LuaSourceContainer",
    "BaseScript", "Script", "LocalScript", "CollectionService"]],
  ["P3 - Mondo e part", [
    "PVInstance", "BasePart", "FormFactorPart", "Part", "WedgePart", "CornerWedgePart", "TrussPart",
    "MeshPart", "PartOperation", "UnionOperation", "IntersectOperation", "NegateOperation",
    "SpawnLocation", "Model", "WorldRoot", "Workspace", "Terrain", "Decal", "Texture",
    "DataModelMesh", "FileMesh", "SpecialMesh", "BlockMesh", "SurfaceAppearance", "MaterialVariant"]],
  ["P4 - Fisica e movimento", [
    "Attachment", "Bone", "Constraint", "WeldConstraint", "NoCollisionConstraint", "JointInstance",
    "Weld", "Motor6D", "Motor", "HingeConstraint", "PrismaticConstraint", "SlidingBallConstraint",
    "CylindricalConstraint", "BallSocketConstraint", "UniversalConstraint", "PlaneConstraint",
    "RopeConstraint", "RodConstraint", "SpringConstraint", "LinearVelocity", "AngularVelocity",
    "AlignPosition", "AlignOrientation", "VectorForce", "Torque", "LineForce",
    "BodyMover", "BodyVelocity", "BodyPosition", "BodyGyro", "BodyForce", "BodyAngularVelocity",
    "BodyThrust", "RocketPropulsion", "PhysicsService", "Seat", "VehicleSeat"]],
  ["P5 - Personaggi, NPC e animazioni", [
    "Humanoid", "HumanoidDescription", "AnimationController", "Animator", "Animation",
    "AnimationTrack", "Keyframe", "KeyframeSequence", "KeyframeMarker", "Player", "Players",
    "Team", "Teams", "PathfindingService", "Path", "PathfindingModifier", "PathfindingLink",
    "Clothing", "Shirt", "Pants", "ShirtGraphic", "BodyColors", "Accoutrement", "Accessory",
    "CharacterMesh", "StarterPlayer", "StarterPlayerScripts", "StarterCharacterScripts"]],
  ["P6 - Strumenti e combattimento", [
    "BackpackItem", "Tool", "Backpack", "StarterPack", "StarterGear", "Explosion", "ForceField",
    "Debris", "ProximityPrompt", "ProximityPromptService", "ClickDetector", "DragDetector",
    "Mouse", "PlayerMouse"]],
  ["P7 - GUI", [
    "GuiBase", "GuiBase2d", "GuiObject", "GuiButton", "GuiLabel", "Frame", "TextLabel", "TextButton",
    "TextBox", "ImageLabel", "ImageButton", "ScrollingFrame", "ViewportFrame", "WorldModel",
    "CanvasGroup", "LayerCollector", "ScreenGui", "BillboardGui", "SurfaceGuiBase", "SurfaceGui",
    "PlayerGui", "StarterGui", "GuiService", "TextService", "UIComponent", "UIBase", "UILayout",
    "UIGridStyleLayout", "UIListLayout", "UIGridLayout", "UIPageLayout", "UITableLayout", "UIPadding",
    "UICorner", "UIStroke", "UIGradient", "UIScale", "UIConstraint", "UIAspectRatioConstraint",
    "UISizeConstraint", "UITextSizeConstraint", "UIFlexItem", "UIDragDetector"]],
  ["P8 - Input, camera, tempo", [
    "UserInputService", "InputObject", "ContextActionService", "Camera", "RunService",
    "TweenBase", "Tween", "TweenService"]],
  ["P9 - Effetti, luci, atmosfera, suono", [
    "ParticleEmitter", "Beam", "Trail", "Fire", "Smoke", "Sparkles", "Highlight",
    "PVAdornment", "SelectionBox", "HandleAdornment", "BoxHandleAdornment", "SphereHandleAdornment",
    "CylinderHandleAdornment", "ConeHandleAdornment", "LineHandleAdornment",
    "Light", "PointLight", "SpotLight", "SurfaceLight", "Lighting", "Sky", "Atmosphere", "Clouds",
    "PostEffect", "BloomEffect", "BlurEffect", "ColorCorrectionEffect", "SunRaysEffect",
    "DepthOfFieldEffect", "Sound", "SoundGroup", "SoundService", "SoundEffect", "ReverbSoundEffect",
    "EqualizerSoundEffect", "EchoSoundEffect", "DistortionSoundEffect", "ChorusSoundEffect",
    "CompressorSoundEffect", "FlangeSoundEffect", "PitchShiftSoundEffect", "TremoloSoundEffect"]],
  ["P10 - Dati, rete e servizi", [
    "DataModel", "ServiceProvider", "ReplicatedStorage", "ReplicatedFirst", "ServerStorage",
    "ServerScriptService", "RemoteEvent", "RemoteFunction", "UnreliableRemoteEvent",
    "DataStoreService", "GlobalDataStore", "DataStore", "OrderedDataStore", "Pages",
    "DataStorePages", "DataStoreKeyPages", "DataStoreKeyInfo", "DataStoreSetOptions",
    "MemoryStoreService", "MemoryStoreSortedMap", "MemoryStoreQueue", "MemoryStoreHashMap",
    "MessagingService", "HttpService", "MarketplaceService", "BadgeService", "GroupService",
    "PolicyService", "LocalizationService", "TextChatService", "TextChannel", "TextChatCommand",
    "TextChatMessage", "TextSource", "TeleportService", "TeleportOptions", "SocialService", "Chat"]],
];

const SICURI = new Set(["None"]);
function usabile(m) {
  const tags = m.Tags || [];
  if (tags.includes("Deprecated") || tags.includes("Hidden") || tags.includes("NotScriptable")) return false;
  if (/^[a-z]/.test(m.Name)) return false;   // vecchi alias: className, findFirstChild...
  const s = m.Security;
  if (typeof s === "string") return SICURI.has(s);
  if (s && typeof s === "object") return SICURI.has(s.Read) || SICURI.has(s.Write);
  return true;
}
const tipo = (t) => (t ? t.Name : "?");
const args = (ps) => (ps || []).map((p) => `${p.Name}: ${tipo(p.Type)}${p.Default !== undefined ? " = " + p.Default : ""}`).join(", ");

function riga(cls, m) {
  const tags = (m.Tags || []).filter((t) => ["ReadOnly", "Yields", "CustomLuaState"].includes(t));
  const nota = tags.length ? `  _(${tags.join(", ")})_` : "";
  switch (m.MemberType) {
    case "Property": {
      const def = m.Default !== undefined && !String(m.Default).startsWith("__api_dump") ? ` = ${m.Default}` : "";
      return `- [ ] \`${cls}.${m.Name}\`: ${tipo(m.ValueType)}${def}${nota}`;
    }
    case "Function":
      return `- [ ] \`${cls}:${m.Name}(${args(m.Parameters)})\` -> ${tipo(m.ReturnType)}${nota}`;
    case "Event":
      return `- [ ] \`${cls}.${m.Name}\` (evento: ${args(m.Parameters) || "nessun argomento"})`;
    case "Callback":
      return `- [ ] \`${cls}.${m.Name}\` (callback: ${args(m.Parameters)}) -> ${tipo(m.ReturnType)}`;
    default:
      return null;
  }
}

let out = `# Lista di controllo delle API Roblox per Blocky

Generata da \`tools/roblox_api/gen_checklist.mjs\` dal dump dell'API Roblox
(\`tools/roblox_api_dump\`, versione ${readFileSync(new URL("../roblox_api_dump/version.txt", import.meta.url), "utf8").trim()}).
Una casella per ogni proprietà, metodo, evento e callback che un gioco può usare,
classe per classe, nei pacchetti di \`docs/PROMPT_API_LUAU.md\`. Sono esclusi i membri
deprecati, nascosti o riservati a Roblox e ai plugin.

**Regola:** si spunta \`[x]\` solo quando il membro **funziona come su Roblox** e c'è un test
in \`tests/compat\` che lo prova. "Esiste e non va in errore" non basta: per quello
c'è già lo strato "presente" (\`docs/api_coverage.md\`).
Ogni classe ha anche i membri delle classi da cui eredita (scritte accanto al nome).

`;
let totale = 0, mancanti = [];
const indice = [];
for (const [titolo, classi] of PACCHETTI) {
  let blocco = `\n## ${titolo}\n`;
  let n = 0;
  for (const nome of classi) {
    const c = byName.get(nome);
    if (!c) { mancanti.push(nome); continue; }
    const righe = c.Members.filter(usabile).map((m) => riga(nome, m)).filter(Boolean).sort();
    const tagsC = (c.Tags || []).filter((t) => ["NotCreatable", "Service", "Deprecated"].includes(t));
    blocco += `\n### ${nome}${c.Superclass && c.Superclass !== "<<<ROOT>>>" ? ` (eredita da ${c.Superclass})` : ""}${tagsC.length ? ` _${tagsC.join(", ")}_` : ""}\n`;
    blocco += righe.length ? righe.join("\n") + "\n" : "_(nessun membro proprio: vedi la classe da cui eredita)_\n";
    n += righe.length;
  }
  totale += n;
  indice.push(`| ${titolo} | ${classi.length} | ${n} |`);
  out += blocco;
}
out = out.replace("\nOgni classe ha anche", `\n| Pacchetto | Classi | Membri |\n|---|---|---|\n${indice.join("\n")}\n| **Totale** | | **${totale}** |\n\nOgni classe ha anche`);
writeFileSync(new URL("../../docs/API_CHECKLIST.md", import.meta.url), out);
console.log(`docs/API_CHECKLIST.md: ${totale} membri${mancanti.length ? "; classi non nel dump: " + mancanti.join(", ") : ""}`);
