# Animazioni R6 di Blocky, generate in Blender (nessun dato copiato da Roblox:
# pose e tempi sono disegnati qui, nello stile "a blocchi" delle R6).
#
#   blender -b --factory-startup --python tools/blender/blocky_r6_anims.py -- <cartella_uscita> [video|stills|all]
#
# Escono:
#   blocky_r6_anims.blend   - rig + tutte le azioni, da ritoccare a mano
#   blocky_r6_anims.glb     - armatura con un'animazione per azione (tracce NLA)
#   anteprima_tutte.mp4     - tutte le animazioni insieme, una per personaggio
#   frame_###.png           - qualche fotogramma fisso della griglia
#
# Convenzioni del rig (come le R6): HumanoidRootPart -> Torso -> Head,
# braccia e gambe. Il personaggio guarda verso -Y. Le ossa puntano tutte in
# su, cosi' gli assi locali sono gli stessi per tutte:
#   X locale = asse dei fianchi (avanti/indietro), Y = verticale (girarsi),
#   Z = asse avanti-dietro (aprire di lato / inclinarsi).

import bpy
import bmesh
import math
import os
import sys
from mathutils import Vector

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = os.path.abspath(ARGS[0] if ARGS else "animazioni_r6")
MODE = ARGS[1] if len(ARGS) > 1 else "all"
os.makedirs(OUT, exist_ok=True)
FPS = 30

# ---------------------------------------------------------------- rig ------

BONES = [
    # nome,                testa (perno),       genitore
    ("HumanoidRootPart", (0.0, 0.0, 3.0), None),
    ("Torso",            (0.0, 0.0, 3.0), "HumanoidRootPart"),
    ("Head",             (0.0, 0.0, 4.0), "Torso"),
    ("Right Arm",        (-1.5, 0.0, 3.5), "Torso"),   # destra del personaggio = -X (guarda verso -Y)
    ("Left Arm",         (1.5, 0.0, 3.5), "Torso"),
    ("Right Leg",        (-0.5, 0.0, 2.0), "Torso"),
    ("Left Leg",         (0.5, 0.0, 2.0), "Torso"),
]

YELLOW = (0.96, 0.80, 0.19, 1.0)
BLUE = (0.05, 0.41, 0.67, 1.0)
GREEN = (0.29, 0.59, 0.29, 1.0)
DARK = (0.08, 0.08, 0.09, 1.0)

# (gruppo/osso, centro, dimensioni, materiale)
BOXES = [
    ("Torso", (0.0, 0.0, 3.0), (2.0, 1.0, 2.0), 1),
    ("Head", (0.0, 0.0, 4.62), (1.25, 1.25, 1.25), 0),
    ("Right Arm", (-1.5, 0.0, 3.0), (1.0, 1.0, 2.0), 0),
    ("Left Arm", (1.5, 0.0, 3.0), (1.0, 1.0, 2.0), 0),
    ("Right Leg", (-0.5, 0.0, 1.0), (1.0, 1.0, 2.0), 2),
    ("Left Leg", (0.5, 0.0, 1.0), (1.0, 1.0, 2.0), 2),
    # faccia: due occhi e il sorriso, per vedere dove guarda
    ("Head", (-0.25, -0.63, 4.75), (0.14, 0.04, 0.22), 3),
    ("Head", (0.25, -0.63, 4.75), (0.14, 0.04, 0.22), 3),
    ("Head", (0.0, -0.63, 4.38), (0.5, 0.04, 0.09), 3),
]


def make_material(name, color):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = color
    return m


MATERIALS = None


def build_character(name, at=(0.0, 0.0), coll=None):
    coll = coll or bpy.context.scene.collection
    global MATERIALS
    if MATERIALS is None:
        MATERIALS = [make_material("Blocky_Giallo", YELLOW), make_material("Blocky_Blu", BLUE),
                     make_material("Blocky_Verde", GREEN), make_material("Blocky_Faccia", DARK)]
    arm = bpy.data.armatures.new(name + "_Rig")
    rig = bpy.data.objects.new(name, arm)
    coll.objects.link(rig)
    rig.location = (at[0], at[1], 0.0)
    if coll is not bpy.context.scene.collection:
        bpy.context.scene.collection.objects.link(rig)   # serve per entrare in Edit Mode
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    for bname, head, parent in BONES:
        eb = arm.edit_bones.new(bname)
        eb.head = head
        eb.tail = (head[0], head[1], head[2] + 0.5)
        eb.roll = 0.0
        if parent:
            eb.parent = arm.edit_bones[parent]
            eb.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    if coll is not bpy.context.scene.collection:
        bpy.context.scene.collection.objects.unlink(rig)
    for pb in rig.pose.bones:
        pb.rotation_mode = "XYZ"

    mesh = bpy.data.meshes.new(name + "_Corpo")
    body = bpy.data.objects.new(name + "_Corpo", mesh)
    coll.objects.link(body)
    for m in MATERIALS:
        mesh.materials.append(m)
    bm = bmesh.new()
    deform = bm.verts.layers.deform.verify()
    groups = {}
    for bname, _, _ in BONES:
        groups[bname] = len(groups)
        body.vertex_groups.new(name=bname)
    for bname, c, s, mat in BOXES:
        res = bmesh.ops.create_cube(bm, size=1.0)
        for v in res["verts"]:
            v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
            v[deform][groups[bname]] = 1.0
        faces = {f for v in res["verts"] for f in v.link_faces}
        for f in faces:
            f.material_index = mat
    bm.to_mesh(mesh)
    bm.free()
    body.parent = rig
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = rig
    return rig


# ------------------------------------------------------------- pose ------
# Una posa si scrive con nomi chiari (gradi e stud):
#   up / fwd_off       : corpo su/giu', avanti
#   t_pitch t_yaw t_roll : busto (pitch + = in avanti, yaw + = verso sinistra)
#   h_pitch h_yaw h_roll : testa (pitch + = guarda giu')
#   ra_* / la_*        : braccio destro / sinistro: fwd (+ = avanti/su), out (+ = di lato), twist
#   rl_* / ll_*        : gamba destra / sinistra: fwd, out


def bone_euler(pose):
    g = lambda k: math.radians(pose.get(k, 0.0))
    side = {"Right": -1.0, "Left": 1.0}
    rot = {
        "HumanoidRootPart": (0.0, 0.0, 0.0),
        "Torso": (g("t_pitch"), g("t_yaw"), g("t_roll")),
        "Head": (g("h_pitch"), g("h_yaw"), g("h_roll")),
    }
    for pre, bname in (("ra", "Right Arm"), ("la", "Left Arm"), ("rl", "Right Leg"), ("ll", "Left Leg")):
        s = side[bname.split()[0]]
        rot[bname] = (-g(pre + "_fwd"), g(pre + "_twist") * s, g(pre + "_out") * s)
    return rot


def key_pose(rig, frame, pose):
    rot = bone_euler(pose)
    for pb in rig.pose.bones:
        pb.rotation_euler = rot[pb.name]
        pb.keyframe_insert("rotation_euler", frame=frame)
    root = rig.pose.bones["HumanoidRootPart"]
    # assi locali della radice: X = X mondo, Y = su, Z = avanti (-Y mondo)
    root.location = (pose.get("side_off", 0.0), pose.get("up", 0.0), pose.get("fwd_off", 0.0))
    root.keyframe_insert("location", frame=frame)


def merge(*dicts):
    out = {}
    for d in dicts:
        out.update(d)
    return out


REST = {"ra_out": 3, "la_out": 3}

ANIMS = {}


def anim(name, length, loop=True):
    def deco(fn):
        ANIMS[name] = (length, loop, fn)
        return fn
    return deco


# Stile R6, versione 4. Ampiezze e tempi presi a riferimento MISURANDO le
# animazioni R6 (come si fa con un video di riferimento): i keyframe qui sotto
# sono nostri, pochi e arrotondati, non una copia dei dati originali.
#   - camminata: ciclo 0.67 s, braccia +-55, gambe piu' indietro (-65) che avanti (+45)
#   - salto: braccia avanti-su fino a ~120 e poi giu', gambe un po' indietro
#   - caduta: braccia a ~155 che oscillano poco, testa che dondola
#   - scalata: ciclo 0.46 s, testa alzata a guardare in su
#   - idle quasi ferma; idle 2 con le braccia che dondolano


@anim("Idle", 60)
def a_idle():
    a = {"ra_fwd": -0.5, "la_fwd": 0.5, "h_pitch": 0.0}
    b = {"ra_fwd": 0.6, "la_fwd": -0.4, "h_pitch": -0.5}
    return [(0, a), (30, b), (60, a)]


@anim("Idle2", 60)
def a_idle2():
    return [(0, {}), (15, {"la_fwd": 35, "ra_fwd": 37, "ll_out": 4}),
            (30, {"rl_fwd": 2}), (45, {"la_fwd": -28, "ra_fwd": -18, "rl_fwd": 3}), (60, {})]


def walk_keys(n):
    # n fotogrammi per ciclo; A = gamba sinistra indietro / braccio sinistro avanti
    h = n // 2
    q = n // 4
    a = {"la_fwd": 56, "ra_fwd": -45, "ll_fwd": -60, "rl_fwd": 45, "la_out": 6, "ra_out": 1, "h_pitch": 2}
    b = {"la_fwd": -30, "ra_fwd": 62, "ll_fwd": 44, "rl_fwd": -72, "la_out": 2, "ra_out": 5, "h_pitch": 2}
    mid_ab = {"la_fwd": 10, "ra_fwd": 20, "ll_fwd": -30, "rl_fwd": -10, "la_out": 3, "ra_out": 5, "h_pitch": 2, "t_yaw": -2}
    mid_ba = {"la_fwd": 15, "ra_fwd": 5, "ll_fwd": 10, "rl_fwd": -40, "la_out": 6, "ra_out": 2, "h_pitch": 2, "t_yaw": -2}
    return [(0, a), (q, mid_ab), (h + 1, b), (h + q + 1, mid_ba), (n, a)]


@anim("Walk", 20)
def a_walk():
    return walk_keys(20)


@anim("Jump", 19, loop=False)
def a_jump():
    return [
        (0, {}),
        (3, {"la_fwd": 12, "ra_fwd": 35, "rl_fwd": -6, "h_pitch": 1}),
        (7, {"la_fwd": 60, "ra_fwd": 92, "ll_fwd": -10, "rl_fwd": -13, "h_pitch": -2}),
        (11, {"la_fwd": 104, "ra_fwd": 124, "ll_fwd": -20, "rl_fwd": -11, "h_pitch": -3.5}),
        (14, {"la_fwd": 110, "ra_fwd": 108, "ll_fwd": -14, "rl_fwd": -7, "h_pitch": -3}),
        (16, {"la_fwd": 98, "ra_fwd": 58, "ll_fwd": -8, "rl_fwd": -4, "h_pitch": -2}),
        (19, {"la_fwd": 20, "ra_fwd": 12, "h_pitch": -1}),
    ]


@anim("Fall", 20)
def a_fall():
    a = {"la_fwd": 151, "ra_fwd": 157, "ll_fwd": 2, "rl_fwd": -2, "ll_out": 5, "rl_out": 3, "h_pitch": -5, "h_yaw": -3}
    b = {"la_fwd": 156, "ra_fwd": 146, "ll_fwd": 3, "rl_fwd": -11, "ll_out": 4, "rl_out": 3, "h_pitch": 2.5, "h_yaw": -3}
    c = {"la_fwd": 150, "ra_fwd": 158, "ll_fwd": -4, "rl_fwd": -12, "ll_out": 4, "rl_out": 4, "h_pitch": 0, "h_yaw": -3}
    return [(0, a), (7, b), (13, c), (20, a)]


@anim("Land", 6, loop=False)
def a_land():
    return [(0, {"la_fwd": 150, "ra_fwd": 155, "h_pitch": -4}), (3, {"la_fwd": 50, "ra_fwd": 55}), (6, {})]


@anim("Climb", 14)
def a_climb():
    base = {"h_pitch": -28, "t_roll": 2}
    return [
        (0, merge(base, {"la_fwd": 52, "ra_fwd": 160, "ll_fwd": 6, "rl_fwd": -36})),
        (4, merge(base, {"la_fwd": 145, "ra_fwd": 105, "ll_fwd": 18, "rl_fwd": 0, "t_roll": 0.5})),
        (7, merge(base, {"la_fwd": 135, "ra_fwd": 70, "ll_fwd": -34, "rl_fwd": 3, "t_roll": 0})),
        (11, merge(base, {"la_fwd": 85, "ra_fwd": 92, "ll_fwd": -4, "rl_fwd": 16, "t_roll": 1.3})),
        (14, merge(base, {"la_fwd": 52, "ra_fwd": 160, "ll_fwd": 6, "rl_fwd": -36})),
    ]


@anim("Sit", 30)
def a_sit():
    s = {"up": -1.0, "rl_fwd": 90, "ll_fwd": 90, "ra_fwd": 90, "la_fwd": 90}
    return [(0, s), (30, s)]


@anim("Tool", 60)
def a_tool():
    # idle quasi ferma con il braccio destro fisso a 90 gradi
    a = {"ra_fwd": 90, "la_fwd": 0.5}
    b = {"ra_fwd": 90, "la_fwd": -0.4, "h_pitch": -0.5}
    return [(0, a), (30, b), (60, a)]


@anim("Wave", 60, loop=False)
def a_wave():
    # (emote nostra) braccio destro alzato di lato che va avanti e indietro
    a = {"ra_out": 160}
    b = {"ra_out": 125}
    return [(0, {}), (6, a), (14, b), (22, a), (30, b), (38, a), (46, b), (54, {}), (60, {})]


@anim("Dance", 48)
def a_dance():
    # (emote nostra) braccia a turno in alto, gambe che si aprono a tempo
    k = [
        (0, {"ra_fwd": 180, "la_fwd": 0}),
        (6, {"ra_fwd": 90, "la_fwd": 90, "rl_out": 12, "ll_out": 12}),
        (12, {"ra_fwd": 0, "la_fwd": 180}),
        (18, {"ra_fwd": 90, "la_fwd": 90, "rl_out": 12, "ll_out": 12}),
        (24, {"ra_out": 90, "la_out": 90}),
        (30, {"ra_out": 20, "la_out": 20, "rl_fwd": 30, "ll_fwd": -30}),
        (36, {"ra_out": 90, "la_out": 90}),
        (42, {"ra_out": 20, "la_out": 20, "rl_fwd": -30, "ll_fwd": 30}),
    ]
    k.append((48, k[0][1]))
    return k


def make_action(rig, name, length, loop, keys):
    rig.animation_data_create()
    rig.animation_data.action = None
    for frame, pose in keys:
        key_pose(rig, frame, pose)
    act = rig.animation_data.action
    act.name = name
    act.use_fake_user = True
    act.use_frame_range = True
    act.frame_range = (0, length)
    act.use_cyclic = loop
    # Ripetizione infinita (Cycles) per l'anteprima e per chi la apre in Blender.
    try:
        from bpy_extras import anim_utils
        cb = anim_utils.action_get_channelbag_for_slot(act, rig.animation_data.action_slot)
        for fc in cb.fcurves:
            fc.modifiers.new("CYCLES")
            for kp in fc.keyframe_points:
                kp.interpolation = "BEZIER"
                kp.handle_left_type = kp.handle_right_type = "AUTO_CLAMPED"
    except Exception as e:  # API diversa: si va avanti senza ripetizione
        print("[anim] niente Cycles su", name, e)
    return act


# ----------------------------------------------------------- scena ------

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = FPS

names = list(ANIMS.keys())
COLS = 4
SPACE_X, SPACE_Y = 6.5, 9.0
rows = (len(names) + COLS - 1) // COLS
grid_rigs = []
for i, n in enumerate(names):
    cx = (i % COLS - (COLS - 1) / 2.0) * SPACE_X
    cy = (i // COLS) * SPACE_Y
    rig = build_character("R6_" + n, (cx, cy))
    length, loop, fn = ANIMS[n]
    make_action(rig, n + "_anteprima", length, True, fn())
    grid_rigs.append(rig)

# rig "pulito" per l'esportazione: tutte le azioni come tracce NLA
master = build_character("BlockyR6", (0.0, -40.0))
master.animation_data_create()
for n in names:
    length, loop, fn = ANIMS[n]
    act = make_action(master, n, length, loop, fn())
    master.animation_data.action = None
    tr = master.animation_data.nla_tracks.new()
    tr.name = n
    strip = tr.strips.new(n, 0, act)
    tr.mute = True
master.hide_render = True

# Pose per il motore: ogni fotogramma di ogni animazione, rotazione di ogni parte
# rispetto alla parte padre come quaternione [w, x, y, z] NEL SISTEMA DI BLOCKY
# (avanti +Z, su +Y, lato del braccio sinistro -X). Qui il personaggio guarda -Y
# e ha il braccio sinistro a +X: rispetto a Blocky e' uno specchio sull'asse X,
# quindi (w, x, y, z) -> (w, x, -y, -z) e lo spostamento laterale cambia segno.
# tools/make_r6_anims.cpp ne fa il file .blkanim del motore.
import json
export = {"format": "blocky-r6-frames", "fps": FPS, "animations": []}
for n in names:
    length, loop, fn = ANIMS[n]
    master.animation_data.action = bpy.data.actions[n]
    frames = []
    for fr in range(length + 1):
        scene.frame_set(fr)
        bones = {}
        for pb in master.pose.bones:
            if pb.name == "HumanoidRootPart":
                continue
            q = pb.rotation_euler.to_quaternion()
            bones[pb.name] = [round(q.w, 6), round(q.x, 6), round(-q.y, 6), round(-q.z, 6)]
        loc = master.pose.bones["HumanoidRootPart"].location
        frames.append({"bones": bones, "root": [round(-loc.x, 5), round(loc.y, 5), round(loc.z, 5)]})
    export["animations"].append({"name": n, "loop": loop, "length": length, "frames": frames})
master.animation_data.action = None
with open(os.path.join(OUT, "r6_pose_frames.json"), "w", encoding="utf-8") as fp:
    json.dump(export, fp, separators=(",", ":"))
print("[anim] pose per il motore:", len(names), "animazioni")
for ch in master.children:
    ch.hide_render = True

# pavimento
bpy.ops.mesh.primitive_plane_add(size=1, location=(0, (rows - 1) * SPACE_Y / 2.0, 0))
floor = bpy.context.active_object
floor.scale = (COLS * SPACE_X + 6, rows * SPACE_Y + 8, 1)
floor.data.materials.append(make_material("Pavimento", (0.62, 0.64, 0.62, 1.0)))

# camera ortografica 3/4 davanti, dall'alto
cam_data = bpy.data.cameras.new("Cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = 31.0
cam = bpy.data.objects.new("Cam", cam_data)
scene.collection.objects.link(cam)
target = Vector((0.0, (rows - 1) * SPACE_Y / 2.0 + 0.5, 2.0))
cam.location = target + Vector((9.0, -30.0, 17.0))
cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
scene.camera = cam


def add_label(text, loc, coll, cam_obj, size=0.85):
    cu = bpy.data.curves.new("Lbl_" + text, "FONT")
    cu.body = text
    cu.align_x = "CENTER"
    cu.size = size
    lbl = bpy.data.objects.new("Lbl_" + text, cu)
    lbl.location = loc
    lbl.rotation_euler = cam_obj.rotation_euler   # rivolta alla camera
    lbl.data.materials.append(make_material("Etichetta", (0.05, 0.05, 0.06, 1.0)))
    coll.objects.link(lbl)


for i, n in enumerate(names):
    cx = (i % COLS - (COLS - 1) / 2.0) * SPACE_X
    cy = (i // COLS) * SPACE_Y
    add_label(n, (cx, cy, 5.9), scene.collection, cam)

scene.view_settings.view_transform = "Standard"   # colori veri, non "filmici"
scene.render.engine = "BLENDER_WORKBENCH"
sh = scene.display.shading
sh.light = "STUDIO"
sh.color_type = "MATERIAL"
sh.show_shadows = True
sh.shadow_intensity = 0.35
sh.show_cavity = True
scene.display.light_direction = (0.45, -0.35, 0.82)
world = bpy.data.worlds.new("Cielo")
world.color = (0.55, 0.72, 0.95)
scene.world = world
scene.render.resolution_x = 1280
scene.render.resolution_y = 960
scene.render.resolution_percentage = 100
scene.frame_start = 0
scene.frame_end = 179   # 6 secondi: ogni animazione si ripete

# Chi apre il file deve solo premere Spazio: vista dalla camera, colori dei
# materiali, timeline sull'anteprima (0-179, in loop).
scene.frame_current = 0
for scr in bpy.data.screens:
    for area in scr.areas:
        if area.type != "VIEW_3D":
            continue
        for sp in area.spaces:
            if sp.type == "VIEW_3D":
                sp.region_3d.view_perspective = "CAMERA"
                sp.shading.type = "SOLID"
                sp.shading.color_type = "MATERIAL"
                sp.shading.light = "STUDIO"
                sp.overlay.show_overlays = False
                sp.show_gizmo = False
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "blocky_r6_anims.blend"))

try:
    bpy.ops.object.select_all(action="DESELECT")
    master.select_set(True)
    for ch in master.children:
        ch.select_set(True)
    bpy.context.view_layer.objects.active = master
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, "blocky_r6_anims.glb"), use_selection=True,
                              export_animation_mode="NLA_TRACKS", export_force_sampling=True)
    print("[anim] GLB esportato")
except Exception as e:
    print("[anim] GLB non esportato:", e)

if MODE in ("stills", "all"):
    scene.render.image_settings.file_format = "PNG"
    for f in (0, 4, 9, 15, 24, 33, 48):
        scene.frame_set(f)
        scene.render.filepath = os.path.join(OUT, "frame_%03d.png" % f)
        bpy.ops.render.render(write_still=True)

if MODE in ("strips", "all"):
    # Per ogni animazione: 5 copie del personaggio in fila, ognuna a un istante
    # diverso (traccia NLA spostata nel tempo), fotografate in un'immagine sola.
    # Stessa scena (in background Blender anima solo la scena attiva): ogni
    # striscia ha la sua collezione e si accende solo quella da fotografare.
    T = 1000
    before = [o for o in scene.objects if not o.hide_render]
    for o in before:
        o.hide_render = True
    old_cam = scene.camera
    rx, ry = scene.render.resolution_x, scene.render.resolution_y
    scene.render.resolution_x, scene.render.resolution_y = 1800, 520
    scene.render.image_settings.file_format = "PNG"
    base = Vector((0.0, -300.0, 0.0))
    strip_colls = []
    for n in names:
        length, loop, fn = ANIMS[n]
        coll = bpy.data.collections.new("Striscia_" + n)
        scene.collection.children.link(coll)
        strip_colls.append(coll)
        times = [round(length * k / 5.0) for k in range(5)]
        for j, t in enumerate(times):
            r = build_character("S_%s_%d" % (n, j), (base.x + (j - 2) * 6.0, base.y), coll)
            r.animation_data_create()
            tr = r.animation_data.nla_tracks.new()
            tr.strips.new(n, T - t, bpy.data.actions[n])
        cd = bpy.data.cameras.new("CamS_" + n)
        cd.type = "ORTHO"
        cd.ortho_scale = 31.0
        c = bpy.data.objects.new("CamS_" + n, cd)
        coll.objects.link(c)
        tgt = base + Vector((0.0, 0.0, 3.0))
        c.location = tgt + Vector((7.0, -26.0, 6.0))
        c.rotation_euler = (tgt - c.location).to_track_quat("-Z", "Y").to_euler()
        for j, t in enumerate(times):
            add_label("f%d" % t, (base.x + (j - 2) * 6.0, base.y, 6.3), coll, c, 0.55)
        fl = bpy.data.objects.new("PavS_" + n, floor.data)
        fl.location = base
        fl.scale = (36, 10, 1)
        coll.objects.link(fl)
    for k, n in enumerate(names):
        for j, coll in enumerate(strip_colls):
            coll.hide_render = (j != k)
        scene.camera = bpy.data.objects["CamS_" + n]
        scene.frame_set(T)
        scene.render.filepath = os.path.join(OUT, "striscia_%s.png" % n)
        bpy.ops.render.render(write_still=True)
    for coll in strip_colls:
        coll.hide_render = True
    for o in before:
        o.hide_render = False
    scene.camera = old_cam
    scene.render.resolution_x, scene.render.resolution_y = rx, ry

if MODE in ("video", "all"):
    im = scene.render.image_settings
    im.media_type = "VIDEO"
    im.file_format = "FFMPEG"
    ff = scene.render.ffmpeg
    ff.format = "MPEG4"
    ff.codec = "H264"
    ff.constant_rate_factor = "HIGH"
    scene.render.filepath = os.path.join(OUT, "anteprima_tutte.mp4")
    bpy.ops.render.render(animation=True)
    print("[anim] video:", scene.render.filepath)

print("[anim] FATTO", OUT, ", ".join(names))
