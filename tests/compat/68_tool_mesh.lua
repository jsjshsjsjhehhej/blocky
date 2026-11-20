-- Un tool puo' puntare a un modello (obj/fbx/glb) come in Studio.
local tool = Instance.new("Tool")
tool.Name = "Revolver"
tool.MeshId = "models/revolver.glb"
assert(type(tool.MeshId) == "string" and #tool.MeshId > 0, "MeshId")
tool.MeshId = ""
assert(tool.MeshId == "", "MeshId vuoto")
COMPAT_OK = true
