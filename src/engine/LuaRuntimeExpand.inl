// Expanded Luau API surface — included inside LuaRuntime class
// Instance.new extras + Part/Players/UIS/RunService/Debris helpers

    static bool luaReadRgb(lua_State* ls, int idx, float& r, float& g, float& b) {
        if (!lua_istable(ls, idx)) return false;
        auto comp = [&](const char* a, const char* b, float& o) {
            lua_getfield(ls, idx, a);
            if (!lua_isnumber(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, idx, b); }
            if (lua_isnumber(ls, -1)) o = (float)lua_tonumber(ls, -1);
            lua_pop(ls, 1);
        };
        comp("r", "R", r);
        comp("g", "G", g);
        comp("b", "B", b);
        return true;
    }

    static WorkspacePart* luaAdornPart(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_getfield(ls, 1, "__partIndex");
        const int i = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        if (!rt || !rt->workspace || i < 0 || i >= (int)rt->workspace->parts.size()) return nullptr;
        return &rt->workspace->parts[(size_t)i];
    }

    static int luaFaceFromValue(lua_State* ls, int idx) {
        if (lua_isnumber(ls, idx)) return std::clamp((int)lua_tointeger(ls, idx), 0, 5);
        const char* s = lua_tostring(ls, idx);
        if (!s) return 5;
        if (std::strcmp(s, "Right") == 0) return 0;
        if (std::strcmp(s, "Top") == 0) return 1;
        if (std::strcmp(s, "Back") == 0) return 2;
        if (std::strcmp(s, "Left") == 0) return 3;
        if (std::strcmp(s, "Bottom") == 0) return 4;
        return 5;
    }

    static int luaAdornIndex(lua_State* ls) {
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        return 1;
    }

    static int luaAdornNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key) return 0;
        lua_getfield(ls, 1, "__adornKind");
        const int kind = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        if (std::strcmp(key, "Parent") == 0 || std::strcmp(key, "Adornee") == 0) {
            int i = -1;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) i = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            lua_pushstring(ls, "__partIndex");
            lua_pushinteger(ls, i);
            lua_rawset(ls, 1);
        }
        lua_pushvalue(ls, 2);
        lua_pushvalue(ls, 3);
        lua_rawset(ls, 1);
        WorkspacePart* p = luaAdornPart(ls);
        if (!p) return 0;
        lua_getfield(ls, 1, "Enabled");
        const bool en = lua_isnil(ls, -1) ? true : lua_toboolean(ls, -1) != 0;
        lua_pop(ls, 1);
        if (kind == 0) p->highlightEnabled = en;
        else if (kind == 1) p->selectionBox = en;
        else if (kind == 2) p->selectionSphere = en;
        else if (kind == 3) p->dragDetector = en;
        else if (kind == 4) p->decalEnabled = en;
        if (kind == 0) {
            float r = p->highlightR, g = p->highlightG, b = p->highlightB;
            lua_getfield(ls, 1, "FillColor");
            if (!lua_istable(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, 1, "OutlineColor"); }
            if (luaReadRgb(ls, -1, r, g, b)) { p->highlightR = r; p->highlightG = g; p->highlightB = b; }
            lua_pop(ls, 1);
        }
        if (kind == 4) {
            float r = p->decalR, g = p->decalG, b = p->decalB;
            lua_getfield(ls, 1, "Color3");
            if (!lua_istable(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, 1, "Color"); }
            if (luaReadRgb(ls, -1, r, g, b)) { p->decalR = r; p->decalG = g; p->decalB = b; }
            lua_pop(ls, 1);
            lua_getfield(ls, 1, "Face");
            if (!lua_isnil(ls, -1)) p->decalFace = (float)luaFaceFromValue(ls, -1);
            lua_pop(ls, 1);
            lua_getfield(ls, 1, "Transparency");
            if (lua_isnumber(ls, -1)) p->decalTransparency = std::clamp((float)lua_tonumber(ls, -1), 0.f, 1.f);
            lua_pop(ls, 1);
        }
        return 0;
    }

    static int luaMakeAdorn(lua_State* ls, const char* className, int kind) {
        lua_newtable(ls);
        lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");
        lua_pushstring(ls, luaL_optstring(ls, 2, className)); lua_setfield(ls, -2, "Name");
        lua_pushinteger(ls, kind); lua_setfield(ls, -2, "__adornKind");
        if (lua_istable(ls, 3)) {
            lua_getfield(ls, 3, "__partIndex");
            if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); }
            lua_pop(ls, 1);
        }
        lua_newtable(ls);
        blockyPushCFunction(ls, luaAdornNewIndex); lua_setfield(ls, -2, "__newindex");
        blockyPushCFunction(ls, luaAdornIndex); lua_setfield(ls, -2, "__index");
        lua_setmetatable(ls, -2);
        lua_getfield(ls, -1, "__partIndex");
        const int i = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        auto* rt = fromState(ls);
        if (rt && rt->workspace && i >= 0 && i < (int)rt->workspace->parts.size()) {
            WorkspacePart& p = rt->workspace->parts[(size_t)i];
            if (kind == 0) p.highlightEnabled = true;
            else if (kind == 1) p.selectionBox = true;
            else if (kind == 2) p.selectionSphere = true;
            else if (kind == 3) p.dragDetector = true;
            else if (kind == 4) p.decalEnabled = true;
        }
        return 1;
    }

    static void luaApplyLightFx(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->lighting) return;
        lua_getfield(ls, 1, "__fxKind");
        const int kind = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        lua_getfield(ls, 1, "Enabled");
        const bool en = lua_isnil(ls, -1) ? true : lua_toboolean(ls, -1) != 0;
        lua_pop(ls, 1);
        auto num = [&](const char* k, float def) {
            lua_getfield(ls, 1, k);
            const float v = lua_isnumber(ls, -1) ? (float)lua_tonumber(ls, -1) : def;
            lua_pop(ls, 1);
            return v;
        };
        LightingService& L = *rt->lighting;
        if (kind == 0) {
            L.ccEnabled = en;
            L.ccBrightness = num("Brightness", L.ccBrightness);
            L.ccContrast = num("Contrast", L.ccContrast);
            L.ccSaturation = num("Saturation", L.ccSaturation);
            float r = L.ccTint.r, g = L.ccTint.g, b = L.ccTint.b;
            lua_getfield(ls, 1, "TintColor");
            luaReadRgb(ls, -1, r, g, b);
            lua_pop(ls, 1);
            L.ccTint = {r, g, b};
        } else if (kind == 1) {
            L.bloomFx = en;
            L.bloomFxIntensity = num("Intensity", L.bloomFxIntensity);
            L.bloomFxThreshold = num("Threshold", L.bloomFxThreshold);
        } else if (kind == 2) {
            L.blurFx = en;
            L.blurFxSize = num("Size", L.blurFxSize);
        } else if (kind == 3) {
            L.dofFx = en;
            L.dofFar = num("FarIntensity", L.dofFar);
        } else if (kind == 4) {
            L.sunRaysEnabled = en;
            L.sunRaysIntensity = num("Intensity", L.sunRaysIntensity);
            L.sunRaysSpread = num("Spread", L.sunRaysSpread);
        }
        L.clampAll();
    }

    static int luaLightFxIndex(lua_State* ls) {
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        return 1;
    }
    static int luaLightFxNewIndex(lua_State* ls) {
        lua_pushvalue(ls, 2);
        lua_pushvalue(ls, 3);
        lua_rawset(ls, 1);
        luaApplyLightFx(ls);
        return 0;
    }
    static int luaMakeLightFx(lua_State* ls, const char* className, int kind) {
        lua_newtable(ls);
        lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");
        lua_pushstring(ls, luaL_optstring(ls, 2, className)); lua_setfield(ls, -2, "Name");
        lua_pushinteger(ls, kind); lua_setfield(ls, -2, "__fxKind");
        lua_newtable(ls);
        blockyPushCFunction(ls, luaLightFxNewIndex); lua_setfield(ls, -2, "__newindex");
        blockyPushCFunction(ls, luaLightFxIndex); lua_setfield(ls, -2, "__index");
        lua_setmetatable(ls, -2);
        return 1;
    }

    static int luaAttachmentNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        lua_getfield(ls, 1, "__attachmentIndex");
        const int ai = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        auto& at = scriptControls().attachments;
        if (key && ai >= 0 && ai < (int)at.size()) {
            auto& a = at[(size_t)ai];
            if (std::strcmp(key, "Position") == 0 && lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "X"); a.ox = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
                lua_getfield(ls, 3, "Y"); a.oy = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
                lua_getfield(ls, 3, "Z"); a.oz = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
            } else if (std::strcmp(key, "Parent") == 0 && lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) a.partIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
        }
        lua_pushvalue(ls, 2); lua_pushvalue(ls, 3); lua_rawset(ls, 1);
        return 0;
    }

    static int luaTrailNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        lua_getfield(ls, 1, "__trailIndex");
        const int ti = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        auto& tr = scriptControls().trails;
        if (key && ti >= 0 && ti < (int)tr.size()) {
            auto& t = tr[(size_t)ti];
            if (std::strcmp(key, "Enabled") == 0) t.enabled = lua_toboolean(ls, 3) != 0;
            else if (std::strcmp(key, "Lifetime") == 0) t.life = std::clamp((float)lua_tonumber(ls, 3), 0.05f, 8.f);
            else if (std::strcmp(key, "Width") == 0) t.width = std::clamp((float)lua_tonumber(ls, 3), 0.02f, 4.f);
            else if (std::strcmp(key, "Color") == 0 && lua_istable(ls, 3)) {
                float r = t.r, g = t.g, b = t.b;
                if (luaReadRgb(ls, 3, r, g, b)) { t.r = r; t.g = g; t.b = b; }
            } else if (std::strcmp(key, "Parent") == 0 && lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) t.partIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
        }
        lua_pushvalue(ls, 2); lua_pushvalue(ls, 3); lua_rawset(ls, 1);
        return 0;
    }

    static void luaPaintLimb(Humanoid& h, const char* name, const glm::vec3& c) {
        for (auto& p : h.parts)
            if (p.name == name) p.color = c;
    }

    /**
     * Di chi sono questi vestiti / BodyColors, guardando il Parent in pidx:
     * un NPC (Actors:Spawn) o il personaggio del giocatore (un Model con
     * Humanoid). Altrimenti (nessun Parent, workspace, una cartella) di nessuno:
     * i valori si tengono e si applicano quando arriva il Parent giusto.
     */
    static bool clothTarget(lua_State* ls, int pidx, int& npc) {
        npc = -1;
        if (!lua_istable(ls, pidx)) return false;
        pidx = lua_absindex(ls, pidx);
        lua_pushstring(ls, "__npc");
        lua_rawget(ls, pidx);
        if (lua_isnumber(ls, -1)) { npc = (int)lua_tointeger(ls, -1); lua_pop(ls, 1); return true; }
        lua_pop(ls, 1);
        for (const char* k : {"__partIndex", "__labelIndex"}) {
            lua_pushstring(ls, k);
            lua_rawget(ls, pidx);
            const bool engineThing = !lua_isnil(ls, -1);
            lua_pop(ls, 1);
            if (engineThing) return false;
        }
        bool hasHum = false;
        if (lua_getfield(ls, pidx, "Humanoid"), lua_istable(ls, -1)) hasHum = true;
        lua_pop(ls, 1);
        return hasHum;   // il Character del giocatore
    }


    static int luaClothesNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (key && std::strcmp(key, "Parent") == 0) {
            lua_pushvalue(ls, 2); lua_pushvalue(ls, 3); lua_rawset(ls, 1);
            int npc = -1;
            const bool attached = clothTarget(ls, 3, npc);
            lua_pushinteger(ls, npc); lua_setfield(ls, 1, "__npc");
            lua_pushboolean(ls, attached ? 1 : 0); lua_setfield(ls, 1, "__attached");
            if (!attached) return 0;
            // ora ha un corpo: si applica tutto quello scritto prima
            std::vector<std::string> keys;
            lua_pushnil(ls);
            while (lua_next(ls, 1) != 0) {
                if (lua_type(ls, -2) == LUA_TSTRING && lua_istable(ls, -1)) {
                    const char* k = lua_tostring(ls, -2);
                    if (k && std::strncmp(k, "__", 2) != 0 && std::strcmp(k, "Parent") != 0) keys.push_back(k);
                }
                lua_pop(ls, 1);
            }
            for (const auto& k : keys) {
                lua_getfield(ls, 1, k.c_str());
                luaClothApply(ls, 1, k.c_str(), lua_gettop(ls));
                lua_pop(ls, 1);
            }
            lua_getfield(ls, 1, "__cloth");
            const bool hat = lua_isnumber(ls, -1) && lua_tointeger(ls, -1) == 2;
            lua_pop(ls, 1);
            if (hat) luaClothApply(ls, 1, "__hatOn", 0);
            return 0;
        }
        lua_getfield(ls, 1, "__attached");
        const bool attached = lua_toboolean(ls, -1) != 0;
        lua_pop(ls, 1);
        if (key && attached) luaClothApply(ls, 1, key, 3);
        if (key) { lua_pushvalue(ls, 2); lua_pushvalue(ls, 3); lua_rawset(ls, 1); }
        return 0;
    }

    /** Applica key = valore (in vidx) al corpo dei vestiti in self. */
    static void luaClothApply(lua_State* ls, int self, const char* key, int vidx) {
        auto* rt = fromState(ls);
        lua_getfield(ls, self, "__cloth");
        const int kind = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : 0;
        lua_pop(ls, 1);
        lua_getfield(ls, self, "__npc");
        const int npc = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        Humanoid* hum = nullptr;
        if (rt && rt->workspace && npc >= 0 && npc < (int)rt->workspace->npcs.size())
            hum = &rt->workspace->npcs[(size_t)npc].humanoid;
        else if (rt && npc < 0) hum = rt->playerHumanoid;
        if (hum && std::strcmp(key, "__hatOn") == 0) { hum->hatOn = true; return; }
        // BodyColors: HeadColor3 (Color3, Roblox) come HeadColor
        std::string k = key;
        if (kind == 3 && k.size() > 6 && k.back() == '3') k.pop_back();
        key = k.c_str();
        if (vidx > 0 && hum && lua_istable(ls, vidx)) {
            Humanoid& h = *hum;
            float r = 1.f, g = 1.f, b = 1.f;
            if (luaReadRgb(ls, 3, r, g, b)) {
                const glm::vec3 c(r, g, b);
                if (kind == 0 && (std::strcmp(key, "Color3") == 0 || std::strcmp(key, "Color") == 0)) {
                    h.shirtOn = true; h.shirtColor = c;   // (Color3 della maglia)
                    luaPaintLimb(h, "Torso", c); luaPaintLimb(h, "Left Arm", c); luaPaintLimb(h, "Right Arm", c);
                } else if (kind == 1 && (std::strcmp(key, "Color3") == 0 || std::strcmp(key, "Color") == 0)) {
                    h.pantsOn = true; h.pantsColor = c;
                    luaPaintLimb(h, "Left Leg", c); luaPaintLimb(h, "Right Leg", c);
                } else if (kind == 2 && (std::strcmp(key, "Color3") == 0 || std::strcmp(key, "Color") == 0)) {
                    h.hatOn = true; h.hatColor = c;
                } else if (kind == 3) {
                    if (std::strcmp(key, "HeadColor") == 0) luaPaintLimb(h, "Head", c);
                    else if (std::strcmp(key, "TorsoColor") == 0) luaPaintLimb(h, "Torso", c);
                    else if (std::strcmp(key, "LeftArmColor") == 0) luaPaintLimb(h, "Left Arm", c);
                    else if (std::strcmp(key, "RightArmColor") == 0) luaPaintLimb(h, "Right Arm", c);
                    else if (std::strcmp(key, "LeftLegColor") == 0) luaPaintLimb(h, "Left Leg", c);
                    else if (std::strcmp(key, "RightLegColor") == 0) luaPaintLimb(h, "Right Leg", c);
                }
            }
        }
    }

    static int luaMakeClothes(lua_State* ls, const char* className, int kind) {
        int npc = -1;
        bool attached = false;
        for (int idx = 2; idx <= 3 && !attached; ++idx)
            attached = clothTarget(ls, idx, npc);
        lua_newtable(ls);
        lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");
        lua_pushstring(ls, lua_isstring(ls, 2) ? lua_tostring(ls, 2) : className); lua_setfield(ls, -2, "Name");
        lua_pushinteger(ls, kind); lua_setfield(ls, -2, "__cloth");
        lua_pushinteger(ls, npc); lua_setfield(ls, -2, "__npc");
        lua_pushboolean(ls, attached ? 1 : 0); lua_setfield(ls, -2, "__attached");
        lua_newtable(ls);
        blockyPushCFunction(ls, luaClothesNewIndex); lua_setfield(ls, -2, "__newindex");
        lua_setmetatable(ls, -2);
        if (kind == 2 && attached) {
            auto* rt = fromState(ls);
            Humanoid* hum = nullptr;
            if (rt && rt->workspace && npc >= 0 && npc < (int)rt->workspace->npcs.size())
                hum = &rt->workspace->npcs[(size_t)npc].humanoid;
            else if (rt) hum = rt->playerHumanoid;
            if (hum) hum->hatOn = true;
        }
        return 1;
    }

    /**
     * GUI in stile Roblox: valori iniziali come in Roblox (Position 0,0, Size in
     * pixel, colori di Studio) e niente sullo schermo finche' l'elemento non
     * sta in una ScreenGui messa in PlayerGui / StarterGui.
     */
    static void robloxGuiInit(WorkspaceLabel& l, const char* cls) {
        l.uiLayout = true;
        l.uiAttached = false;
        l.skipNet = true;   // la GUI del giocatore (PlayerGui) resta sul suo schermo, come Roblox
        l.anchorX = l.anchorY = 0.f;
        l.offsetX = l.offsetY = 0.f;
        l.anchorPointX = l.anchorPointY = 0.f;
        l.sizeScaleX = l.sizeScaleY = 0.f;
        l.bgColor = {0.639f, 0.635f, 0.647f};
        l.bgTransparency = 0.f;
        l.color = {0.106f, 0.165f, 0.208f};
        l.fontScale = 1.f;
        const std::string c = cls ? cls : "";
        if (c == "ScreenGui") {
            l.className = "ScreenGui";
            l.sizeScaleX = l.sizeScaleY = 1.f;
            l.sizeW = l.sizeH = 0.f;
            l.text.clear();
            l.bgTransparency = 1.f;
        } else if (c == "TextLabel" || c == "TextButton" || c == "TextBox") {
            if (c == "TextBox") l.className = "TextBox";
            l.sizeW = 200.f;
            l.sizeH = 50.f;
            l.text.clear();   // niente scritta "Label"/"Button": si vede solo il Text dello script
        } else {   // Frame, ScrollingFrame, ImageLabel, ImageButton
            l.sizeW = 100.f;
            l.sizeH = 100.f;
            l.text.clear();
        }
    }

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
            lab.className = "ScreenGui";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.0f;
            lab.sizeW = 200; lab.sizeH = 100;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
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
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
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
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
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
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
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
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
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
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "ImageButton") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "ImageButton");
            lab.className = "ImageButton";
            lab.text = lab.name;
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
            pushLiveLabel(ls, (int)rt->workspace->labels.size() - 1);
            return 1;
        }
        if (std::strcmp(className, "TextBox") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, "TextBox");
            lab.className = "TextBox";
            lab.text = "";
            lab.visible = true;
            lab.anchorX = 0.5f; lab.anchorY = 0.5f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1)) lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
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
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
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
            lab.skipNet = rt->net && rt->net->role == NetRole::Client;
            rt->workspace->labels.push_back(std::move(lab));
            robloxGuiInit(rt->workspace->labels.back(), className);
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
            p.seat = true;   // come su Roblox: un Seat fa sedere chi ci cammina sopra
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "Seat"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "VehicleSeat") == 0) {
            WorkspacePart p; p.name = luaL_optstring(ls, 2, "VehicleSeat");
            p.size = {4, 1, 2}; p.color = {0.75f, 0.55f, 0.2f};
            p.seat = true;   // un VehicleSeat e' prima di tutto un sedile
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
            const bool spot = std::strcmp(className, "SpotLight") == 0;
            const bool surface = std::strcmp(className, "SurfaceLight") == 0;
            if (partIdx >= 0 && partIdx < (int)rt->workspace->parts.size()) {
                WorkspacePart& lp = rt->workspace->parts[partIdx];
                lp.pointLight = true;
                lp.lightRange = 16.f;
                lp.lightBrightness = 1.f;
                lp.lightKind = surface ? 2.f : (spot ? 1.f : 0.f);
                if (spot || surface) lp.lightAngle = 90.f;
                if (surface) lp.lightFace = 1.f;
            }
            lua_newtable(ls);
            lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, className)); lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, partIdx); lua_setfield(ls, -2, "_part");
            trackPartTable(ls, -1);
            lua_newtable(ls);
            lua_pushnumber(ls, 16); lua_setfield(ls, -2, "Range");
            lua_pushnumber(ls, 1); lua_setfield(ls, -2, "Brightness");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (spot || surface) { lua_pushnumber(ls, 90); lua_setfield(ls, -2, "Angle"); }
            if (surface) { lua_pushnumber(ls, 1); lua_setfield(ls, -2, "Face"); }
            lua_setfield(ls, -2, "_v");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaLightIndex);
            lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaLightNewIndex);
            lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "Fire") == 0 || std::strcmp(className, "Smoke") == 0
            || std::strcmp(className, "Sparkles") == 0) {
            ScriptParticleEmitter e;
            e.className = className;
            e.name = luaL_optstring(ls, 2, className);
            if (std::strcmp(className, "Smoke") == 0) {
                e.preset = (int)ParticlePreset::Smoke;
                e.rate = 8.f;
                e.life = 1.4f;
                e.color = {0.55f, 0.55f, 0.55f};
            } else if (std::strcmp(className, "Fire") == 0) {
                e.preset = (int)ParticlePreset::Sparks;
                e.rate = 14.f;
                e.speed = 4.f;
                e.life = 0.7f;
                e.color = {1.f, 0.35f, 0.05f};
                e.customMotion = true;
            } else {
                e.preset = (int)ParticlePreset::Sparks;
                e.rate = 18.f;
                e.color = {1.f, 0.95f, 0.45f};
            }
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
            lua_newtable(ls);
            blockyPushCFunction(ls, luaAttachmentNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
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
        if (std::strcmp(className, "Trail") == 0) {
            ScriptControls::ScriptTrail t;
            t.name = luaL_optstring(ls, 2, "Trail");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) t.partIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            scriptControls().trails.push_back(std::move(t));
            const int ti = (int)scriptControls().trails.size() - 1;
            lua_newtable(ls);
            lua_pushstring(ls, "Trail"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, scriptControls().trails[(size_t)ti].name.c_str()); lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, ti); lua_setfield(ls, -2, "__trailIndex");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaTrailNewIndex); lua_setfield(ls, -2, "__newindex");
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
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); trackPartTable(ls, -2); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "Highlight") == 0) return luaMakeAdorn(ls, "Highlight", 0);
        if (std::strcmp(className, "SelectionBox") == 0) return luaMakeAdorn(ls, "SelectionBox", 1);
        if (std::strcmp(className, "SelectionSphere") == 0) return luaMakeAdorn(ls, "SelectionSphere", 2);
        if (std::strcmp(className, "DragDetector") == 0) return luaMakeAdorn(ls, "DragDetector", 3);
        if (std::strcmp(className, "Decal") == 0) return luaMakeAdorn(ls, "Decal", 4);
        if (std::strcmp(className, "ColorCorrectionEffect") == 0) return luaMakeLightFx(ls, "ColorCorrectionEffect", 0);
        if (std::strcmp(className, "BloomEffect") == 0) return luaMakeLightFx(ls, "BloomEffect", 1);
        if (std::strcmp(className, "BlurEffect") == 0) return luaMakeLightFx(ls, "BlurEffect", 2);
        if (std::strcmp(className, "DepthOfFieldEffect") == 0) return luaMakeLightFx(ls, "DepthOfFieldEffect", 3);
        if (std::strcmp(className, "SunRaysEffect") == 0) return luaMakeLightFx(ls, "SunRaysEffect", 4);
        if (std::strcmp(className, "BoxHandleAdornment") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "BoxHandleAdornment"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "BoxHandleAdornment")); lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, 1); lua_setfield(ls, -2, "Enabled");
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); trackPartTable(ls, -2); }
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
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); trackPartTable(ls, -2); }
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
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); trackPartTable(ls, -2); }
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
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); trackPartTable(ls, -2); }
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
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); trackPartTable(ls, -2); }
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
                if (lua_isnumber(ls, -1)) { lua_pushvalue(ls, -1); lua_setfield(ls, -3, "__partIndex"); trackPartTable(ls, -2); }
                lua_pop(ls, 1);
            }
            return 1;
        }
        if (std::strcmp(className, "Accessory") == 0) return luaMakeClothes(ls, "Accessory", 2);
        if (std::strcmp(className, "Hat") == 0) return luaMakeClothes(ls, "Hat", 2);
        if (std::strcmp(className, "Pants") == 0) return luaMakeClothes(ls, "Pants", 1);
        if (std::strcmp(className, "Shirt") == 0) return luaMakeClothes(ls, "Shirt", 0);
        if (std::strcmp(className, "ShirtGraphic") == 0) return luaMakeClothes(ls, "ShirtGraphic", 0);
        if (std::strcmp(className, "BodyColors") == 0) return luaMakeClothes(ls, "BodyColors", 3);
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
            p.anchored = false;
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
            p.anchored = false;
            p.name = luaL_optstring(ls, 2, "TrussPart");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "TrussPart"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "MeshPart") == 0) {
            WorkspacePart p;
            p.anchored = false;
            p.name = luaL_optstring(ls, 2, "MeshPart");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "MeshPart"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "UnionOperation") == 0) {
            WorkspacePart p;
            p.anchored = false;
            p.name = luaL_optstring(ls, 2, "UnionOperation");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "UnionOperation"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "NegateOperation") == 0) {
            WorkspacePart p;
            p.anchored = false;
            p.name = luaL_optstring(ls, 2, "NegateOperation");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "NegateOperation"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "IntersectOperation") == 0) {
            WorkspacePart p;
            p.anchored = false;
            p.name = luaL_optstring(ls, 2, "IntersectOperation");
            p.size = {4, 2, 4};
            rt->workspace->parts.push_back(p);
            pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
            lua_pushstring(ls, "IntersectOperation"); lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "LinearVelocity") == 0 || std::strcmp(className, "VectorForce") == 0) {
            const bool vel = std::strcmp(className, "LinearVelocity") == 0;
            lua_newtable(ls);
            lua_pushstring(ls, className); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, className)); lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, vel ? 0 : 1); lua_setfield(ls, -2, "__moverKind");
            lua_pushinteger(ls, -1); lua_setfield(ls, -2, "__moverSlot");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaMoverNewIndex); lua_setfield(ls, -2, "__newindex");
            blockyPushCFunction(ls, luaMoverIndex); lua_setfield(ls, -2, "__index");
            lua_setmetatable(ls, -2);
            return 1;
        }
        return 0;
    }

    static bool luaReadVec3(lua_State* ls, int idx, glm::vec3& o) {
        if (!lua_istable(ls, idx)) return false;
        auto comp = [&](const char* k, float& v) {
            lua_getfield(ls, idx, k);
            v = lua_isnumber(ls, -1) ? (float)lua_tonumber(ls, -1) : 0.f;
            lua_pop(ls, 1);
        };
        comp("X", o.x); comp("Y", o.y); comp("Z", o.z);
        return true;
    }

    static int luaMoverIndex(lua_State* ls) {
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        return 1;
    }

    static int luaMoverNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        lua_pushvalue(ls, 2);
        lua_pushvalue(ls, 3);
        lua_rawset(ls, 1);
        if (!key) return 0;
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return 0;
        lua_getfield(ls, 1, "__moverSlot");
        int wi = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        lua_getfield(ls, 1, "__moverKind");
        const int kind = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : 0;
        lua_pop(ls, 1);
        auto& welds = rt->workspace->welds;
        auto applyVec = [&](const glm::vec3& v) {
            if (wi < 0 || wi >= (int)welds.size()) return;
            const float mag = glm::length(v);
            welds[(size_t)wi].axis = mag > 1e-4f ? v / mag : glm::vec3(0.f, 1.f, 0.f);
            if (kind == 0) welds[(size_t)wi].length = mag;
            else welds[(size_t)wi].stiffness = mag;
        };
        if ((std::strcmp(key, "VectorVelocity") == 0 || std::strcmp(key, "Force") == 0) && lua_istable(ls, 3)) {
            glm::vec3 v(0.f);
            if (luaReadVec3(ls, 3, v)) applyVec(v);
        } else if (std::strcmp(key, "Parent") == 0 && lua_istable(ls, 3) && wi < 0) {
            lua_getfield(ls, 3, "__partIndex");
            const int pi = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
            lua_pop(ls, 1);
            if (pi >= 0) {
                const ConstraintKind ck = kind == 0 ? ConstraintKind::LinearVelocity : ConstraintKind::VectorForce;
                wi = partconstraints::create(*rt->workspace, ck, pi, pi);
                if (wi >= 0) {
                    lua_pushinteger(ls, wi);
                    lua_setfield(ls, 1, "__moverSlot");
                    lua_getfield(ls, 1, kind == 0 ? "VectorVelocity" : "Force");
                    glm::vec3 v(0.f);
                    if (luaReadVec3(ls, -1, v)) applyVec(v);
                    lua_pop(ls, 1);
                }
            }
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
        else if (std::strcmp(key, "Color") == 0 && lua_istable(ls, 3)) {
            float r = b.r, g = b.g, bl = b.b;
            if (luaReadRgb(ls, 3, r, g, bl)) { b.r = r; b.g = g; b.b = bl; }
        }
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
        appendEffectChildren(ls, parent, n);
        std::vector<int> stack; stack.push_back(parent);
        std::vector<char> seen(rt->workspace->parts.size(), 0);
        while (!stack.empty()) {
            int cur = stack.back(); stack.pop_back();
            for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
                if (rt->workspace->parts[i].parentIndex == cur && !seen[i]) {
                    seen[i] = 1;
                    pushLivePart(ls, i);
                    lua_rawseti(ls, -2, n++);
                    appendEffectChildren(ls, i, n);
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
        if (pushClassEffect(ls, parent, cls)) return 1;
        lua_pushnil(ls); return 1;
    }
    // Antenati Luau (Model, Folder dello script) sopra la part piu' alta del motore.
    static int pushLuaAncestor(lua_State* ls, int idx, const char* wanted, bool byClass) {
        auto* rt = fromState(ls);
        int top = idx;
        while (rt && rt->workspace && top >= 0 && top < (int)rt->workspace->parts.size()
               && rt->workspace->parts[top].parentIndex >= 0)
            top = rt->workspace->parts[top].parentIndex;
        if (pushPreludeField(ls, "luaAncestorOfPart")) {
            lua_pushinteger(ls, top);
            lua_pushstring(ls, wanted);
            lua_pushboolean(ls, byClass);
            if (lua_pcall(ls, 3, 1, 0) == LUA_OK) return 1;
            lua_pop(ls, 1);
        }
        lua_pushnil(ls);
        return 1;
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
        return pushLuaAncestor(ls, idx, name, false);
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
        return pushLuaAncestor(ls, idx, cls, true);
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
        pushSignalMethod(ls, luaAttrChangedConnect, "luaAttrChangedConnect", 0); lua_setfield(ls, -2, "Connect"); pushSignalMethod(ls, luaAttrChangedConnect, "luaAttrChangedConnect", 1); lua_setfield(ls, -2, "Once"); pushSignalMethod(ls, luaAttrChangedConnect, "luaAttrChangedConnect", 2); lua_setfield(ls, -2, "Wait");
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
        const char* name = luaL_optstring(ls, lua_istable(ls, 1) ? 2 : 1, "");
        if (name && social().user == name)
            lua_pushinteger(ls, social().userId);
        else
            lua_pushinteger(ls, 0);
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
    static bool mouseButtonDown(int button) {
        GLFWwindow* win = glfwGetCurrentContext();
        return win && glfwGetMouseButton(win, button) == GLFW_PRESS;
    }
    static int luaInputGetMouseButtonsPressed(lua_State* ls) {
        lua_newtable(ls);
        int n = 1;
        const char* names[] = {"MouseButton1", "MouseButton2", "MouseButton3"};
        const int ids[] = {GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, GLFW_MOUSE_BUTTON_MIDDLE};
        for (int i = 0; i < 3; ++i) {
            if (!mouseButtonDown(ids[i])) continue;
            lua_newtable(ls);
            lua_pushstring(ls, names[i]);
            lua_setfield(ls, -2, "Name");
            lua_rawseti(ls, -2, n++);
        }
        return 1;
    }
    static int luaMouseButtonConnect(lua_State* ls) {
        int kind = (int)lua_tointeger(ls, lua_upvalueindex(1));
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Mouse:Connect(function)");
        lua_pushvalue(ls, fidx);
        int ref = blockyLuaRef(ls);
        auto& sc = scriptControls();
        if (kind == 1) sc.mouse1Up.push_back(ref);
        else if (kind == 2) sc.mouse2Down.push_back(ref);
        else if (kind == 3) sc.mouse2Up.push_back(ref);
        else sc.mouse1Down.push_back(ref);
        return 0;
    }
    static void pushLookCFrame(lua_State* ls, const glm::vec3& pos, const glm::vec3& look) {
        if (pushPreludeField(ls, "CFrame")) {
            lua_getfield(ls, -1, "lookAt");
            lua_remove(ls, -2);
            if (lua_isfunction(ls, -1)) {
                pushVec3(ls, pos.x, pos.y, pos.z);
                pushVec3(ls, pos.x + look.x, pos.y + look.y, pos.z + look.z);
                if (lua_pcall(ls, 2, 1, 0) == LUA_OK) return;
            }
            lua_pop(ls, 1);
        }
        lua_newtable(ls);
        pushVec3(ls, pos.x, pos.y, pos.z);
        lua_setfield(ls, -2, "Position");
        pushVec3(ls, look.x, look.y, look.z);
        lua_setfield(ls, -2, "LookVector");
    }
    static int luaMouseIndex(lua_State* ls) {
        const char* k = lua_tostring(ls, 2);
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        if (!lua_isnil(ls, -1)) return 1;
        lua_pop(ls, 1);
        if (!k) return 0;
        auto* rt = fromState(ls);
        if (!std::strcmp(k, "X") || !std::strcmp(k, "Y")) {
            double x = 0, y = 0;
            if (GLFWwindow* win = glfwGetCurrentContext()) glfwGetCursorPos(win, &x, &y);
            lua_pushnumber(ls, !std::strcmp(k, "X") ? x : y);
            return 1;
        }
        glm::vec3 o{0.f}, d{0.f, 0.f, -1.f};
        fillAimRay(rt, o, d);
        if (!std::strcmp(k, "UnitRay")) {
            lua_newtable(ls);
            pushVec3(ls, o.x, o.y, o.z); lua_setfield(ls, -2, "Origin");
            pushVec3(ls, d.x, d.y, d.z); lua_setfield(ls, -2, "Direction");
            return 1;
        }
        WorldRayHit hit = castWorld(rt, o, d, 800.f);
        glm::vec3 pos = hit.ok ? hit.pos : o + d * 800.f;
        if (!std::strcmp(k, "Hit") || !std::strcmp(k, "Origin")) {
            pushLookCFrame(ls, !std::strcmp(k, "Origin") ? o : pos, d);
            return 1;
        }
        if (!std::strcmp(k, "Target")) {
            if (hit.ok && !hit.voxel && hit.part >= 0) pushLivePart(ls, hit.part);
            else lua_pushnil(ls);
            return 1;
        }
        return 0;
    }
    static int luaPlayerGetMouse(lua_State* ls) {
        lua_getfield(ls, LUA_REGISTRYINDEX, "BlockyMouse");
        if (lua_istable(ls, -1)) return 1;
        lua_pop(ls, 1);
        lua_newtable(ls);
        lua_pushstring(ls, "Mouse"); lua_setfield(ls, -2, "ClassName");
        for (int kind = 0; kind < 4; ++kind) {
            static const char* fields[] = {"Button1Down", "Button1Up", "Button2Down", "Button2Up"};
            lua_newtable(ls);
            lua_pushinteger(ls, kind);
            lua_pushcclosure(ls, luaMouseButtonConnect, "luaMouseButtonConnect", 1);
            lua_setfield(ls, -2, "Connect");
            lua_setfield(ls, -2, fields[kind]);
        }
        lua_newtable(ls);
        blockyPushCFunction(ls, luaMouseIndex);
        lua_setfield(ls, -2, "__index");
        lua_setmetatable(ls, -2);
        lua_pushvalue(ls, -1);
        lua_setfield(ls, LUA_REGISTRYINDEX, "BlockyMouse");
        return 1;
    }
    static int luaPlayerDistanceFromCharacter(lua_State* ls) {
        auto* rt = fromState(ls);
        int arg = lua_istable(ls, 1) ? 2 : 1;
        glm::vec3 p{0.f};
        if (!readVec3Arg(ls, arg, p))
            return blockyLuaError(ls, "DistanceFromCharacter(Vector3)");
        glm::vec3 me = (rt && rt->playerPos) ? *rt->playerPos + glm::vec3(0.f, 2.5f, 0.f) : glm::vec3(0.f);
        lua_pushnumber(ls, glm::length(p - me));
        return 1;
    }
    static int luaInputGetLastInputType(lua_State* ls) {
        // EnumItem, non una stringa: i giochi leggono .Name ("Keyboard", "Gamepad1"...).
        lua_getglobal(ls, "Enum");
        if (lua_istable(ls, -1)) {
            lua_getfield(ls, -1, "UserInputType");
            if (lua_istable(ls, -1)) {
                lua_getfield(ls, -1, "Keyboard");
                if (!lua_isnil(ls, -1)) {
                    lua_remove(ls, -2);
                    lua_remove(ls, -2);
                    return 1;
                }
                lua_pop(ls, 1);
            }
            lua_pop(ls, 1);
        }
        lua_pop(ls, 1);
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
        pushSignalMethod(ls, luaCollectionTagAddedConnect, "luaCollectionTagAddedConnect", 0); lua_setfield(ls, -2, "Connect"); pushSignalMethod(ls, luaCollectionTagAddedConnect, "luaCollectionTagAddedConnect", 1); lua_setfield(ls, -2, "Once"); pushSignalMethod(ls, luaCollectionTagAddedConnect, "luaCollectionTagAddedConnect", 2); lua_setfield(ls, -2, "Wait");
        return 1;
    }
    static int luaCollectionGetInstanceRemovedSignal(lua_State* ls) {
        const char* tag = luaL_checkstring(ls, lua_istable(ls, 1) ? 2 : 1);
        lua_newtable(ls);
        lua_pushstring(ls, tag); lua_setfield(ls, -2, "_tag");
        pushSignalMethod(ls, luaCollectionTagRemovedConnect, "luaCollectionTagRemovedConnect", 0); lua_setfield(ls, -2, "Connect"); pushSignalMethod(ls, luaCollectionTagRemovedConnect, "luaCollectionTagRemovedConnect", 1); lua_setfield(ls, -2, "Once"); pushSignalMethod(ls, luaCollectionTagRemovedConnect, "luaCollectionTagRemovedConnect", 2); lua_setfield(ls, -2, "Wait");
        return 1;
    }

    static int luaWorkspaceGetGravity(lua_State* ls) {
        lua_pushnumber(ls, scriptControls().workspaceGravity);
        return 1;
    }
    static int luaWorkspaceSetGravity(lua_State* ls) {
        float g = (float)luaL_optnumber(ls, lua_istable(ls, 1) ? 2 : 1, 80.0);
        scriptControls().workspaceGravity = std::clamp(g, 0.f, 1000.f);
        // anche il campo, che e' quello che la fisica legge (readWorkspaceGravity)
        lua_getglobal(ls, "workspace");
        if (lua_istable(ls, -1)) {
            lua_pushstring(ls, "Gravity");
            lua_pushnumber(ls, scriptControls().workspaceGravity);
            lua_rawset(ls, -3);
        }
        lua_pop(ls, 1);
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
            pushSignalMethod(L, luaInputBeganConnect, "luaInputBeganConnect", 0);
            lua_setfield(L, -2, "Connect");
            pushSignalMethod(L, luaInputBeganConnect, "luaInputBeganConnect", 1);
            lua_setfield(L, -2, "Once");
            pushSignalMethod(L, luaInputBeganConnect, "luaInputBeganConnect", 2);
            lua_setfield(L, -2, "Wait");
            lua_setfield(L, -2, "InputBegan");
            lua_newtable(L);
            pushSignalMethod(L, luaInputEndedConnect, "luaInputEndedConnect", 0);
            lua_setfield(L, -2, "Connect");
            pushSignalMethod(L, luaInputEndedConnect, "luaInputEndedConnect", 1);
            lua_setfield(L, -2, "Once");
            pushSignalMethod(L, luaInputEndedConnect, "luaInputEndedConnect", 2);
            lua_setfield(L, -2, "Wait");
            lua_setfield(L, -2, "InputEnded");
            blockyPushCFunction(L, luaInputGetMouseLocation);
            lua_setfield(L, -2, "GetMouseLocation");
            blockyPushCFunction(L, luaInputIsGamepadEnabled);
            lua_setfield(L, -2, "IsGamepadEnabled");
            blockyPushCFunction(L, luaInputGetKeysPressed);
            lua_setfield(L, -2, "GetKeysPressed");
            blockyPushCFunction(L, luaInputGetMouseButtonsPressed);
            lua_setfield(L, -2, "GetMouseButtonsPressed");
            blockyPushCFunction(L, luaInputIsMouseButtonPressed);
            lua_setfield(L, -2, "IsMouseButtonPressed");
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
                pushSignalMethod(L, luaPlayerRemovingConnect, "luaPlayerRemovingConnect", 0);
                lua_setfield(L, -2, "Connect");
                pushSignalMethod(L, luaPlayerRemovingConnect, "luaPlayerRemovingConnect", 1);
                lua_setfield(L, -2, "Once");
                pushSignalMethod(L, luaPlayerRemovingConnect, "luaPlayerRemovingConnect", 2);
                lua_setfield(L, -2, "Wait");
                lua_setfield(L, -2, "PlayerRemoving");
                blockyPushCFunction(L, luaPlayersGetUserIdFromNameAsync);
                lua_setfield(L, -2, "GetUserIdFromNameAsync");
                lua_getfield(L, -1, "LocalPlayer");
                if (lua_istable(L, -1)) {
                    const char* nm = social().user.empty() ? "Player" : social().user.c_str();
                    lua_pushstring(L, nm); lua_setfield(L, -2, "Name");
                    lua_pushstring(L, nm); lua_setfield(L, -2, "DisplayName");
                    lua_pushinteger(L, social().userId); lua_setfield(L, -2, "UserId");
                    blockyPushCFunction(L, luaPlayerKick); lua_setfield(L, -2, "Kick");
                    blockyPushCFunction(L, studio_pro_apis::luaPlayerIsInGroup); lua_setfield(L, -2, "IsInGroup");
                    blockyPushCFunction(L, studio_pro_apis::luaGroupRank); lua_setfield(L, -2, "GetRankInGroup");
                    blockyPushCFunction(L, luaPlayerGetMouse); lua_setfield(L, -2, "GetMouse");
                    blockyPushCFunction(L, luaPlayerDistanceFromCharacter); lua_setfield(L, -2, "DistanceFromCharacter");
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
            lua_pushnumber(L, -12.0);   // sotto si muore (ScriptsAndPhysics.h)
            lua_setfield(L, -2, "FallenPartsDestroyHeight");
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
