// Expanded Luau API surface — included inside LuaRuntime class
// Instance.new extras + Part/Players/UIS/RunService/Debris helpers

    static int tryInstanceNewExpand(lua_State* ls, const char* className) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace || !className) return 0;

        if (std::strcmp(className, "IntValue") == 0) {
            const char* nm = luaL_optstring(ls, 2, "IntValue");
            lua_newtable(ls);
            lua_pushstring(ls, "IntValue"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushnumber(ls, 0); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "NumberValue") == 0) {
            const char* nm = luaL_optstring(ls, 2, "NumberValue");
            lua_newtable(ls);
            lua_pushstring(ls, "NumberValue"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushnumber(ls, 0); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "StringValue") == 0) {
            const char* nm = luaL_optstring(ls, 2, "StringValue");
            lua_newtable(ls);
            lua_pushstring(ls, "StringValue"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, ""); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "BoolValue") == 0) {
            const char* nm = luaL_optstring(ls, 2, "BoolValue");
            lua_newtable(ls);
            lua_pushstring(ls, "BoolValue"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 0); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "ObjectValue") == 0) {
            const char* nm = luaL_optstring(ls, 2, "ObjectValue");
            lua_newtable(ls);
            lua_pushstring(ls, "ObjectValue"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushnil(ls); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "CFrameValue") == 0) {
            const char* nm = luaL_optstring(ls, 2, "CFrameValue");
            lua_newtable(ls);
            lua_pushstring(ls, "CFrameValue"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushnil(ls); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "Color3Value") == 0) {
            const char* nm = luaL_optstring(ls, 2, "Color3Value");
            lua_newtable(ls);
            lua_pushstring(ls, "Color3Value"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushnil(ls); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "Vector3Value") == 0) {
            const char* nm = luaL_optstring(ls, 2, "Vector3Value");
            lua_newtable(ls);
            lua_pushstring(ls, "Vector3Value"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushnil(ls); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "BrickColorValue") == 0) {
            const char* nm = luaL_optstring(ls, 2, "BrickColorValue");
            lua_newtable(ls);
            lua_pushstring(ls, "BrickColorValue"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm); lua_setfield(ls, -2, "Name");
            lua_pushnil(ls); lua_setfield(ls, -2, "_value");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaValueObjectIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaValueObjectNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "ScreenGui") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "ScreenGui");
            lab.className = "Frame";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.0f;
            lab.sizeW = 200; lab.sizeH = 100;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "Frame") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "Frame");
            lab.className = "Frame";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            lab.sizeW = 200; lab.sizeH = 100;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "ScrollingFrame") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "ScrollingFrame");
            lab.className = "ScrollingFrame";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "TextLabel") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "TextLabel");
            lab.className = "TextLabel";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "TextButton") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "TextButton");
            lab.className = "TextButton";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "ImageLabel") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "ImageLabel");
            lab.className = "ImageLabel";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "ImageButton") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "ImageButton");
            lab.className = "TextButton";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "TextBox") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "TextBox");
            lab.className = "TextLabel";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "ViewportFrame") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "ViewportFrame");
            lab.className = "Frame";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            lab.sizeW = 200; lab.sizeH = 100;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "CanvasGroup") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "CanvasGroup");
            lab.className = "Frame";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            lab.sizeW = 200; lab.sizeH = 100;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "Model") == 0) {
            WorkspaceModel m;
            m.name = luaL_optstring(ls, 2, "Model");
            m.position = {0, 0, 0};
            rt->workspace->models.push_back(std::move(m));
            pushLiveModel(ls, (int)rt->workspace->models.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "SpawnLocation") == 0) {
            WorkspacePart p;
            p.name = luaL_optstring(ls, 2, "SpawnLocation");
            p.size = {6, 1, 6};
            p.color = {0.2f, 0.8f, 0.4f};
            p.position = rt->workspace->spawn.position;
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "SpawnLocation"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "Seat") == 0) {
            WorkspacePart p; p.name = luaL_optstring(ls, 2, "Seat");
            p.size = {4, 1, 2}; p.color = {0.75f, 0.55f, 0.2f};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "Seat"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "VehicleSeat") == 0) {
            WorkspacePart p; p.name = luaL_optstring(ls, 2, "VehicleSeat");
            p.size = {4, 1, 2}; p.color = {0.75f, 0.55f, 0.2f};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "VehicleSeat"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "PointLight") == 0 || std::strcmp(className, "SpotLight") == 0
            || std::strcmp(className, "SurfaceLight") == 0) {
            int partIdx = -1;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) partIdx = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            if (partIdx >= 0 && partIdx < (int)rt->workspace->parts.size()) {
                rt->workspace->parts[partIdx].pointLight = true;
                rt->workspace->parts[partIdx].lightRange = 16.f;
                rt->workspace->parts[partIdx].lightBrightness = 1.f;
            }
            lua_newtable(ls);
            lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, className)); lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, partIdx); lua_setfield(ls, -2, "__partIndex");
            lua_pushnumber(ls, 16); lua_setfield(ls, -2, "Range");
            lua_pushnumber(ls, 1); lua_setfield(ls, -2, "Brightness");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            return 1;
        }
        if (std::strcmp(className, "Fire") == 0 || std::strcmp(className, "Smoke") == 0
            || std::strcmp(className, "Sparkles") == 0) {
            ScriptParticleEmitter e;
            e.rate = (std::strcmp(className, "Smoke") == 0) ? 6.f : 14.f;
            e.preset = (std::strcmp(className, "Smoke") == 0) ? (int)ParticlePreset::Smoke : (int)ParticlePreset::Sparks;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) e.partIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->scriptEmitters_.push_back(e);
            lua_newtable(ls);
            lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, className)); lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, (int)rt->scriptEmitters_.size() - 1); lua_setfield(ls, -2, "__emitterIndex");
            blockyPushCFunction(ls, luaParticleEmitterEmit); lua_setfield(ls, -2, "Emit");
            return 1;
        }
        if (std::strcmp(className, "Attachment") == 0) {
            ScriptControls::ScriptAttachment a;
            a.name = luaL_optstring(ls, 2, "Attachment");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) a.partIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            scriptControls().attachments.push_back(a);
            int ai = (int)scriptControls().attachments.size() - 1;
            lua_newtable(ls);
            lua_pushstring(ls, "Attachment"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, a.name.c_str()); lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, ai); lua_setfield(ls, -2, "__attachmentIndex");
            return 1;
        }
        if (std::strcmp(className, "Beam") == 0) {
            ScriptControls::ScriptBeam b;
            b.name = luaL_optstring(ls, 2, "Beam");
            scriptControls().beams.push_back(b);
            int bi = (int)scriptControls().beams.size() - 1;
            lua_newtable(ls);
            lua_pushstring(ls, "Beam"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, b.name.c_str()); lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, bi); lua_setfield(ls, -2, "__beamIndex");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaBeamNewIndex); lua_setfield(ls, -2, "__newindex");
            blockyPushCFunction(ls, luaBeamIndex); lua_setfield(ls, -2, "__index");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "Configuration") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "Configuration"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "Configuration")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "Highlight") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "Highlight"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "Highlight")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "SelectionBox") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "SelectionBox"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "SelectionBox")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "SelectionSphere") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "SelectionSphere"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "SelectionSphere")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "BoxHandleAdornment") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "BoxHandleAdornment"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "BoxHandleAdornment")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "ConeHandleAdornment") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "ConeHandleAdornment"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "ConeHandleAdornment")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "LineHandleAdornment") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "LineHandleAdornment"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "LineHandleAdornment")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "CylinderHandleAdornment") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "CylinderHandleAdornment"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "CylinderHandleAdornment")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "SphereHandleAdornment") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "SphereHandleAdornment"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "SphereHandleAdornment")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "WireframeHandleAdornment") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "WireframeHandleAdornment"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "WireframeHandleAdornment")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "Accessory") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "Accessory"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "Accessory")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "Hat") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "Hat"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "Hat")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "Pants") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "Pants"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "Pants")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "Shirt") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "Shirt"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "Shirt")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "ShirtGraphic") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "ShirtGraphic"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "ShirtGraphic")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "BodyColors") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "BodyColors"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "BodyColors")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "CharacterMesh") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "CharacterMesh"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "CharacterMesh")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "SpecialMesh") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "SpecialMesh"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "SpecialMesh")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "BlockMesh") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "BlockMesh"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "BlockMesh")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "CylinderMesh") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "CylinderMesh"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "CylinderMesh")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "FileMesh") == 0) {
            lua_newtable(ls); lua_pushstring(ls, "FileMesh"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "FileMesh")); lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "CornerWedgePart") == 0) {
            WorkspacePart p;
            p.name = luaL_optstring(ls, 2, "CornerWedgePart");
            p.size = {4, 2, 4};
            p.shape = PartShape::Wedge;
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "CornerWedgePart"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "TrussPart") == 0) {
            WorkspacePart p;
            p.name = luaL_optstring(ls, 2, "TrussPart");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "TrussPart"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "MeshPart") == 0) {
            WorkspacePart p;
            p.name = luaL_optstring(ls, 2, "MeshPart");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "MeshPart"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "UnionOperation") == 0) {
            WorkspacePart p;
            p.name = luaL_optstring(ls, 2, "UnionOperation");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "UnionOperation"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "NegateOperation") == 0) {
            WorkspacePart p;
            p.name = luaL_optstring(ls, 2, "NegateOperation");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "NegateOperation"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "IntersectOperation") == 0) {
            WorkspacePart p;
            p.name = luaL_optstring(ls, 2, "IntersectOperation");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "IntersectOperation"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        return 0;
    }


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
        (void)fromState(ls);
        double x = 0, y = 0;
        if (GLFWwindow* win = glfwGetCurrentContext())
            glfwGetCursorPos(win, &x, &y);
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
    static float easeLinear(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        (void)out; return t;
    }
    static float easeSine(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        return out ? std::sin(t * 1.5707963f) : (1.f - std::cos(t * 1.5707963f));
    }
    static float easeBack(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        (void)out; return t * t * (3.f - 2.f * t);
    }
    static float easeQuad(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        return out ? 1.f - (1.f - t) * (1.f - t) : t * t;
    }
    static float easeQuart(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        (void)out; return t * t * (3.f - 2.f * t);
    }
    static float easeQuint(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        (void)out; return t * t * (3.f - 2.f * t);
    }
    static float easeBounce(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        (void)out; return t * t * (3.f - 2.f * t);
    }
    static float easeElastic(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        (void)out; return t * t * (3.f - 2.f * t);
    }
    static float easeExponential(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        (void)out; return t * t * (3.f - 2.f * t);
    }
    static float easeCircular(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        (void)out; return t * t * (3.f - 2.f * t);
    }
    static float easeCubic(float t, bool out) {
        t = std::clamp(t, 0.f, 1.f);
        return out ? 1.f - (1.f - t) * (1.f - t) * (1.f - t) : t * t * t;
    }
    static int luaTweenEval(lua_State* ls) {
        const char* style = luaL_optstring(ls, 1, "Quad");
        const char* dir = luaL_optstring(ls, 2, "Out");
        float t = (float)luaL_optnumber(ls, 3, 0);
        bool out = !dir || std::strcmp(dir, "In") != 0;
        float r = t;
        if (std::strcmp(style, "Linear") == 0) r = easeLinear(t, out);
        if (std::strcmp(style, "Sine") == 0) r = easeSine(t, out);
        if (std::strcmp(style, "Back") == 0) r = easeBack(t, out);
        if (std::strcmp(style, "Quad") == 0) r = easeQuad(t, out);
        if (std::strcmp(style, "Quart") == 0) r = easeQuart(t, out);
        if (std::strcmp(style, "Quint") == 0) r = easeQuint(t, out);
        if (std::strcmp(style, "Bounce") == 0) r = easeBounce(t, out);
        if (std::strcmp(style, "Elastic") == 0) r = easeElastic(t, out);
        if (std::strcmp(style, "Exponential") == 0) r = easeExponential(t, out);
        if (std::strcmp(style, "Circular") == 0) r = easeCircular(t, out);
        if (std::strcmp(style, "Cubic") == 0) r = easeCubic(t, out);
        lua_pushnumber(ls, r);
        return 1;
    }
    static int luaMaterialNamePlastic(lua_State* ls) {
        lua_pushstring(ls, "Plastic");
        return 1;
    }
    static int luaMaterialNameWood(lua_State* ls) {
        lua_pushstring(ls, "Wood");
        return 1;
    }
    static int luaMaterialNameSlate(lua_State* ls) {
        lua_pushstring(ls, "Slate");
        return 1;
    }
    static int luaMaterialNameConcrete(lua_State* ls) {
        lua_pushstring(ls, "Concrete");
        return 1;
    }
    static int luaMaterialNameCorrodedMetal(lua_State* ls) {
        lua_pushstring(ls, "CorrodedMetal");
        return 1;
    }
    static int luaMaterialNameDiamondPlate(lua_State* ls) {
        lua_pushstring(ls, "DiamondPlate");
        return 1;
    }
    static int luaMaterialNameFoil(lua_State* ls) {
        lua_pushstring(ls, "Foil");
        return 1;
    }
    static int luaMaterialNameGrass(lua_State* ls) {
        lua_pushstring(ls, "Grass");
        return 1;
    }
    static int luaMaterialNameIce(lua_State* ls) {
        lua_pushstring(ls, "Ice");
        return 1;
    }
    static int luaMaterialNameMarble(lua_State* ls) {
        lua_pushstring(ls, "Marble");
        return 1;
    }
    static int luaMaterialNameGranite(lua_State* ls) {
        lua_pushstring(ls, "Granite");
        return 1;
    }
    static int luaMaterialNameBrick(lua_State* ls) {
        lua_pushstring(ls, "Brick");
        return 1;
    }
    static int luaMaterialNamePebble(lua_State* ls) {
        lua_pushstring(ls, "Pebble");
        return 1;
    }
    static int luaMaterialNameSand(lua_State* ls) {
        lua_pushstring(ls, "Sand");
        return 1;
    }
    static int luaMaterialNameFabric(lua_State* ls) {
        lua_pushstring(ls, "Fabric");
        return 1;
    }
    static int luaMaterialNameSmoothPlastic(lua_State* ls) {
        lua_pushstring(ls, "SmoothPlastic");
        return 1;
    }
    static int luaMaterialNameMetal(lua_State* ls) {
        lua_pushstring(ls, "Metal");
        return 1;
    }
    static int luaMaterialNameWoodPlanks(lua_State* ls) {
        lua_pushstring(ls, "WoodPlanks");
        return 1;
    }
    static int luaMaterialNameCobblestone(lua_State* ls) {
        lua_pushstring(ls, "Cobblestone");
        return 1;
    }
    static int luaMaterialNameNeon(lua_State* ls) {
        lua_pushstring(ls, "Neon");
        return 1;
    }
    static int luaMaterialNameGlass(lua_State* ls) {
        lua_pushstring(ls, "Glass");
        return 1;
    }
    static int luaMaterialNameForceField(lua_State* ls) {
        lua_pushstring(ls, "ForceField");
        return 1;
    }
    static int luaMaterialNameAsphalt(lua_State* ls) {
        lua_pushstring(ls, "Asphalt");
        return 1;
    }
    static int luaMaterialNameBasalt(lua_State* ls) {
        lua_pushstring(ls, "Basalt");
        return 1;
    }
    static int luaMaterialNameCrackedLava(lua_State* ls) {
        lua_pushstring(ls, "CrackedLava");
        return 1;
    }
    static int luaMaterialNameGlacier(lua_State* ls) {
        lua_pushstring(ls, "Glacier");
        return 1;
    }
    static int luaMaterialNameGround(lua_State* ls) {
        lua_pushstring(ls, "Ground");
        return 1;
    }
    static int luaMaterialNameLeafyGrass(lua_State* ls) {
        lua_pushstring(ls, "LeafyGrass");
        return 1;
    }
    static int luaMaterialNameLimestone(lua_State* ls) {
        lua_pushstring(ls, "Limestone");
        return 1;
    }
    static int luaMaterialNameMud(lua_State* ls) {
        lua_pushstring(ls, "Mud");
        return 1;
    }
    static int luaMaterialNamePavement(lua_State* ls) {
        lua_pushstring(ls, "Pavement");
        return 1;
    }
    static int luaMaterialNameRock(lua_State* ls) {
        lua_pushstring(ls, "Rock");
        return 1;
    }
    static int luaMaterialNameSalt(lua_State* ls) {
        lua_pushstring(ls, "Salt");
        return 1;
    }
    static int luaMaterialNameSandstone(lua_State* ls) {
        lua_pushstring(ls, "Sandstone");
        return 1;
    }
    static int luaMaterialNameSnow(lua_State* ls) {
        lua_pushstring(ls, "Snow");
        return 1;
    }
    static int luaStubTextServiceGetTextSize(lua_State* ls) {
        // stub TextServiceGetTextSize
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubPolicyServiceGetPolicy(lua_State* ls) {
        // stub PolicyServiceGetPolicy
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubVoiceChatIsEnabled(lua_State* ls) {
        // stub VoiceChatIsEnabled
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubSocialServicePromptInvite(lua_State* ls) {
        // stub SocialServicePromptInvite
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubGroupServiceGetInfo(lua_State* ls) {
        // stub GroupServiceGetInfo
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubBadgeServiceHasBadge(lua_State* ls) {
        // stub BadgeServiceHasBadge
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubPointsServiceAward(lua_State* ls) {
        // stub PointsServiceAward
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubInsertServiceLoadAsset(lua_State* ls) {
        // stub InsertServiceLoadAsset
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubAssetServiceCreatePlace(lua_State* ls) {
        // stub AssetServiceCreatePlace
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubContentProviderPreload(lua_State* ls) {
        // stub ContentProviderPreload
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubKeyframeSeqGet(lua_State* ls) {
        // stub KeyframeSeqGet
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubControllerServiceCreate(lua_State* ls) {
        // stub ControllerServiceCreate
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubHapticIsVibration(lua_State* ls) {
        // stub HapticIsVibration
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubGuiServiceGetInset(lua_State* ls) {
        // stub GuiServiceGetInset
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubLocalizationGetTranslator(lua_State* ls) {
        // stub LocalizationGetTranslator
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubNotificationServiceDisplay(lua_State* ls) {
        // stub NotificationServiceDisplay
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubMemoryStoreGet(lua_State* ls) {
        // stub MemoryStoreGet
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubMessagingServicePublish(lua_State* ls) {
        // stub MessagingServicePublish
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubDataStoreGetOrdered(lua_State* ls) {
        // stub DataStoreGetOrdered
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubAvatarEditorGetCatalog(lua_State* ls) {
        // stub AvatarEditorGetCatalog
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubCaptureServiceTakeCapture(lua_State* ls) {
        // stub CaptureServiceTakeCapture
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubVoiceChatJoin(lua_State* ls) {
        // stub VoiceChatJoin
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubProximityPromptServiceEnable(lua_State* ls) {
        // stub ProximityPromptServiceEnable
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }
    static int luaStubSharedTableRegistryGet(lua_State* ls) {
        // stub SharedTableRegistryGet
        (void)ls;
        lua_pushnil(ls);
        return 1;
    }

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
