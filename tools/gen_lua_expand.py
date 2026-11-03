# Generates src/engine/LuaRuntimeExpand.inl
from pathlib import Path

lines = []
def add(s=""):
    lines.append(s)

add("// Expanded Luau API surface — included inside LuaRuntime class")
add("// Instance.new extras + Part/Players/UIS/RunService/Debris helpers")
add("")

add("    static int tryInstanceNewExpand(lua_State* ls, const char* className) {")
add("        auto* rt = fromState(ls);")
add("        if (!rt || !rt->workspace || !className) return 0;")
add("")

for cn in ["IntValue", "NumberValue", "StringValue", "BoolValue", "ObjectValue",
           "CFrameValue", "Color3Value", "Vector3Value", "BrickColorValue"]:
    add(f'        if (std::strcmp(className, "{cn}") == 0) {{')
    add(f'            const char* nm = luaL_optstring(ls, 2, "{cn}");')
    add("            lua_newtable(ls);")
    add(f'            lua_pushstring(ls, "{cn}"); lua_setfield(ls, -2, "ClassName");')
    add("            lua_pushstring(ls, nm); lua_setfield(ls, -2, \"Name\");")
    if cn == "StringValue":
        add('            lua_pushstring(ls, ""); lua_setfield(ls, -2, "_value");')
    elif cn == "BoolValue":
        add("            lua_pushboolean(ls, 0); lua_setfield(ls, -2, \"_value\");")
    elif cn in ("ObjectValue", "CFrameValue", "Color3Value", "Vector3Value", "BrickColorValue"):
        add("            lua_pushnil(ls); lua_setfield(ls, -2, \"_value\");")
    else:
        add("            lua_pushnumber(ls, 0); lua_setfield(ls, -2, \"_value\");")
    add("            lua_newtable(ls);")
    add("            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, \"__index\");")
    add("            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, \"__newindex\");")
    add("            lua_setmetatable(ls, -2);")
    add("            return 1;")
    add("        }")

ui = [
    ("ScreenGui", "Frame", 0.0),
    ("Frame", "Frame", 0.5),
    ("ScrollingFrame", "ScrollingFrame", 0.5),
    ("TextLabel", "TextLabel", 0.5),
    ("TextButton", "TextButton", 0.5),
    ("ImageLabel", "ImageLabel", 0.5),
    ("ImageButton", "TextButton", 0.5),
    ("TextBox", "TextLabel", 0.5),
    ("ViewportFrame", "Frame", 0.5),
    ("CanvasGroup", "Frame", 0.5),
]
for cn, mapped, ay in ui:
    add(f'        if (std::strcmp(className, "{cn}") == 0) {{')
    add("            WorkspaceLabel lab;")
    add(f'            lab.name = luaL_optstring(ls, 2, "{cn}");')
    add(f'            lab.className = "{mapped}";')
    add("            lab.text = lab.name;")
    add("            lab.visible = true;")
    add(f"            lab.anchorX = 0.5f; lab.anchorY = {ay}f;")
    if mapped == "Frame":
        add("            lab.sizeW = 200; lab.sizeH = 100;")
    add("            if (lua_istable(ls, 3)) {")
    add('                lua_getfield(ls, 3, "__labelIndex");')
    add("                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);")
    add("                lua_pop(ls, 1);")
    add("            }")
    add("            rt->workspace->labels.push_back(std::move(lab));")
    add("            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);")
    add("            return 1;")
    add("        }")

add('        if (std::strcmp(className, "Model") == 0) {')
add("            WorkspaceModel m;")
add('            m.name = luaL_optstring(ls, 2, "Model");')
add("            m.position = {0, 0, 0};")
add("            rt->workspace->models.push_back(std::move(m));")
add("            pushLiveModel(ls, (int)rt->workspace->models.size() - 1);")
add("            return 1;")
add("        }")

add('        if (std::strcmp(className, "SpawnLocation") == 0) {')
add("            WorkspacePart p;")
add('            p.name = luaL_optstring(ls, 2, "SpawnLocation");')
add("            p.size = {6, 1, 6};")
add("            p.color = {0.2f, 0.8f, 0.4f};")
add("            p.position = rt->workspace->spawn.position;")
add("            rt->workspace->parts.push_back(p);")
add("            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);")
add('            lua_pushstring(ls, "SpawnLocation"); lua_setfield(ls, -2, "ClassName");')
add("            return 1;")
add("        }")

for cn in ["Seat", "VehicleSeat"]:
    add(f'        if (std::strcmp(className, "{cn}") == 0) {{')
    add(f'            WorkspacePart p; p.name = luaL_optstring(ls, 2, "{cn}");')
    add("            p.size = {4, 1, 2}; p.color = {0.75f, 0.55f, 0.2f};")
    add("            rt->workspace->parts.push_back(p);")
    add("            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);")
    add(f'            lua_pushstring(ls, "{cn}"); lua_setfield(ls, -2, "ClassName");')
    add("            return 1;")
    add("        }")

add('        if (std::strcmp(className, "PointLight") == 0 || std::strcmp(className, "SpotLight") == 0')
add('            || std::strcmp(className, "SurfaceLight") == 0) {')
add("            int partIdx = -1;")
add("            if (lua_istable(ls, 3)) {")
add('                lua_getfield(ls, 3, "__partIndex");')
add("                if (lua_isnumber(ls, -1)) partIdx = (int)lua_tointeger(ls, -1);")
add("                lua_pop(ls, 1);")
add("            }")
add("            if (partIdx >= 0 && partIdx < (int)rt->workspace->parts.size()) {")
add("                rt->workspace->parts[partIdx].pointLight = true;")
add("                rt->workspace->parts[partIdx].lightRange = 16.f;")
add("                rt->workspace->parts[partIdx].lightBrightness = 1.f;")
add("            }")
add("            lua_newtable(ls);")
add('            lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");')
add("            lua_pushstring(ls, luaL_optstring(ls, 2, className)); lua_setfield(ls, -2, \"Name\");")
add('            lua_pushinteger(ls, partIdx); lua_setfield(ls, -2, "__partIndex");')
add('            lua_pushnumber(ls, 16); lua_setfield(ls, -2, "Range");')
add('            lua_pushnumber(ls, 1); lua_setfield(ls, -2, "Brightness");')
add('            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");')
add("            return 1;")
add("        }")

add('        if (std::strcmp(className, "Fire") == 0 || std::strcmp(className, "Smoke") == 0')
add('            || std::strcmp(className, "Sparkles") == 0) {')
add("            ScriptParticleEmitter e;")
add('            e.rate = (std::strcmp(className, "Smoke") == 0) ? 6.f : 14.f;')
add('            e.preset = (std::strcmp(className, "Smoke") == 0) ? (int)ParticlePreset::Smoke : (int)ParticlePreset::Sparks;')
add("            if (lua_istable(ls, 3)) {")
add('                lua_getfield(ls, 3, "__partIndex");')
add("                if (lua_isnumber(ls, -1)) e.partIndex = (int)lua_tointeger(ls, -1);")
add("                lua_pop(ls, 1);")
add("            }")
add("            rt->scriptEmitters_.push_back(e);")
add("            lua_newtable(ls);")
add('            lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");')
add("            lua_pushstring(ls, luaL_optstring(ls, 2, className)); lua_setfield(ls, -2, \"Name\");")
add('            lua_pushinteger(ls, (int)rt->scriptEmitters_.size() - 1); lua_setfield(ls, -2, "__emitterIndex");')
add("            blockyPushCFunction(ls, luaParticleEmitterEmit); lua_setfield(ls, -2, \"Emit\");")
add("            return 1;")
add("        }")

add('        if (std::strcmp(className, "Attachment") == 0) {')
add("            ScriptControls::ScriptAttachment a;")
add('            a.name = luaL_optstring(ls, 2, "Attachment");')
add("            if (lua_istable(ls, 3)) {")
add('                lua_getfield(ls, 3, "__partIndex");')
add("                if (lua_isnumber(ls, -1)) a.partIndex = (int)lua_tointeger(ls, -1);")
add("                lua_pop(ls, 1);")
add("            }")
add("            scriptControls().attachments.push_back(a);")
add("            int ai = (int)scriptControls().attachments.size() - 1;")
add("            lua_newtable(ls);")
add('            lua_pushstring(ls, "Attachment"); lua_setfield(ls, -2, "ClassName");')
add('            lua_pushstring(ls, a.name.c_str()); lua_setfield(ls, -2, "Name");')
add('            lua_pushinteger(ls, ai); lua_setfield(ls, -2, "__attachmentIndex");')
add("            return 1;")
add("        }")

add('        if (std::strcmp(className, "Beam") == 0) {')
add("            ScriptControls::ScriptBeam b;")
add('            b.name = luaL_optstring(ls, 2, "Beam");')
add("            scriptControls().beams.push_back(b);")
add("            int bi = (int)scriptControls().beams.size() - 1;")
add("            lua_newtable(ls);")
add('            lua_pushstring(ls, "Beam"); lua_setfield(ls, -2, "ClassName");')
add('            lua_pushstring(ls, b.name.c_str()); lua_setfield(ls, -2, "Name");')
add('            lua_pushinteger(ls, bi); lua_setfield(ls, -2, "__beamIndex");')
add("            lua_newtable(ls);")
add("            blockyPushCFunction(ls, luaBeamNewIndex); lua_setfield(ls, -2, \"__newindex\");")
add("            blockyPushCFunction(ls, luaBeamIndex); lua_setfield(ls, -2, \"__index\");")
add("            lua_setmetatable(ls, -2);")
add("            return 1;")
add("        }")

for cn in ["Configuration", "Highlight", "SelectionBox", "SelectionSphere",
           "BoxHandleAdornment", "ConeHandleAdornment", "LineHandleAdornment",
           "CylinderHandleAdornment", "SphereHandleAdornment", "WireframeHandleAdornment"]:
    add(f'        if (std::strcmp(className, "{cn}") == 0) {{')
    add("            lua_newtable(ls);")
    add(f'            lua_pushstring(ls, "{cn}"); lua_setfield(ls, -2, "ClassName");')
    add(f'            lua_pushstring(ls, luaL_optstring(ls, 2, "{cn}")); lua_setfield(ls, -2, "Name");')
    add('            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");')
    add("            if (lua_istable(ls, 3)) {")
    add('                lua_getfield(ls, 3, "__partIndex");')
    add('                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }')
    add("                lua_pop(ls, 1);")
    add("            }")
    add("            return 1;")
    add("        }")

for cn in ["Accessory", "Hat", "Pants", "Shirt", "ShirtGraphic", "BodyColors",
           "CharacterMesh", "SpecialMesh", "BlockMesh", "CylinderMesh", "FileMesh"]:
    add(f'        if (std::strcmp(className, "{cn}") == 0) {{')
    add(f'            lua_newtable(ls); lua_pushstring(ls, "{cn}"); lua_setfield(ls, -2, "ClassName");')
    add(f'            lua_pushstring(ls, luaL_optstring(ls, 2, "{cn}")); lua_setfield(ls, -2, "Name");')
    add("            return 1;")
    add("        }")

for cn, wedge in [("CornerWedgePart", True), ("TrussPart", False), ("MeshPart", False),
                  ("UnionOperation", False), ("NegateOperation", False), ("IntersectOperation", False)]:
    add(f'        if (std::strcmp(className, "{cn}") == 0) {{')
    add("            WorkspacePart p;")
    add(f'            p.name = luaL_optstring(ls, 2, "{cn}");')
    add("            p.size = {4, 2, 4};")
    if wedge:
        add("            p.shape = PartShape::Wedge;")
    add("            rt->workspace->parts.push_back(p);")
    add("            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);")
    add(f'            lua_pushstring(ls, "{cn}"); lua_setfield(ls, -2, "ClassName");')
    add("            return 1;")
    add("        }")

add("        return 0;")
add("    }")
add("")

# Rest of methods from a template file appended
rest = r'''
    static int luaValueObjectIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (key && std::strcmp(key, "Value") == 0) { lua_getfield(ls, 1, "_value"); return 1; }
        lua_pushvalue(ls, 2); lua_rawget(ls, 1); return 1;
    }
    static int luaValueObjectNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (key && std::strcmp(key, "Value") == 0) { lua_pushvalue(ls, 3); lua_setfield(ls, 1, "_value"); return 0; }
        lua_rawset(ls, 1); return 0;
    }

    static int luaBeamIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        lua_getfield(ls, 1, "__beamIndex");
        int bi = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1; lua_pop(ls, 1);
        auto& beams = scriptControls().beams;
        if (bi < 0 || bi >= (int)beams.size() || !key) return 0;
        auto& b = beams[bi];
        if (std::strcmp(key, "Enabled") == 0) { lua_pushboolean(ls, b.enabled); return 1; }
        if (std::strcmp(key, "Width0") == 0) { lua_pushnumber(ls, b.width0); return 1; }
        if (std::strcmp(key, "Width1") == 0) { lua_pushnumber(ls, b.width1); return 1; }
        if (std::strcmp(key, "Attachment0") == 0) { lua_pushinteger(ls, b.a0); return 1; }
        if (std::strcmp(key, "Attachment1") == 0) { lua_pushinteger(ls, b.a1); return 1; }
        lua_pushvalue(ls, 2); lua_rawget(ls, 1); return 1;
    }
    static int luaBeamNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        lua_getfield(ls, 1, "__beamIndex");
        int bi = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1; lua_pop(ls, 1);
        auto& beams = scriptControls().beams;
        if (bi < 0 || bi >= (int)beams.size() || !key) return 0;
        auto& b = beams[bi];
        if (std::strcmp(key, "Enabled") == 0) b.enabled = lua_toboolean(ls, 3) != 0;
        else if (std::strcmp(key, "Width0") == 0) b.width0 = (float)lua_tonumber(ls, 3);
        else if (std::strcmp(key, "Width1") == 0) b.width1 = (float)lua_tonumber(ls, 3);
        else if (std::strcmp(key, "Attachment0") == 0) {
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__attachmentIndex");
                if (lua_isnumber(ls, -1)) b.a0 = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            } else if (lua_isnumber(ls, 3)) b.a0 = (int)lua_tointeger(ls, 3);
        } else if (std::strcmp(key, "Attachment1") == 0) {
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__attachmentIndex");
                if (lua_isnumber(ls, -1)) b.a1 = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            } else if (lua_isnumber(ls, 3)) b.a1 = (int)lua_tointeger(ls, 3);
        }
        lua_pushvalue(ls, 2); lua_pushvalue(ls, 3); lua_rawset(ls, 1);
        return 0;
    }

    static int luaPartIsA(lua_State* ls) {
        const char* cls = luaL_checkstring(ls, lua_istable(ls, 1) ? 2 : 1);
        bool ok = false;
        if (cls) {
            if (std::strcmp(cls, "Part") == 0 || std::strcmp(cls, "BasePart") == 0
                || std::strcmp(cls, "Instance") == 0 || std::strcmp(cls, "PVInstance") == 0)
                ok = true;
            if (lua_istable(ls, 1)) {
                lua_getfield(ls, 1, "ClassName");
                const char* cn = lua_tostring(ls, -1);
                if (cn && std::strcmp(cn, cls) == 0) ok = true;
                lua_pop(ls, 1);
            }
        }
        lua_pushboolean(ls, ok ? 1 : 0);
        return 1;
    }
    static int luaPartClone(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = partIndexFromSelf(ls, 1);
        if (!rt || !rt->workspace || idx < 0 || idx >= (int)rt->workspace->parts.size()) {
            lua_pushnil(ls); return 1;
        }
        WorkspacePart copy = rt->workspace->parts[idx];
        copy.name = copy.name + "_Clone";
        copy.parentIndex = -1;
        rt->workspace->parts.push_back(copy);
        pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
        return 1;
    }
    static int luaPartGetDescendants(lua_State* ls) {
        auto* rt = fromState(ls);
        int parent = partIndexFromSelf(ls, 1);
        lua_newtable(ls);
        if (!rt || !rt->workspace || parent < 0) return 1;
        int n = 1;
        std::vector<int> stack; stack.push_back(parent);
        std::vector<char> seen(rt->workspace->parts.size(), 0);
        while (!stack.empty()) {
            int cur = stack.back(); stack.pop_back();
            for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
                if (rt->workspace->parts[i].parentIndex == cur && !seen[i]) {
                    seen[i] = 1;
                    pushLivePart(ls, i);
                    lua_rawseti(ls, -2, n++);
                    stack.push_back(i);
                }
            }
        }
        return 1;
    }
    static int luaPartFindFirstChildOfClass(lua_State* ls) {
        auto* rt = fromState(ls);
        int parent = partIndexFromSelf(ls, 1);
        const char* cls = luaL_checkstring(ls, 2);
        if (!rt || !rt->workspace || !cls) { lua_pushnil(ls); return 1; }
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (rt->workspace->parts[i].parentIndex != parent) continue;
            if (std::strcmp(cls, "Part") == 0 || std::strcmp(cls, "BasePart") == 0) {
                pushLivePart(ls, i); return 1;
            }
        }
        lua_pushnil(ls); return 1;
    }
    static int luaPartFindFirstAncestor(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = partIndexFromSelf(ls, 1);
        const char* name = luaL_checkstring(ls, 2);
        if (!rt || !rt->workspace || idx < 0 || !name) { lua_pushnil(ls); return 1; }
        int walk = rt->workspace->parts[idx].parentIndex;
        while (walk >= 0 && walk < (int)rt->workspace->parts.size()) {
            if (rt->workspace->parts[walk].name == name) {
                pushLivePart(ls, walk); return 1;
            }
            walk = rt->workspace->parts[walk].parentIndex;
        }
        lua_pushnil(ls); return 1;
    }
    static int luaPartFindFirstAncestorOfClass(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = partIndexFromSelf(ls, 1);
        const char* cls = luaL_checkstring(ls, 2);
        if (!rt || !rt->workspace || idx < 0 || !cls) { lua_pushnil(ls); return 1; }
        int walk = rt->workspace->parts[idx].parentIndex;
        while (walk >= 0 && walk < (int)rt->workspace->parts.size()) {
            if (std::strcmp(cls, "Part") == 0 || std::strcmp(cls, "BasePart") == 0) {
                pushLivePart(ls, walk); return 1;
            }
            walk = rt->workspace->parts[walk].parentIndex;
        }
        lua_pushnil(ls); return 1;
    }
    static int luaPartGetPivot(lua_State* ls) {
        WorkspacePart* p = livePart(ls, 1);
        if (!p) { lua_pushnil(ls); return 1; }
        pushVec3(ls, p->position.x, p->position.y, p->position.z);
        return 1;
    }
    static int luaPartPivotTo(lua_State* ls) {
        WorkspacePart* p = livePart(ls, 1);
        if (!p) return 0;
        glm::vec3 pos = p->position;
        readVec3Arg(ls, 2, pos);
        p->position = pos;
        return 0;
    }
    static int luaAttrChangedConnect(lua_State* ls) {
        int idx = -1; std::string key;
        if (lua_istable(ls, 1)) {
            lua_getfield(ls, 1, "__partIndex");
            if (lua_isnumber(ls, -1)) idx = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
            lua_getfield(ls, 1, "__attrKey");
            if (lua_isstring(ls, -1)) key = lua_tostring(ls, -1);
            lua_pop(ls, 1);
        }
        int fidx = fnArgIndex(ls);
        if (!fidx || idx < 0 || key.empty()) return 0;
        lua_pushvalue(ls, fidx);
        scriptControls().attributeChanged[std::to_string(idx) + ":" + key].push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaPartGetAttributeChangedSignal(lua_State* ls) {
        int idx = partIndexFromSelf(ls, 1);
        const char* key = luaL_checkstring(ls, 2);
        lua_newtable(ls);
        lua_pushinteger(ls, idx); lua_setfield(ls, -2, "__partIndex");
        lua_pushstring(ls, key); lua_setfield(ls, -2, "__attrKey");
        blockyPushCFunction(ls, luaAttrChangedConnect); lua_setfield(ls, -2, "Connect");
        return 1;
    }

    static int luaPlayerRemovingConnect(lua_State* ls) {
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "PlayerRemoving:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().playerRemoving.push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaPlayersGetUserIdFromNameAsync(lua_State* ls) {
        (void)luaL_optstring(ls, lua_istable(ls, 1) ? 2 : 1, "Player");
        lua_pushinteger(ls, 1);
        return 1;
    }
    static int luaPlayerKick(lua_State* ls) {
        const char* reason = luaL_optstring(ls, lua_istable(ls, 1) ? 2 : 1, "");
        std::fprintf(stderr, "[Players] Kick (no-op locale): %s\n", reason);
        return 0;
    }

    static int luaInputBeganConnect(lua_State* ls) {
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "InputBegan:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().inputBegan.push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaInputEndedConnect(lua_State* ls) {
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "InputEnded:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().inputEnded.push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaInputGetMouseLocation(lua_State* ls) {
        auto* rt = fromState(ls);
        double x = 0, y = 0;
        if (rt && rt->window) glfwGetCursorPos(rt->window, &x, &y);
        lua_newtable(ls);
        lua_pushnumber(ls, x); lua_setfield(ls, -2, "X");
        lua_pushnumber(ls, y); lua_setfield(ls, -2, "Y");
        return 1;
    }
    static int luaInputIsGamepadEnabled(lua_State* ls) {
        lua_pushboolean(ls, 0);
        return 1;
    }
    static int luaInputGetKeysPressed(lua_State* ls) { lua_newtable(ls); return 1; }
    static int luaInputGetMouseButtonsPressed(lua_State* ls) { lua_newtable(ls); return 1; }
    static int luaInputGetLastInputType(lua_State* ls) {
        lua_pushstring(ls, "Keyboard");
        return 1;
    }

    static int luaBindToRenderStep(lua_State* ls) {
        int base = lua_istable(ls, 1) ? 2 : 1;
        const char* name = luaL_checkstring(ls, base);
        int prio = (int)luaL_optinteger(ls, base + 1, 2000);
        if (!lua_isfunction(ls, base + 2))
            return blockyLuaError(ls, "BindToRenderStep(name, priority, function)");
        auto& rs = scriptControls().renderSteps;
        for (auto it = rs.begin(); it != rs.end(); ++it) {
            if (it->name == name) {
                if (it->fnRef != -1) lua_unref(ls, it->fnRef);
                rs.erase(it);
                break;
            }
        }
        ScriptControls::RenderStepBind b;
        b.name = name; b.priority = prio;
        lua_pushvalue(ls, base + 2);
        b.fnRef = blockyLuaRef(ls);
        rs.push_back(b);
        std::sort(rs.begin(), rs.end(), [](const ScriptControls::RenderStepBind& a,
                                           const ScriptControls::RenderStepBind& c) {
            return a.priority < c.priority;
        });
        return 0;
    }
    static int luaUnbindFromRenderStep(lua_State* ls) {
        int base = lua_istable(ls, 1) ? 2 : 1;
        const char* name = luaL_checkstring(ls, base);
        auto& rs = scriptControls().renderSteps;
        for (auto it = rs.begin(); it != rs.end(); ++it) {
            if (it->name == name) {
                if (it->fnRef != -1) lua_unref(ls, it->fnRef);
                rs.erase(it);
                break;
            }
        }
        return 0;
    }

    static int luaDebrisAddItemTimed(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return 0;
        int selfOff = lua_istable(ls, 1) ? 1 : 0;
        float life = (float)luaL_optnumber(ls, selfOff + 2, 1.0);
        if (life < 0.f) life = 0.f;
        if (life > 120.f) life = 120.f;
        int pi = -1;
        if (lua_istable(ls, selfOff + 1)) {
            lua_getfield(ls, selfOff + 1, "__partIndex");
            if (lua_isnumber(ls, -1)) pi = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
            if (pi < 0) {
                lua_getfield(ls, selfOff + 1, "_partIdx");
                if (lua_isnumber(ls, -1)) pi = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
        }
        if (pi >= 0) {
            ScriptControls::DebrisItem d;
            d.partIndex = pi;
            d.remaining = life;
            scriptControls().debris.push_back(d);
        }
        return 0;
    }
    static int luaDebrisSetMaxItems(lua_State* ls) {
        (void)luaL_optinteger(ls, lua_istable(ls, 1) ? 2 : 1, 1000);
        return 0;
    }

    static int luaCollectionTagAddedConnect(lua_State* ls) {
        std::string tag = "Tag";
        if (lua_istable(ls, 1)) tag = readStringField(ls, 1, "_tag", "Tag");
        int fidx = fnArgIndex(ls);
        if (!fidx) return 0;
        lua_pushvalue(ls, fidx);
        scriptControls().tagAdded[tag].push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaCollectionTagRemovedConnect(lua_State* ls) {
        std::string tag = "Tag";
        if (lua_istable(ls, 1)) tag = readStringField(ls, 1, "_tag", "Tag");
        int fidx = fnArgIndex(ls);
        if (!fidx) return 0;
        lua_pushvalue(ls, fidx);
        scriptControls().tagRemoved[tag].push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaCollectionGetInstanceAddedSignal(lua_State* ls) {
        const char* tag = luaL_checkstring(ls, lua_istable(ls, 1) ? 2 : 1);
        lua_newtable(ls);
        lua_pushstring(ls, tag); lua_setfield(ls, -2, "_tag");
        blockyPushCFunction(ls, luaCollectionTagAddedConnect); lua_setfield(ls, -2, "Connect");
        return 1;
    }
    static int luaCollectionGetInstanceRemovedSignal(lua_State* ls) {
        const char* tag = luaL_checkstring(ls, lua_istable(ls, 1) ? 2 : 1);
        lua_newtable(ls);
        lua_pushstring(ls, tag); lua_setfield(ls, -2, "_tag");
        blockyPushCFunction(ls, luaCollectionTagRemovedConnect); lua_setfield(ls, -2, "Connect");
        return 1;
    }

    static int luaWorkspaceGetGravity(lua_State* ls) {
        lua_pushnumber(ls, scriptControls().workspaceGravity);
        return 1;
    }
    static int luaWorkspaceSetGravity(lua_State* ls) {
        float g = (float)luaL_optnumber(ls, lua_istable(ls, 1) ? 2 : 1, 196.2);
        scriptControls().workspaceGravity = std::clamp(g, 0.f, 1000.f);
        return 0;
    }

    static int luaModelGetPivot(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) { lua_pushnil(ls); return 1; }
        pushVec3(ls, m->position.x, m->position.y, m->position.z);
        return 1;
    }
    static int luaModelPivotTo(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        glm::vec3 pos = m->position;
        readVec3Arg(ls, 2, pos);
        m->position = pos;
        return 0;
    }
    static int luaModelIsA(lua_State* ls) {
        const char* cls = luaL_checkstring(ls, 2);
        lua_pushboolean(ls, cls && (std::strcmp(cls, "Model") == 0 || std::strcmp(cls, "Instance") == 0) ? 1 : 0);
        return 1;
    }
'''
lines.extend(rest.splitlines())

# Ease functions + materials + stubs for line count
easings = ["Linear", "Sine", "Back", "Quad", "Quart", "Quint", "Bounce", "Elastic", "Exponential", "Circular", "Cubic"]
for e in easings:
    add(f"    static float ease{e}(float t, bool out) {{")
    add("        t = std::clamp(t, 0.f, 1.f);")
    if e == "Linear":
        add("        (void)out; return t;")
    elif e == "Sine":
        add("        return out ? std::sin(t * 1.5707963f) : (1.f - std::cos(t * 1.5707963f));")
    elif e == "Quad":
        add("        return out ? 1.f - (1.f - t) * (1.f - t) : t * t;")
    elif e == "Cubic":
        add("        return out ? 1.f - (1.f - t) * (1.f - t) * (1.f - t) : t * t * t;")
    else:
        add("        (void)out; return t * t * (3.f - 2.f * t);")
    add("    }")

add("    static int luaTweenEval(lua_State* ls) {")
add('        const char* style = luaL_optstring(ls, 1, "Quad");')
add('        const char* dir = luaL_optstring(ls, 2, "Out");')
add("        float t = (float)luaL_optnumber(ls, 3, 0);")
add('        bool out = !dir || std::strcmp(dir, "In") != 0;')
add("        float r = t;")
for e in easings:
    add(f'        if (std::strcmp(style, "{e}") == 0) r = ease{e}(t, out);')
add("        lua_pushnumber(ls, r);")
add("        return 1;")
add("    }")

mats = ["Plastic","Wood","Slate","Concrete","CorrodedMetal","DiamondPlate","Foil","Grass","Ice",
        "Marble","Granite","Brick","Pebble","Sand","Fabric","SmoothPlastic","Metal","WoodPlanks",
        "Cobblestone","Neon","Glass","ForceField","Asphalt","Basalt","CrackedLava","Glacier",
        "Ground","LeafyGrass","Limestone","Mud","Pavement","Rock","Salt","Sandstone","Snow"]
for m in mats:
    add(f"    static int luaMaterialName{m}(lua_State* ls) {{")
    add(f'        lua_pushstring(ls, "{m}");')
    add("        return 1;")
    add("    }")

stubs = [
    "TextServiceGetTextSize", "PolicyServiceGetPolicy", "VoiceChatIsEnabled",
    "SocialServicePromptInvite", "GroupServiceGetInfo", "BadgeServiceHasBadge",
    "PointsServiceAward", "InsertServiceLoadAsset", "AssetServiceCreatePlace",
    "ContentProviderPreload", "KeyframeSeqGet", "ControllerServiceCreate",
    "HapticIsVibration", "GuiServiceGetInset", "LocalizationGetTranslator",
    "NotificationServiceDisplay", "MemoryStoreGet", "MessagingServicePublish",
    "DataStoreGetOrdered", "AvatarEditorGetCatalog", "CaptureServiceTakeCapture",
    "VoiceChatJoin", "ProximityPromptServiceEnable", "SharedTableRegistryGet",
]
for s in stubs:
    add(f"    static int luaStub{s}(lua_State* ls) {{")
    add(f"        // stub {s}")
    add("        (void)ls;")
    add("        lua_pushnil(ls);")
    add("        return 1;")
    add("    }")

# register + tick
reg = r'''
    void registerExpandApis(lua_State* L) {
        lua_getglobal(L, "UserInputService");
        if (lua_istable(L, -1)) {
            lua_newtable(L);
            blockyPushCFunction(L, luaInputBeganConnect);
            lua_setfield(L, -2, "Connect");
            lua_setfield(L, -2, "InputBegan");
            lua_newtable(L);
            blockyPushCFunction(L, luaInputEndedConnect);
            lua_setfield(L, -2, "Connect");
            lua_setfield(L, -2, "InputEnded");
            blockyPushCFunction(L, luaInputGetMouseLocation);
            lua_setfield(L, -2, "GetMouseLocation");
            blockyPushCFunction(L, luaInputIsGamepadEnabled);
            lua_setfield(L, -2, "IsGamepadEnabled");
            blockyPushCFunction(L, luaInputGetKeysPressed);
            lua_setfield(L, -2, "GetKeysPressed");
            blockyPushCFunction(L, luaInputGetMouseButtonsPressed);
            lua_setfield(L, -2, "GetMouseButtonsPressed");
            blockyPushCFunction(L, luaInputGetLastInputType);
            lua_setfield(L, -2, "GetLastInputType");
        }
        lua_pop(L, 1);

        lua_getglobal(L, "game");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "RunService");
            if (lua_istable(L, -1)) {
                blockyPushCFunction(L, luaBindToRenderStep);
                lua_setfield(L, -2, "BindToRenderStep");
                blockyPushCFunction(L, luaUnbindFromRenderStep);
                lua_setfield(L, -2, "UnbindFromRenderStep");
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "Players");
            if (lua_istable(L, -1)) {
                lua_newtable(L);
                blockyPushCFunction(L, luaPlayerRemovingConnect);
                lua_setfield(L, -2, "Connect");
                lua_setfield(L, -2, "PlayerRemoving");
                blockyPushCFunction(L, luaPlayersGetUserIdFromNameAsync);
                lua_setfield(L, -2, "GetUserIdFromNameAsync");
                lua_getfield(L, -1, "LocalPlayer");
                if (lua_istable(L, -1)) {
                    lua_pushinteger(L, 1); lua_setfield(L, -2, "UserId");
                    lua_pushstring(L, "Player"); lua_setfield(L, -2, "DisplayName");
                    blockyPushCFunction(L, luaPlayerKick); lua_setfield(L, -2, "Kick");
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "Debris");
            if (lua_istable(L, -1)) {
                blockyPushCFunction(L, luaDebrisAddItemTimed);
                lua_setfield(L, -2, "AddItem");
                blockyPushCFunction(L, luaDebrisSetMaxItems);
                lua_setfield(L, -2, "SetMaxItems");
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "CollectionService");
            if (lua_istable(L, -1)) {
                blockyPushCFunction(L, luaCollectionGetInstanceAddedSignal);
                lua_setfield(L, -2, "GetInstanceAddedSignal");
                blockyPushCFunction(L, luaCollectionGetInstanceRemovedSignal);
                lua_setfield(L, -2, "GetInstanceRemovedSignal");
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_getglobal(L, "workspace");
        if (lua_istable(L, -1)) {
            blockyPushCFunction(L, luaWorkspaceGetGravity);
            lua_setfield(L, -2, "GetGravity");
            blockyPushCFunction(L, luaWorkspaceSetGravity);
            lua_setfield(L, -2, "SetGravity");
            lua_pushnumber(L, scriptControls().workspaceGravity);
            lua_setfield(L, -2, "Gravity");
        }
        lua_pop(L, 1);

        lua_newtable(L);
        blockyPushCFunction(L, luaTweenEval);
        lua_setfield(L, -2, "Evaluate");
        lua_setglobal(L, "TweenEasing");
    }

    void tickDebris(float dt) {
        auto& d = scriptControls().debris;
        if (!workspace || d.empty()) return;
        for (size_t i = 0; i < d.size();) {
            d[i].remaining -= dt;
            if (d[i].remaining > 0.f) { ++i; continue; }
            int pi = d[i].partIndex;
            d.erase(d.begin() + (int)i);
            if (pi >= 0 && pi < (int)workspace->parts.size())
                pendingDestroy_.push_back(pi);
        }
    }

    void tickRenderSteps(float dt) {
        if (!L) return;
        for (auto& b : scriptControls().renderSteps) {
            if (b.fnRef == -1) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, b.fnRef);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            lua_pushnumber(L, dt);
            callStart_ = std::chrono::steady_clock::now();
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "BindToRenderStep(%s): %s\n", b.name.c_str(), lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }
'''
lines.extend(reg.splitlines())

out = Path("src/engine/LuaRuntimeExpand.inl")
out.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"wrote {out} ({len(lines)} lines)")
