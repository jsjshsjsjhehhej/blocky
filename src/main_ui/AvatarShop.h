// Libreria animazioni di camminata dell'avatar (import/rilascio/equip) e
// inventario/shop degli accessori (equip/unequip, pubblicazione nello shop).
// Non e' un header riusabile: usa direttamente le globali di main.cpp
// (g_catalog, g_menu, g_animator, ecc.). Compila solo se incluso da
// main.cpp esattamente in questo punto, mai da altri translation unit.

static void syncEquippedWalkToAnimator() {
    if (g_catalog.equippedWalkId.empty()) {
        g_animator.clearWalkOverride();
        return;
    }
    for (const auto& c : g_catalog.walkClips) {
        if (c.id == g_catalog.equippedWalkId && c.valid()) {
            if (Animator::looksLikeToolAnim(c)) {
                g_catalog.equippedWalkId.clear();
                animLibrary().defaultWalkId.clear();
                g_animator.clearWalkOverride();
                return;
            }
            g_animator.setWalkOverride(c);
            return;
        }
    }
    g_animator.clearWalkOverride();
}

static void loadWalkLibraryIntoCatalog() {
    animLibrary().loadIndex();
    animPacks().load();
    std::string eq;
    r6LoadLibrary(g_catalog.walkClips, eq);
    for (const auto& c : g_catalog.walkClips)
        animLibrary().byId[c.id] = c;
    if (const R6WalkClip* w = animLibrary().get("classic_walk")) {
        bool found = false;
        for (const auto& c : g_catalog.walkClips)
            if (c.id == "classic_walk") { found = true; break; }
        if (!found) g_catalog.walkClips.push_back(*w);
    }
    if (!eq.empty()) {
        g_catalog.equippedWalkId = eq;
        animLibrary().defaultWalkId = eq;
    } else {
        g_catalog.equippedWalkId = "classic_walk";
        animLibrary().defaultWalkId = "classic_walk";
    }
    if (animLibrary().defaultJumpId.empty())
        animLibrary().defaultJumpId = "classic_jump";
    syncEquippedWalkToAnimator();
    g_animator.setWalkAnimId(animLibrary().defaultWalkId);
    g_animator.setJumpAnimId(animLibrary().defaultJumpId);
}

static void openWalkingAnimWebGuide() {
    // Era un blocco #ifdef _WIN32 che si ricalcolava la cartella a mano; ora
    // e' la stessa riga su ogni piattaforma, e dove aprire un browser non si
    // puo' lo dice la tabella dei servizi invece di non succedere nulla.
    const std::string html =
        (plat::fs::path(plat::selfDir()) / "assets" / "web" / "walking_anim.html").string();

    if (!plat::fs::exists(html)) {
        g_catalog.status = "Guida web non trovata: assets/web/walking_anim.html";
        return;
    }
    if (!plat::has(plat::Service::OpenUrl)) {
        g_catalog.status = "Qui non si aprono link: apri assets/web/walking_anim.html a mano";
        return;
    }
    g_catalog.status = plat::sys::openUrl(html)
        ? "Guida web Walking Animation aperta nel browser"
        : "Impossibile aprire la guida web (assets/web/walking_anim.html)";
}

static void downloadDefaultR6PlayerModel() {
    std::string path;
    if (!saveR6PlayerObjDialog(path, publisherHwnd())) {
        std::string err;
        if (!exportDefaultR6ToLibrary(path, err)) {
            g_catalog.status = "Download R6 annullato / fallito: " + err;
            return;
        }
        g_catalog.status = "Player R6 salvato in: " + path;
    } else {
        std::string err;
        if (!exportDefaultR6PlayerObj(path, err)) {
            g_catalog.status = "Export R6 fallito: " + err;
            return;
        }
        g_catalog.status = "Player R6 scaricato: " + path + " — aprilo in Blender/Maya";
    }
    g_menu.chatLines.push_back("[Avatar] Modello R6 default scaricato (OBJ beveled → Blender/Maya)");
}

static void importWalkingAnimationFromFile() {
    plat::requestFile("Blocky — importa animazione", "*/*", [](const std::string& path) {
    if (path.empty()) {
        g_catalog.status = "Import animazione annullato";
        return;
    }
    R6WalkClip clip;
    std::string err;
    std::vector<std::string> names;
    std::string prefer = g_catalog.walkClipPrefBuf[0]
        ? g_catalog.walkClipPrefBuf : "WalkingAnimation";
    if (!r6ImportWalkFromFile(path, clip, err, prefer, &names)) {
        g_catalog.status = err;
        return;
    }
    g_catalog.pendingClipNames = std::move(names);
    g_catalog.pendingWalk = std::move(clip);
    g_catalog.hasPendingWalk = true;
    g_catalog.previewWalk = true;
    g_catalog.pendingAsWalkDefault = !Animator::looksLikeToolAnim(g_catalog.pendingWalk)
        && g_catalog.pendingWalk.looped;
    std::snprintf(g_catalog.walkNameBuf, sizeof(g_catalog.walkNameBuf), "%s",
                  g_catalog.pendingWalk.clipName.c_str());
    g_catalog.status = "Keyframe da \"" + g_catalog.pendingWalk.clipName
        + "\" — imposta AnimationId e rilascia";
    });
}

static bool releaseWalkingAnimation() {
    if (!g_catalog.hasPendingWalk || !g_catalog.pendingWalk.valid()) {
        g_catalog.status = "Prima importa FBX/GLB da Blender/Maya con animazioni";
        return false;
    }
    std::string name = g_catalog.walkNameBuf[0]
        ? g_catalog.walkNameBuf
        : g_catalog.pendingWalk.displayName;
    auto mod = Moderation::checkAvatarOrModelName(name);
    if (mod.verdict != Moderation::Verdict::Ok) {
        g_catalog.status = mod.reason;
        return false;
    }
    g_catalog.pendingWalk.displayName = name;
    g_catalog.pendingWalk.id = sanitizeGameId(name);
    std::string low = name;
    for (char& c : low) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (low.find("walk") == std::string::npos && low.find("run") == std::string::npos)
        g_catalog.pendingWalk.looped = false;

    if (!animLibrary().registerClip(name, g_catalog.pendingWalk)) {
        g_catalog.status = "Registrazione AnimationId fallita";
        return false;
    }
    std::string path = r6AnimsDir() + "/" + g_catalog.pendingWalk.id + ".walk.json";

    bool replaced = false;
    for (auto& c : g_catalog.walkClips) {
        if (c.id == g_catalog.pendingWalk.id) {
            c = g_catalog.pendingWalk;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        g_catalog.walkClips.push_back(g_catalog.pendingWalk);

    if (g_catalog.pendingAsWalkDefault) {
        if (Animator::looksLikeToolAnim(g_catalog.pendingWalk)) {
            g_catalog.pendingAsWalkDefault = false;
            g_menu.chatLines.push_back(
                "[Studio] Anim tool/pistola: NON impostata come walk (usa Animator:Play)");
        } else {
            g_catalog.equippedWalkId = g_catalog.pendingWalk.id;
            animLibrary().setDefaultWalk(name);
            syncEquippedWalkToAnimator();
        }
    }
    r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);

    seedDefaultShop(g_menu);
    std::string seller = g_menu.username[0] ? g_menu.username : "Builder";
    bool exists = false;
    for (auto& s : g_menu.shopItems) {
        if (s.kind == 1 && (s.sourcePath == path || s.name == name)) {
            s.name = name;
            s.sourcePath = path;
            s.owned = true;
            s.fromPublisher = true;
            s.seller = seller;
            exists = true;
            break;
        }
    }
    if (!exists) {
        ClassicMenuState::ShopItem it;
        it.name = name;
        it.seller = seller;
        it.sourcePath = path;
        it.kind = 1;
        it.price = 80;
        it.owned = true;
        it.fromPublisher = true;
        g_menu.shopItems.push_back(std::move(it));
    }
    saveShopItems(g_menu);
    ecoGrant(g_menu.username, 30, "publish walk anim");
    ecoSyncMenu(g_menu);
    saveUserProfile(g_menu);

    g_catalog.hasPendingWalk = false;
    std::snprintf(g_lastPublishedAnimId, sizeof(g_lastPublishedAnimId), "%s", name.c_str());
    g_catalog.status = "Pubblicato! AnimationId = \"" + name
        + "\"  →  game.Animator:Play(\"" + name + "\")";
    g_menu.chatLines.push_back("[Studio] AnimationId pubblicato: " + name);
    g_exportMsg = "AnimationId: " + name;
    g_showPlayerAnimPublisher = true;
    return true;
}

static bool equipWalkClipById(const std::string& id) {
    for (const auto& c : g_catalog.walkClips) {
        if (c.id == id && c.valid()) {
            g_catalog.equippedWalkId = id;
            syncEquippedWalkToAnimator();
            r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
            return true;
        }
    }
    return false;
}

static void upsertOwnedAvatarInventory(const AvatarAccessory& a) {
    if (a.sourcePath.empty() || a.name.empty()) return;
    seedDefaultShop(g_menu);
    std::string seller = g_menu.username[0] ? g_menu.username : "Player";
    ClassicMenuState::ShopItem* found = nullptr;
    for (auto& s : g_menu.shopItems) {
        if (s.kind != 0) continue;
        if ((!a.sourcePath.empty() && s.sourcePath == a.sourcePath && s.slot == (int)a.slot)
            || (s.name == a.name && s.slot == (int)a.slot && s.fromPublisher && s.seller == seller)) {
            found = &s;
            break;
        }
    }
    if (!found) {
        g_menu.shopItems.push_back({});
        found = &g_menu.shopItems.back();
        found->price = 0;
        found->fromPublisher = true;
    }
    found->name = a.name;
    found->seller = seller;
    found->sourcePath = a.sourcePath;
    found->slot = (int)a.slot;
    found->offset[0] = a.offset.x; found->offset[1] = a.offset.y; found->offset[2] = a.offset.z;
    found->rotation[0] = a.rotation.x; found->rotation[1] = a.rotation.y; found->rotation[2] = a.rotation.z;
    found->scale[0] = a.scale.x; found->scale[1] = a.scale.y; found->scale[2] = a.scale.z;
    found->tint[0] = a.tint.x; found->tint[1] = a.tint.y; found->tint[2] = a.tint.z;
    found->replacesBody = a.replacesBody;
    found->owned = true;
    found->kind = 0;
    found->equipped = true;
}

static void syncCatalogItemsToInventory() {
    for (const auto& a : g_catalog.items)
        upsertOwnedAvatarInventory(a);
    saveShopItems(g_menu);
}

// Prezzo di listino di un accessorio. Era scritto due volte identico dentro
// publishAccessoriesToShop (ramo "gia' nello shop" e ramo "nuovo"): una sola
// copia qui, cosi' la UI puo' mostrare il prezzo PRIMA di pubblicare.
static int avatarItemPrice(const AvatarAccessory& a) {
    int base = 40;
    if (a.slot == AvatarSlot::Head) base = 75;
    else if (a.slot == AvatarSlot::Torso) base = 60;
    return base + (int)(glm::length(a.scale) * 10.f);
}

static std::string avatarSellerName() {
    return g_menu.username[0] ? g_menu.username : "Player";
}

// Ritorna un INDICE, non un puntatore: g_menu.shopItems cresce con push_back
// e un puntatore resterebbe appeso al primo riallocamento del vettore.
static int findShopIndexForAccessory(const AvatarAccessory& a) {
    const std::string seller = avatarSellerName();
    for (int i = 0; i < (int)g_menu.shopItems.size(); ++i) {
        const auto& s = g_menu.shopItems[(size_t)i];
        if (!s.fromPublisher || s.seller != seller || s.slot != (int)a.slot || s.kind != 0)
            continue;
        if (s.sourcePath == a.sourcePath || s.name == a.name) return i;
    }
    return -1;
}

// "In vendita" = ha un prezzo. E' la stessa convenzione che usa gia' la tab
// Shop dell'hub (price <= 0 -> "tuo, creato nel Catalog"), quindi niente
// campo nuovo da salvare/caricare in shop.json.
static bool accessoryIsForSale(const AvatarAccessory& a) {
    const int i = findShopIndexForAccessory(a);
    return i >= 0 && g_menu.shopItems[(size_t)i].price > 0;
}

// Corpo del vecchio ciclo di publishAccessoriesToShop, per un solo oggetto.
// Ritorna true se questo oggetto e' passato ORA da "solo mio" a "in vendita"
// (cioe' se vale una ricompensa); aggiorna reward/blocked come prima.
static bool publishOneAccessory(AvatarAccessory& a, const std::string& seller,
                                int& reward, int& blocked) {
    if (a.sourcePath.empty() || !a.mesh.loaded) return false;

    auto modName = Moderation::checkAvatarOrModelName(a.name);
    if (modName.verdict != Moderation::Verdict::Ok) {
        g_menu.chatLines.push_back(modName.reason + " (" + a.name + ")");
        blocked++;
        return false;
    }
    auto modPath = Moderation::checkAvatarOrModelName(a.sourcePath);
    if (modPath.verdict != Moderation::Verdict::Ok) {
        g_menu.chatLines.push_back(modPath.reason + " (" + a.name + ")");
        blocked++;
        return false;
    }

    std::string lib = copyModelToLibrary(a.sourcePath, a.name);
    if (!lib.empty()) a.sourcePath = lib;

    const int newPrice = avatarItemPrice(a);
    const int idx = findShopIndexForAccessory(a);
    if (idx >= 0) {
        auto& found = g_menu.shopItems[(size_t)idx];
        found.sourcePath = a.sourcePath;
        found.offset[0] = a.offset.x; found.offset[1] = a.offset.y; found.offset[2] = a.offset.z;
        found.rotation[0] = a.rotation.x; found.rotation[1] = a.rotation.y; found.rotation[2] = a.rotation.z;
        found.scale[0] = a.scale.x; found.scale[1] = a.scale.y; found.scale[2] = a.scale.z;
        found.tint[0] = a.tint.x; found.tint[1] = a.tint.y; found.tint[2] = a.tint.z;
        found.replacesBody = a.replacesBody;
        found.owned = true;
        found.equipped = true;
        const bool wasPrivate = (found.price <= 0);
        found.price = newPrice;
        if (wasPrivate) {
            reward += 50;
            return true;
        }
        return false;
    }

    ClassicMenuState::ShopItem it;
    it.name = a.name;
    it.seller = seller;
    it.sourcePath = a.sourcePath;
    it.slot = (int)a.slot;
    it.offset[0] = a.offset.x; it.offset[1] = a.offset.y; it.offset[2] = a.offset.z;
    it.rotation[0] = a.rotation.x; it.rotation[1] = a.rotation.y; it.rotation[2] = a.rotation.z;
    it.scale[0] = a.scale.x; it.scale[1] = a.scale.y; it.scale[2] = a.scale.z;
    it.tint[0] = a.tint.x; it.tint[1] = a.tint.y; it.tint[2] = a.tint.z;
    it.replacesBody = a.replacesBody;
    it.price = newPrice;
    it.owned = true;
    it.fromPublisher = true;
    it.equipped = true;
    g_menu.shopItems.push_back(std::move(it));
    reward += 50;
    return true;
}

static int publishAccessoriesToShop() {
    if (g_catalog.items.empty()) return 0;
    seedDefaultShop(g_menu);
    const std::string seller = avatarSellerName();
    int published = 0;
    int reward = 0;
    int blocked = 0;
    for (auto& a : g_catalog.items) {
        if (publishOneAccessory(a, seller, reward, blocked))
            published++;
    }
    if (blocked > 0 && published == 0) {
        g_catalog.status = "Pubblicazione bloccata dalla moderazione";
    }
    if (published > 0) {
        int give = std::min(reward, EconomyState::kMaxGrant);
        ecoGrant(g_menu.username, give, "publish avatar");
        ecoSyncMenu(g_menu);
        g_menu.chatLines.push_back(
            "[Shop] Pubblicati " + std::to_string(published) + " avatar item (+"
            + std::to_string(give) + " B$). Salvati su disco!");
        g_menu.tab = HubTab::Shop;
        saveUserProfile(g_menu);
        saveShopItems(g_menu);
        saveEquippedAvatar(g_catalog);
    } else if (blocked == 0) {
        saveShopItems(g_menu);
        saveEquippedAvatar(g_catalog);
    }
    return published;
}

// Mette in vendita UN solo accessorio, senza portare l'utente fuori
// dall'editor (niente g_menu.tab = Shop, niente ritorno a Studio): serve al
// bottone per-oggetto del nuovo pannello proprieta'.
static bool publishSingleAccessory(int catalogIdx) {
    if (catalogIdx < 0 || catalogIdx >= (int)g_catalog.items.size()) return false;
    seedDefaultShop(g_menu);
    const std::string seller = avatarSellerName();
    int reward = 0;
    int blocked = 0;
    auto& a = g_catalog.items[(size_t)catalogIdx];
    const bool newly = publishOneAccessory(a, seller, reward, blocked);
    if (blocked > 0 && !newly) {
        g_catalog.status = "Pubblicazione bloccata dalla moderazione";
        return false;
    }
    if (newly) {
        const int give = std::min(reward, EconomyState::kMaxGrant);
        ecoGrant(g_menu.username, give, "publish avatar");
        ecoSyncMenu(g_menu);
        g_menu.chatLines.push_back("[Shop] \"" + a.name + "\" ora in vendita (+"
                                   + std::to_string(give) + " B$)");
        saveUserProfile(g_menu);
        g_catalog.status = "In vendita nello Shop: " + a.name;
    } else {
        g_catalog.status = "Prezzo aggiornato: " + a.name;
    }
    saveShopItems(g_menu);
    saveEquippedAvatar(g_catalog);
    return true;
}

// Ritira dal negozio ma lascia l'oggetto sull'avatar (price = 0 e' proprio lo
// stato "mio ma non in vendita"). Non toglie i B$ gia' guadagnati: coerente
// con publishOneAccessory, che paga solo la prima volta.
static bool unpublishAccessory(int catalogIdx) {
    if (catalogIdx < 0 || catalogIdx >= (int)g_catalog.items.size()) return false;
    const auto& a = g_catalog.items[(size_t)catalogIdx];
    const int idx = findShopIndexForAccessory(a);
    if (idx < 0) return false;
    g_menu.shopItems[(size_t)idx].price = 0;
    saveShopItems(g_menu);
    g_catalog.status = "Ritirato dal negozio: " + a.name + " (resta sul tuo avatar)";
    return true;
}

static bool equipShopItem(int idx) {
    if (idx < 0 || idx >= (int)g_menu.shopItems.size()) return false;
    const auto& it = g_menu.shopItems[idx];
    if (!it.owned || it.sourcePath.empty()) return false;

    if (it.kind == 1) {
        R6WalkClip clip;
        if (!r6LoadClipFile(it.sourcePath, clip)) {
            g_menu.chatLines.push_back("[Shop] Walk non trovata: " + it.sourcePath);
            return false;
        }
        clip.displayName = it.name;
        bool found = false;
        for (auto& c : g_catalog.walkClips) {
            if (c.id == clip.id) { c = clip; found = true; break; }
        }
        if (!found) g_catalog.walkClips.push_back(clip);
        g_catalog.equippedWalkId = clip.id;
        syncEquippedWalkToAnimator();
        r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
        g_menu.chatLines.push_back("[Shop] Walking Animation equipaggiata: " + it.name);
        return true;
    }

    AvatarAccessory acc;
    acc.slot = (AvatarSlot)it.slot;
    acc.name = it.name;
    acc.sourcePath = it.sourcePath;
    acc.offset = {it.offset[0], it.offset[1], it.offset[2]};
    acc.rotation = {it.rotation[0], it.rotation[1], it.rotation[2]};
    acc.scale = {it.scale[0], it.scale[1], it.scale[2]};
    acc.tint = {it.tint[0], it.tint[1], it.tint[2]};
    acc.replacesBody = it.replacesBody;
    if (!acc.mesh.load(it.sourcePath)) {
        g_menu.chatLines.push_back("[Shop] Equip fallito: file non trovato");
        return false;
    }
    for (auto ait = g_catalog.items.begin(); ait != g_catalog.items.end();) {
        if (ait->slot == acc.slot)
            ait = g_catalog.items.erase(ait);
        else
            ++ait;
    }
    if (acc.replacesBody)
        g_catalog.hideDefaultPart[(int)acc.slot] = true;
    g_catalog.items.push_back(std::move(acc));
    g_catalog.selected = (int)g_catalog.items.size() - 1;
    g_menu.chatLines.push_back(std::string("[Shop] Equipaggiato ") + it.name);
    saveEquippedAvatar(g_catalog);
    return true;
}

static bool unequipShopItem(int idx) {
    if (idx < 0 || idx >= (int)g_menu.shopItems.size()) return false;
    const auto& it = g_menu.shopItems[idx];
    bool removed = false;
    for (auto ait = g_catalog.items.begin(); ait != g_catalog.items.end();) {
        if ((!it.sourcePath.empty() && ait->sourcePath == it.sourcePath && (int)ait->slot == it.slot)
            || (ait->name == it.name && (int)ait->slot == it.slot)) {
            ait = g_catalog.items.erase(ait);
            removed = true;
        } else {
            ++ait;
        }
    }
    if (g_catalog.selected >= (int)g_catalog.items.size())
        g_catalog.selected = (int)g_catalog.items.size() - 1;
    if (removed)
        g_menu.chatLines.push_back(std::string("[Shop] Disequipaggiato ") + it.name);
    else
        g_menu.chatLines.push_back(std::string("[Shop] ") + it.name + " non era equipaggiato");
    saveEquippedAvatar(g_catalog);
    return removed;
}

static bool unequipEquippedIndex(int idx) {
    if (idx == -2) {
        int n = (int)g_catalog.items.size();
        g_catalog.items.clear();
        g_catalog.selected = -1;
        if (n > 0)
            g_menu.chatLines.push_back("[Avatar] Rimossi tutti gli accessori (" + std::to_string(n) + ")");
        saveEquippedAvatar(g_catalog);
        return n > 0;
    }
    if (idx < 0 || idx >= (int)g_catalog.items.size()) return false;
    std::string name = g_catalog.items[idx].name;
    g_catalog.items.erase(g_catalog.items.begin() + idx);
    if (g_catalog.selected == idx) g_catalog.selected = -1;
    else if (g_catalog.selected > idx) g_catalog.selected--;
    g_menu.chatLines.push_back("[Avatar] Disequipaggiato " + name);
    saveEquippedAvatar(g_catalog);
    return true;
}

static void syncEquippedToMenu() {
    g_menu.equippedList.clear();
    for (const auto& a : g_catalog.items) {
        ClassicMenuState::EquippedInfo e;
        e.name = a.name;
        e.slot = (int)a.slot;
        e.sourcePath = a.sourcePath;
        g_menu.equippedList.push_back(std::move(e));
    }
    for (auto& it : g_menu.shopItems) {
        it.equipped = false;
        if (it.sourcePath.empty() && it.kind == 0) continue;
        for (const auto& a : g_catalog.items) {
            bool pathMatch = !it.sourcePath.empty() && a.sourcePath == it.sourcePath
                && (it.kind == 1 || (int)a.slot == it.slot);
            bool nameMatch = it.kind == 0 && a.name == it.name && (int)a.slot == it.slot;
            if (pathMatch || nameMatch) {
                it.equipped = true;
                break;
            }
        }
    }
}
