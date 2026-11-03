#pragma once

// Terrain: voxel a chunk 16^3, greedy mesh, raycast DDA. Stesso ruolo di
// Workspace.Terrain in Luau: SetBlock/GetBlock/Fill/Generate senza una Part
// per ogni cubo, cosi' un mondo Minecraft-like resta giocabile.

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

class Terrain {
public:
    static constexpr int kChunk = 16;
    static constexpr int kMaxChunks = 384;
    static constexpr int kMaxY = 96;
    static constexpr int kMinXZ = -192;
    static constexpr int kMaxXZ = 191;
    static constexpr float kDefaultBlock = 3.f; // ~1 blocco Minecraft vs R6

    enum Id : uint8_t {
        Air = 0, Grass, Dirt, Stone, Sand, Wood, Leaves, Water,
        Snow, Cobble, Bedrock, Coal, Iron, Planks, Glass, Brick,
        Lava, Gravel, Gold, Diamond, Clay, Wool, Flower, Mushroom,
        Cactus, Obsidian, Moss, Torch, Tnt, Farmland, Wheat, Sponge,
        IronBlock, GoldBlock, DiamondBlock,
        Door, DoorOpen, Ladder, ChestBlk, Bed, Sapling, Reed, Count
    };

    struct Quad {
        glm::vec3 center{0};
        glm::vec3 size{1};
        glm::vec3 color{0.5f};
        uint8_t id = 0;
    };

    // Mesh "vera" di un chunk per il renderer: solo le facce esposte
    // (greedy 2D per slice, come Minecraft), vertici gia' in spazio mondo
    // (pos xyz + normale xyz, 6 float). Le facce sono raggruppate per
    // materiale cosi' il renderer fa UNA draw call per (chunk, materiale)
    // invece di una per cubo.
    struct MeshRange {
        uint8_t id = 0;
        int first = 0;   // primo vertice
        int count = 0;   // numero di vertici
    };
    struct ChunkMesh {
        std::vector<float> verts;
        std::vector<MeshRange> ranges;
        uint32_t version = 0;   // cresce a ogni rebuild: il renderer ricarica solo se cambia
        glm::vec3 bmin{0}, bmax{0};
    };

    struct Hit {
        bool ok = false;
        int x = 0, y = 0, z = 0;
        int nx = 0, ny = 0, nz = 0;
        uint8_t id = 0;
        float t = 0;
        glm::vec3 pos{0};
    };
    struct Fuse {
        int x = 0, y = 0, z = 0;
        float left = 0.8f;
    };

    struct Coord {
        int16_t x = 0, y = 0, z = 0;
        bool operator==(const Coord& o) const { return x == o.x && y == o.y && z == o.z; }
    };
    struct CoordHash {
        size_t operator()(const Coord& c) const {
            return (size_t)(uint16_t)c.x * 73856093u
                 ^ (size_t)(uint16_t)c.y * 19349663u
                 ^ (size_t)(uint16_t)c.z * 83492791u;
        }
    };

    void reset() {
        chunks_.clear();
        quads_.clear();
        primed_.clear();
        blockSize_ = kDefaultBlock;
        blockCount_ = 0;
        quadsStale_ = false;
        dirty_ = false;
        seed_ = 0;
        tickAcc_ = 0.f;
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        tickAcc_ += dt;
        if (tickAcc_ >= 0.12f) {
            tickAcc_ = 0.f;
            tickPhysics();
        }
        for (int i = (int)primed_.size() - 1; i >= 0; --i) {
            primed_[(size_t)i].left -= dt;
            if (primed_[(size_t)i].left > 0.f) continue;
            int x = primed_[(size_t)i].x, y = primed_[(size_t)i].y, z = primed_[(size_t)i].z;
            primed_.erase(primed_.begin() + i);
            explode(x, y, z, 3);
        }
        if (dirty_) rebuildMesh();
    }

    float blockSize() const { return blockSize_; }
    void setBlockSize(float s) { blockSize_ = std::clamp(s, 1.f, 8.f); dirty_ = true; }

    static const char* nameOf(uint8_t id) {
        static const char* n[] = {
            "Air", "Grass", "Dirt", "Stone", "Sand", "Wood", "Leaves", "Water",
            "Snow", "Cobble", "Bedrock", "Coal", "Iron", "Planks", "Glass", "Brick",
            "Lava", "Gravel", "Gold", "Diamond", "Clay", "Wool", "Flower", "Mushroom",
            "Cactus", "Obsidian", "Moss", "Torch", "Tnt", "Farmland", "Wheat", "Sponge",
            "IronBlock", "GoldBlock", "DiamondBlock",
            "Door", "DoorOpen", "Ladder", "Chest", "Bed", "Sapling", "Reed"
        };
        return id < Count ? n[id] : "Air";
    }

    static uint8_t parse(const std::string& s) {
        std::string t = s;
        for (char& c : t)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (t == "grass" || t == "erba") return Grass;
        if (t == "dirt" || t == "terra") return Dirt;
        if (t == "stone" || t == "pietra" || t == "rock") return Stone;
        if (t == "sand" || t == "sabbia") return Sand;
        if (t == "wood" || t == "log" || t == "legno") return Wood;
        if (t == "leaves" || t == "foglie") return Leaves;
        if (t == "water" || t == "acqua") return Water;
        if (t == "snow" || t == "neve") return Snow;
        if (t == "cobble" || t == "cobblestone") return Cobble;
        if (t == "bedrock") return Bedrock;
        if (t == "coal") return Coal;
        if (t == "iron" || t == "ferro") return Iron;
        if (t == "planks" || t == "tavole") return Planks;
        if (t == "glass" || t == "vetro") return Glass;
        if (t == "brick" || t == "mattone") return Brick;
        if (t == "lava") return Lava;
        if (t == "gravel" || t == "ghiaia") return Gravel;
        if (t == "gold" || t == "oro") return Gold;
        if (t == "diamond" || t == "diamante") return Diamond;
        if (t == "clay" || t == "argilla") return Clay;
        if (t == "wool" || t == "lana") return Wool;
        if (t == "flower" || t == "fiore") return Flower;
        if (t == "mushroom" || t == "fungo") return Mushroom;
        if (t == "cactus") return Cactus;
        if (t == "obsidian" || t == "ossidiana") return Obsidian;
        if (t == "moss" || t == "muschio") return Moss;
        if (t == "torch" || t == "torcia") return Torch;
        if (t == "tnt") return Tnt;
        if (t == "farmland" || t == "zolla") return Farmland;
        if (t == "wheat" || t == "grano") return Wheat;
        if (t == "sponge" || t == "spugna") return Sponge;
        if (t == "ironblock" || t == "bloccoferro") return IronBlock;
        if (t == "goldblock" || t == "bloccoro") return GoldBlock;
        if (t == "diamondblock" || t == "bloccodiamante") return DiamondBlock;
        if (t == "door" || t == "porta") return Door;
        if (t == "dooropen" || t == "portaaperta") return DoorOpen;
        if (t == "ladder" || t == "scala") return Ladder;
        if (t == "chest" || t == "cassa") return ChestBlk;
        if (t == "bed" || t == "letto") return Bed;
        if (t == "sapling" || t == "germoglio") return Sapling;
        if (t == "reed" || t == "canna" || t == "sugarcane") return Reed;
        if (t == "air" || t == "0") return Air;
        int n = 0;
        for (char c : t) {
            if (c < '0' || c > '9') return Air;
            n = n * 10 + (c - '0');
        }
        return (uint8_t)std::clamp(n, 0, (int)Count - 1);
    }

    static glm::vec3 colorOf(uint8_t id) {
        switch (id) {
        case Grass: return {0.33f, 0.62f, 0.22f};
        case Dirt: return {0.45f, 0.30f, 0.16f};
        case Stone: return {0.48f, 0.48f, 0.50f};
        case Sand: return {0.86f, 0.80f, 0.55f};
        case Wood: return {0.40f, 0.26f, 0.12f};
        case Leaves: return {0.22f, 0.48f, 0.16f};
        case Water: return {0.18f, 0.38f, 0.78f};
        case Snow: return {0.92f, 0.95f, 0.98f};
        case Cobble: return {0.40f, 0.40f, 0.42f};
        case Bedrock: return {0.18f, 0.18f, 0.20f};
        case Coal: return {0.22f, 0.22f, 0.24f};
        case Iron: return {0.72f, 0.70f, 0.64f};
        case Planks: return {0.62f, 0.48f, 0.26f};
        case Glass: return {0.70f, 0.85f, 0.90f};
        case Brick: return {0.58f, 0.28f, 0.22f};
        case Lava: return {0.95f, 0.38f, 0.08f};
        case Gravel: return {0.52f, 0.50f, 0.48f};
        case Gold: return {0.92f, 0.78f, 0.22f};
        case Diamond: return {0.35f, 0.85f, 0.88f};
        case Clay: return {0.62f, 0.48f, 0.42f};
        case Wool: return {0.92f, 0.92f, 0.94f};
        case Flower: return {0.90f, 0.75f, 0.15f};
        case Mushroom: return {0.72f, 0.22f, 0.18f};
        case Cactus: return {0.18f, 0.55f, 0.18f};
        case Obsidian: return {0.10f, 0.06f, 0.18f};
        case Moss: return {0.38f, 0.52f, 0.32f};
        case Torch: return {1.00f, 0.72f, 0.22f};
        case Tnt: return {0.82f, 0.18f, 0.12f};
        case Farmland: return {0.38f, 0.24f, 0.12f};
        case Wheat: return {0.78f, 0.70f, 0.22f};
        case Sponge: return {0.90f, 0.88f, 0.32f};
        case IronBlock: return {0.82f, 0.82f, 0.86f};
        case GoldBlock: return {0.98f, 0.84f, 0.22f};
        case DiamondBlock: return {0.42f, 0.92f, 0.90f};
        case Door: return {0.55f, 0.34f, 0.14f};
        case DoorOpen: return {0.62f, 0.40f, 0.18f};
        case Ladder: return {0.50f, 0.32f, 0.12f};
        case ChestBlk: return {0.72f, 0.50f, 0.16f};
        case Bed: return {0.78f, 0.18f, 0.18f};
        case Sapling: return {0.28f, 0.62f, 0.18f};
        case Reed: return {0.40f, 0.70f, 0.22f};
        default: return {0.7f, 0.7f, 0.7f};
        }
    }

    static bool solid(uint8_t id) {
        return id != Air && id != Water && id != Lava && id != Torch
            && id != Flower && id != Mushroom && id != Wheat
            && id != DoorOpen && id != Ladder && id != Sapling && id != Reed;
    }
    static bool opaque(uint8_t id) {
        return solid(id) && id != Glass && id != Leaves;
    }
    static bool fluid(uint8_t id) { return id == Water || id == Lava; }
    static bool gravity(uint8_t id) { return id == Sand || id == Gravel; }
    static int lightEmit(uint8_t id) {
        if (id == Torch) return 14;
        if (id == Lava) return 12;
        if (id == Tnt) return 2;
        return 0;
    }
    static float hardness(uint8_t id) {
        switch (id) {
        case Air: case Flower: case Mushroom: case Torch: case Wheat:
        case Sapling: case Reed: return 0.05f;
        case Leaves: case Wool: case Ladder: return 0.25f;
        case Grass: case Dirt: case Sand: case Gravel: case Clay: case Farmland: return 0.5f;
        case Wood: case Planks: case Cactus: case Door: case DoorOpen:
        case ChestBlk: case Bed: return 1.0f;
        case Stone: case Cobble: case Moss: case Coal: case Brick: return 1.5f;
        case Iron: case Gold: return 2.5f;
        case Diamond: case IronBlock: case GoldBlock: return 3.0f;
        case DiamondBlock: return 4.0f;
        case Obsidian: return 8.0f;
        case Bedrock: return 100.f;
        case Water: case Lava: return 100.f;
        default: return 0.8f;
        }
    }
    static uint8_t dropOf(uint8_t id) {
        if (id == Grass || id == Farmland) return Dirt;
        if (id == Stone) return Cobble;
        if (id == Coal) return Coal;
        if (id == Iron) return Iron;
        if (id == Gold) return Gold;
        if (id == Diamond) return Diamond;
        if (id == Leaves) return Air;
        if (id == Bedrock) return Air;
        if (id == Wheat) return Wheat;
        if (id == DoorOpen) return Door;
        return id;
    }
    static float breakTime(uint8_t id, const char* tool) {
        float h = hardness(id);
        if (h >= 50.f) return -1.f;
        float mul = 1.f;
        std::string t = tool ? tool : "";
        for (char& c : t)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (t == "woodpick" || t == "wood_pickaxe") mul = 2.f;
        else if (t == "stonepick" || t == "stone_pickaxe") mul = 4.f;
        else if (t == "ironpick" || t == "iron_pickaxe") mul = 6.f;
        else if (t == "diamondpick" || t == "diamond_pickaxe") mul = 8.f;
        else if (t == "shovel" || t == "pala") mul = 3.f;
        else if (t == "axe" || t == "ascia") mul = 3.f;
        else if (t == "hand" || t.empty()) mul = 1.f;
        return std::max(0.05f, h / mul);
    }

    bool inBounds(int x, int y, int z) const {
        return x >= kMinXZ && x <= kMaxXZ && z >= kMinXZ && z <= kMaxXZ
            && y >= 0 && y < kMaxY;
    }

    uint8_t get(int x, int y, int z) const {
        if (!inBounds(x, y, z)) return Air;
        const Chunk* c = find(floorDiv(x), floorDiv(y), floorDiv(z));
        if (!c) return Air;
        return c->at(local(x), local(y), local(z));
    }

    bool set(int x, int y, int z, uint8_t id) {
        if (!inBounds(x, y, z)) return false;
        if (id >= Count) id = Air;
        Chunk* c = touch(floorDiv(x), floorDiv(y), floorDiv(z));
        if (!c) return false;
        uint8_t& cell = c->at(local(x), local(y), local(z));
        if (cell == id) return true;
        if (cell == Air) ++blockCount_;
        else if (id == Air) --blockCount_;
        cell = id;
        c->dirty = true;
        c->quadsDirty = true;
        dirty_ = true;
        quadsStale_ = true;
        // Le facce sul bordo dipendono dal vicino: anche lui va rifatto.
        const int cx = floorDiv(x), cy = floorDiv(y), cz = floorDiv(z);
        const int lx = local(x), ly = local(y), lz = local(z);
        auto mark = [&](int dx, int dy, int dz) {
            if (Chunk* n = findMut(cx + dx, cy + dy, cz + dz)) n->dirty = true;
        };
        if (lx == 0) mark(-1, 0, 0);
        if (lx == kChunk - 1) mark(1, 0, 0);
        if (ly == 0) mark(0, -1, 0);
        if (ly == kChunk - 1) mark(0, 1, 0);
        if (lz == 0) mark(0, 0, -1);
        if (lz == kChunk - 1) mark(0, 0, 1);
        return true;
    }

    int fill(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t id) {
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        if (z0 > z1) std::swap(z0, z1);
        x0 = std::max(x0, kMinXZ); x1 = std::min(x1, kMaxXZ);
        y0 = std::max(y0, 0); y1 = std::min(y1, kMaxY - 1);
        z0 = std::max(z0, kMinXZ); z1 = std::min(z1, kMaxXZ);
        int n = 0;
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                    if (set(x, y, z, id)) ++n;
        return n;
    }

    glm::ivec3 worldToCell(glm::vec3 p) const {
        float s = blockSize_;
        return { (int)std::floor(p.x / s), (int)std::floor(p.y / s), (int)std::floor(p.z / s) };
    }

    glm::vec3 cellToWorld(int x, int y, int z) const {
        float s = blockSize_;
        return { (x + 0.5f) * s, (y + 0.5f) * s, (z + 0.5f) * s };
    }

    /** Enum.Material di Roblox Studio -> voxel interno. */
    static uint8_t materialFromEnum(const std::string& s) {
        std::string t = s;
        for (char& c : t)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (t == "air" || t == "empty") return Air;
        if (t == "grass") return Grass;
        if (t == "ground") return Dirt;
        if (t == "sand") return Sand;
        if (t == "rock" || t == "slate" || t == "basalt") return Stone;
        if (t == "water") return Water;
        if (t == "snow" || t == "glacier") return Snow;
        if (t == "wood") return Wood;
        if (t == "leafygrass" || t == "leaves") return Leaves;
        if (t == "mud") return Dirt;
        if (t == "crackedlava" || t == "lava") return Lava;
        if (t == "ice" || t == "salt" || t == "marble") return Snow;
        if (t == "limestone" || t == "sandstone") return Sand;
        if (t == "cobblestone" || t == "pavement" || t == "granite") return Cobble;
        if (t == "woodplanks") return Planks;
        if (t == "glass") return Glass;
        if (t == "brick") return Brick;
        if (t == "pebble") return Gravel;
        if (t == "metal" || t == "diamondplate" || t == "corrodedmetal") return IronBlock;
        if (t == "asphalt" || t == "concrete") return Bedrock;
        if (t == "fabric") return Wool;
        return parse(s);
    }

    static const char* enumMaterialName(uint8_t id) {
        switch (id) {
        case Grass: return "Grass";
        case Dirt: return "Ground";
        case Stone: return "Rock";
        case Sand: return "Sand";
        case Water: return "Water";
        case Snow: return "Snow";
        case Wood: return "Wood";
        case Leaves: return "LeafyGrass";
        case Lava: return "CrackedLava";
        case Cobble: return "Cobblestone";
        case Planks: return "WoodPlanks";
        case Gravel: return "Pebble";
        default: return nameOf(id);
        }
    }

    int fillBlockWorld(glm::vec3 center, glm::vec3 sizeStuds, uint8_t id) {
        glm::vec3 h = glm::max(sizeStuds * 0.5f, glm::vec3(0.05f));
        glm::ivec3 a = worldToCell(center - h);
        glm::ivec3 b = worldToCell(center + h - glm::vec3(0.001f));
        return fill(a.x, a.y, a.z, b.x, b.y, b.z, id);
    }

    int fillBallWorld(glm::vec3 center, float radius, uint8_t id) {
        if (radius <= 0.f) return 0;
        glm::ivec3 a = worldToCell(center - glm::vec3(radius));
        glm::ivec3 b = worldToCell(center + glm::vec3(radius));
        int n = 0;
        float r2 = radius * radius;
        for (int y = a.y; y <= b.y; ++y)
            for (int z = a.z; z <= b.z; ++z)
                for (int x = a.x; x <= b.x; ++x) {
                    glm::vec3 c = cellToWorld(x, y, z);
                    if (glm::dot(c - center, c - center) <= r2 && set(x, y, z, id)) ++n;
                }
        return n;
    }

    glm::vec3 cellAabbMin(int x, int y, int z) const {
        float s = blockSize_;
        return { x * s, y * s, z * s };
    }
    glm::vec3 cellAabbMax(int x, int y, int z) const {
        float s = blockSize_;
        return { (x + 1) * s, (y + 1) * s, (z + 1) * s };
    }

    // Ritorna l'altezza di spawn (stud) sopra il blocco al centro.
    float generate(int seed, int radius, int maxH) {
        reset();
        seed_ = seed;
        radius = std::clamp(radius, 8, 80);
        maxH = std::clamp(maxH, 8, kMaxY - 2);
        int water = std::max(4, maxH / 5);
        for (int z = -radius; z < radius; ++z) {
            for (int x = -radius; x < radius; ++x) {
                float n = fbm((x + seed * 0.01f) * 0.06f, (z - seed * 0.013f) * 0.06f);
                int h = 3 + (int)std::floor((n * 0.5f + 0.5f) * (float)(maxH - 4));
                h = std::clamp(h, 2, maxH);
                uint8_t top = Grass;
                if (h <= water + 1) top = Sand;
                if (n < -0.35f) top = Sand;
                if (h > maxH - 4 && n > 0.45f) top = Snow;
                set(x, 0, z, Bedrock);
                for (int y = 1; y < h; ++y) {
                    uint8_t id = Stone;
                    if (y >= h - 3) id = Dirt;
                    if (y >= h - 2 && top == Sand) id = Sand;
                    uint32_t hv = hash3(x, y, z);
                    if (y < h - 4 && (hv & 31) == 0) id = Coal;
                    if (y < 8 && (hv & 63) == 7) id = Iron;
                    if (y < 6 && (hv % 90) == 3) id = Gold;
                    if (y < 5 && (hv % 140) == 11) id = Diamond;
                    if (y < h - 2 && (hv % 40) == 5) id = Gravel;
                    set(x, y, z, id);
                }
                set(x, h, z, top);
                if (h < water)
                    for (int y = h + 1; y <= water; ++y)
                        set(x, y, z, Water);
                if (top == Grass && (hash3(x, h, z) % 47) == 0 && h + 5 < kMaxY)
                    tree(x, h + 1, z);
                if (top == Grass && (hash3(x, h, z) % 23) == 0)
                    set(x, h + 1, z, Flower);
                if (top == Sand && (hash3(x, h, z) % 31) == 0)
                    cactus(x, h + 1, z);
            }
        }
        carveCaves(radius, maxH);
        dirty_ = true;
        rebuildMesh();
        int cy = 0;
        for (int y = kMaxY - 1; y >= 0; --y) {
            if (solid(get(0, y, 0))) { cy = y + 1; break; }
        }
        return (cy + 0.15f) * blockSize_;
    }

    Hit raycast(glm::vec3 origin, glm::vec3 dir, float maxDist) const {
        Hit h;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (len < 1e-6f) return h;
        dir /= len;
        maxDist = maxDist > 0.f ? maxDist : 80.f;
        float s = blockSize_;
        glm::vec3 p = origin / s;
        int x = (int)std::floor(p.x), y = (int)std::floor(p.y), z = (int)std::floor(p.z);
        glm::ivec3 step{
            dir.x >= 0.f ? 1 : -1,
            dir.y >= 0.f ? 1 : -1,
            dir.z >= 0.f ? 1 : -1
        };
        auto tDelta = [](float d) {
            float a = std::fabs(d);
            return a < 1e-8f ? 1e30f : 1.f / a;
        };
        glm::vec3 delta{ tDelta(dir.x), tDelta(dir.y), tDelta(dir.z) };
        auto tSide = [&](float pos, float d, int cell, int st) {
            if (std::fabs(d) < 1e-8f) return 1e30f;
            float edge = (st > 0) ? (float)(cell + 1) : (float)cell;
            return (edge - pos) / d;
        };
        glm::vec3 tmax{
            tSide(p.x, dir.x, x, step.x),
            tSide(p.y, dir.y, y, step.y),
            tSide(p.z, dir.z, z, step.z)
        };
        glm::ivec3 n{0, 0, 0};
        float t = 0.f;
        for (int i = 0; i < 256; ++i) {
            if (t > maxDist / s) break;
            uint8_t id = get(x, y, z);
            if (solid(id)) {
                h.ok = true;
                h.x = x; h.y = y; h.z = z;
                h.nx = n.x; h.ny = n.y; h.nz = n.z;
                h.id = id;
                h.t = t * s;
                h.pos = origin + dir * h.t;
                return h;
            }
            if (tmax.x < tmax.y && tmax.x < tmax.z) {
                t = tmax.x;
                tmax.x += delta.x;
                x += step.x;
                n = { -step.x, 0, 0 };
            } else if (tmax.y < tmax.z) {
                t = tmax.y;
                tmax.y += delta.y;
                y += step.y;
                n = { 0, -step.y, 0 };
            } else {
                t = tmax.z;
                tmax.z += delta.z;
                z += step.z;
                n = { 0, 0, -step.z };
            }
        }
        return h;
    }

    void collideAabbs(const glm::vec3& pmin, const glm::vec3& pmax,
                      std::vector<glm::vec3>& mins, std::vector<glm::vec3>& maxs) const {
        if (chunks_.empty()) return;
        glm::ivec3 a = worldToCell(pmin - glm::vec3(0.05f));
        glm::ivec3 b = worldToCell(pmax + glm::vec3(0.05f));
        for (int y = a.y; y <= b.y; ++y)
            for (int z = a.z; z <= b.z; ++z)
                for (int x = a.x; x <= b.x; ++x) {
                    if (!solid(get(x, y, z))) continue;
                    mins.push_back(cellAabbMin(x, y, z));
                    maxs.push_back(cellAabbMax(x, y, z));
                }
    }

    int surfaceY(int x, int z) const {
        for (int y = kMaxY - 1; y >= 0; --y)
            if (solid(get(x, y, z))) return y;
        return 0;
    }

    int getLight(int x, int y, int z) const {
        int sky = 15;
        for (int yy = y + 1; yy < kMaxY; ++yy) {
            if (opaque(get(x, yy, z))) { sky = 0; break; }
        }
        int blk = lightEmit(get(x, y, z));
        const int d[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (int n = 0; n < 6; ++n) {
            int e = lightEmit(get(x + d[n][0], y + d[n][1], z + d[n][2]));
            if (e > blk) blk = std::max(0, e - 1);
        }
        return std::max(sky, blk);
    }

    bool place(int x, int y, int z, uint8_t id) {
        uint8_t cur = get(x, y, z);
        if (cur != Air && !fluid(cur) && cur != Flower && cur != Wheat) return false;
        if (id == Torch || id == Flower || id == Wheat || id == Sapling || id == Reed) {
            if (!solid(get(x, y - 1, z)) && get(x, y - 1, z) != Reed) return false;
        }
        return set(x, y, z, id);
    }

    uint8_t mine(int x, int y, int z) {
        uint8_t id = get(x, y, z);
        if (id == Air || id == Bedrock) return Air;
        uint8_t drop = dropOf(id);
        set(x, y, z, Air);
        return drop;
    }

    int explode(int cx, int cy, int cz, int r) {
        r = std::clamp(r, 1, 8);
        int n = 0;
        for (int y = cy - r; y <= cy + r; ++y)
            for (int z = cz - r; z <= cz + r; ++z)
                for (int x = cx - r; x <= cx + r; ++x) {
                    int dx = x - cx, dy = y - cy, dz = z - cz;
                    if (dx * dx + dy * dy + dz * dz > r * r) continue;
                    uint8_t id = get(x, y, z);
                    if (id == Air || id == Bedrock || id == Obsidian) continue;
                    if (id == Tnt) primeTnt(x, y, z);
                    set(x, y, z, Air);
                    ++n;
                }
        return n;
    }

    bool primeTnt(int x, int y, int z) {
        if (get(x, y, z) != Tnt && get(x, y, z) != Air) {
            set(x, y, z, Tnt);
        }
        if (get(x, y, z) != Tnt) return false;
        for (const Fuse& f : primed_)
            if (f.x == x && f.y == y && f.z == z) return true;
        primed_.push_back({x, y, z, 0.8f});
        return true;
    }

    bool save(const std::string& name) const {
        std::string p = safeName(name);
        if (p.empty()) return false;
        std::ofstream out(p, std::ios::binary);
        if (!out) return false;
        const char mag[4] = {'B', 'L', 'K', 'V'};
        out.write(mag, 4);
        int32_t ver = 1, seed = seed_;
        float bs = blockSize_;
        out.write((char*)&ver, 4);
        out.write((char*)&seed, 4);
        out.write((char*)&bs, 4);
        for (const auto& kv : chunks_) {
            int32_t ox = (int)kv.first.x * kChunk;
            int32_t oy = (int)kv.first.y * kChunk;
            int32_t oz = (int)kv.first.z * kChunk;
            for (int y = 0; y < kChunk; ++y)
                for (int z = 0; z < kChunk; ++z)
                    for (int x = 0; x < kChunk; ++x) {
                        uint8_t id = kv.second.at(x, y, z);
                        if (id == Air) continue;
                        int32_t wx = ox + x, wy = oy + y, wz = oz + z;
                        out.write((char*)&wx, 4);
                        out.write((char*)&wy, 4);
                        out.write((char*)&wz, 4);
                        out.write((char*)&id, 1);
                    }
        }
        return (bool)out;
    }

    bool load(const std::string& name) {
        std::string p = safeName(name);
        if (p.empty()) return false;
        std::ifstream in(p, std::ios::binary);
        if (!in) return false;
        char mag[4]{};
        in.read(mag, 4);
        if (mag[0] != 'B' || mag[1] != 'L' || mag[2] != 'K' || mag[3] != 'V') return false;
        int32_t ver = 0, seed = 0;
        float bs = kDefaultBlock;
        in.read((char*)&ver, 4);
        in.read((char*)&seed, 4);
        in.read((char*)&bs, 4);
        if (!in) return false;
        reset();
        seed_ = seed;
        blockSize_ = bs > 0.5f ? bs : kDefaultBlock;
        while (in) {
            int32_t x = 0, y = 0, z = 0;
            uint8_t id = 0;
            in.read((char*)&x, 4);
            in.read((char*)&y, 4);
            in.read((char*)&z, 4);
            in.read((char*)&id, 1);
            if (!in) break;
            set(x, y, z, id);
        }
        dirty_ = true;
        rebuildMesh();
        return true;
    }

    int seed() const { return seed_; }

    const char* biome(int x, int z) const {
        float n = fbm((x + seed_ * 0.01f) * 0.06f, (z - seed_ * 0.013f) * 0.06f);
        int h = surfaceY(x, z);
        if (h <= 5) return "ocean";
        if (n < -0.28f) return "desert";
        if (n > 0.42f) return "taiga";
        return "plains";
    }

    bool grow(int x, int y, int z) {
        uint8_t id = get(x, y, z);
        if (id == Sapling) {
            set(x, y, z, Air);
            tree(x, y, z);
            return true;
        }
        if (id == Farmland && get(x, y + 1, z) == Air) {
            set(x, y + 1, z, Wheat);
            return true;
        }
        if (id == Reed && y + 1 < kMaxY && get(x, y + 1, z) == Air) {
            set(x, y + 1, z, Reed);
            return true;
        }
        return id == Wheat;
    }

    bool toggleDoor(int x, int y, int z) {
        uint8_t id = get(x, y, z);
        if (id == Door) return set(x, y, z, DoorOpen);
        if (id == DoorOpen) return set(x, y, z, Door);
        return false;
    }

    bool replace(int x, int y, int z, uint8_t from, uint8_t to) {
        if (get(x, y, z) != from) return false;
        return set(x, y, z, to);
    }

    static bool climbable(uint8_t id) { return id == Ladder; }

    const char* use(int x, int y, int z) {
        uint8_t id = get(x, y, z);
        if (id == Tnt) { primeTnt(x, y, z); return "tnt"; }
        if (id == Door || id == DoorOpen) { toggleDoor(x, y, z); return "door"; }
        if (id == ChestBlk) return "chest";
        if (id == Bed) return "bed";
        if (id == Farmland || id == Sapling) { grow(x, y, z); return "grow"; }
        return "";
    }

    // Box greedy (vecchio formato, usato da tool/test): si calcolano solo se
    // qualcuno li chiede, il renderer usa le ChunkMesh a facce.
    const std::vector<Quad>& quads() {
        if (dirty_) rebuildMesh();
        if (quadsStale_) {
            quadsStale_ = false;
            size_t total = 0;
            for (auto& kv : chunks_) {
                if (kv.second.quadsDirty) {
                    kv.second.quadsDirty = false;
                    emitChunk(kv.first, kv.second);
                }
                total += kv.second.quads.size();
            }
            quads_.clear();
            quads_.reserve(total);
            for (auto& kv : chunks_)
                quads_.insert(quads_.end(), kv.second.quads.begin(), kv.second.quads.end());
        }
        return quads_;
    }
    /** Itera le mesh per-chunk (facce esposte): fn(const Coord&, const ChunkMesh&). */
    template <class F>
    void forEachChunkMesh(F&& fn) {
        if (dirty_) rebuildMesh();
        for (const auto& kv : chunks_)
            fn(kv.first, kv.second.mesh);
    }
    size_t meshVertexCount() {
        if (dirty_) rebuildMesh();
        size_t n = 0;
        for (const auto& kv : chunks_) n += kv.second.mesh.verts.size() / 6;
        return n;
    }
    int chunkCount() const { return (int)chunks_.size(); }
    // Contatore incrementale (tutte le scritture passano da set()): prima
    // scorreva tutti i chunk, e il pannello Studio lo chiamava a ogni frame.
    int blockCount() const { return blockCount_; }
    bool empty() const { return chunks_.empty(); }

    static float noise(float x, float y, float z) {
        return perlin3(x, y, z);
    }

private:
    struct Chunk {
        uint8_t blocks[kChunk * kChunk * kChunk]{};
        bool dirty = true;        // mesh a facce (renderer) da rifare
        bool quadsDirty = true;   // box greedy (quads()) da rifare
        // Mesh greedy di QUESTO chunk: si rigenera solo se dirty, cosi'
        // modificare un blocco non rifà tutto il mondo.
        std::vector<Quad> quads;
        ChunkMesh mesh;
        uint8_t& at(int x, int y, int z) {
            return blocks[x + kChunk * (z + kChunk * y)];
        }
        uint8_t at(int x, int y, int z) const {
            return blocks[x + kChunk * (z + kChunk * y)];
        }
    };

    static int floorDiv(int v) {
        return (int)std::floor((float)v / (float)kChunk);
    }
    static int local(int v) {
        int r = v % kChunk;
        return r < 0 ? r + kChunk : r;
    }

    const Chunk* find(int cx, int cy, int cz) const {
        Coord k{ (int16_t)cx, (int16_t)cy, (int16_t)cz };
        auto it = chunks_.find(k);
        return it == chunks_.end() ? nullptr : &it->second;
    }
    Chunk* findMut(int cx, int cy, int cz) {
        Coord k{ (int16_t)cx, (int16_t)cy, (int16_t)cz };
        auto it = chunks_.find(k);
        return it == chunks_.end() ? nullptr : &it->second;
    }
    Chunk* touch(int cx, int cy, int cz) {
        Coord k{ (int16_t)cx, (int16_t)cy, (int16_t)cz };
        auto it = chunks_.find(k);
        if (it != chunks_.end()) return &it->second;
        if ((int)chunks_.size() >= kMaxChunks) return nullptr;
        return &chunks_[k];
    }

    static std::string safeName(const std::string& name) {
        if (name.empty() || name.size() > 40) return {};
        for (char c : name) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
                return {};
        }
        if (name.find("..") != std::string::npos) return {};
        return name + (name.size() > 4 && name.substr(name.size() - 4) == ".blk" ? "" : ".blk");
    }

    void cactus(int x, int y, int z) {
        int h = 1 + (int)(hash3(x, y, z) % 3);
        for (int i = 0; i < h; ++i) set(x, y + i, z, Cactus);
    }

    void carveCaves(int radius, int maxH) {
        for (int z = -radius; z < radius; ++z)
            for (int x = -radius; x < radius; ++x)
                for (int y = 2; y < maxH - 2; ++y) {
                    uint8_t id = get(x, y, z);
                    if (id != Stone && id != Dirt && id != Coal && id != Gravel) continue;
                    float n = perlin3(x * 0.09f, y * 0.12f, z * 0.09f);
                    if (n > 0.52f) set(x, y, z, Air);
                }
    }

    void tickPhysics() {
        int steps = 0;
        std::vector<Coord> cells;
        cells.reserve(64);
        for (const auto& kv : chunks_) {
            int ox = (int)kv.first.x * kChunk, oy = (int)kv.first.y * kChunk, oz = (int)kv.first.z * kChunk;
            for (int y = 0; y < kChunk; ++y)
                for (int z = 0; z < kChunk; ++z)
                    for (int x = 0; x < kChunk; ++x) {
                        uint8_t id = kv.second.at(x, y, z);
                        if (!gravity(id) && !fluid(id) && id != Sapling && id != Farmland && id != Reed)
                            continue;
                        cells.push_back({(int16_t)(ox + x), (int16_t)(oy + y), (int16_t)(oz + z)});
                    }
        }
        for (const Coord& c : cells) {
            if (steps > 80) break;
            uint8_t id = get(c.x, c.y, c.z);
            if ((id == Sapling || id == Farmland || id == Reed)
                && (hash3(c.x, c.y, c.z + (int)tickAcc_) % 20) == 0) {
                grow(c.x, c.y, c.z);
                ++steps;
                continue;
            }
            if (gravity(id) && get(c.x, c.y - 1, c.z) == Air) {
                set(c.x, c.y - 1, c.z, id);
                set(c.x, c.y, c.z, Air);
                ++steps;
                continue;
            }
            if (!fluid(id)) continue;
            if (get(c.x, c.y - 1, c.z) == Air) {
                set(c.x, c.y - 1, c.z, id);
                ++steps;
                continue;
            }
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int i = 0; i < 4; ++i) {
                int nx = c.x + d[i][0], nz = c.z + d[i][1];
                if (get(nx, c.y, nz) != Air) continue;
                set(nx, c.y, nz, id);
                ++steps;
                break;
            }
        }
    }

    void tree(int x, int y, int z) {
        int h = 3 + (hash3(x, y, z) % 3);
        for (int i = 0; i < h; ++i) set(x, y + i, z, Wood);
        int top = y + h;
        for (int dy = -1; dy <= 2; ++dy)
            for (int dz = -2; dz <= 2; ++dz)
                for (int dx = -2; dx <= 2; ++dx) {
                    if (std::abs(dx) == 2 && std::abs(dz) == 2 && dy != 0) continue;
                    if (dx == 0 && dz == 0 && dy <= 0) continue;
                    set(x + dx, top + dy, z + dz, Leaves);
                }
    }

    void rebuildMesh() {
        dirty_ = false;
        for (auto& kv : chunks_) {
            if (kv.second.dirty) {
                kv.second.dirty = false;
                emitChunkFaces(kv.first, kv.second);
            }
        }
    }

    void emitChunk(const Coord& c, Chunk& ch) {
        ch.quads.clear();
        const int ox = (int)c.x * kChunk, oy = (int)c.y * kChunk, oz = (int)c.z * kChunk;
        bool used[kChunk * kChunk * kChunk]{};
        for (int y = 0; y < kChunk; ++y)
            for (int z = 0; z < kChunk; ++z)
                for (int x = 0; x < kChunk; ++x) {
                    int i = x + kChunk * (z + kChunk * y);
                    uint8_t id = ch.blocks[i];
                    if (id == Air || used[i]) continue;
                    int w = 1, d = 1, h = 1;
                    while (x + w < kChunk && ch.at(x + w, y, z) == id
                           && !used[(x + w) + kChunk * (z + kChunk * y)])
                        ++w;
                    bool row = true;
                    while (z + d < kChunk && row) {
                        for (int xx = 0; xx < w; ++xx) {
                            if (ch.at(x + xx, y, z + d) != id
                                || used[(x + xx) + kChunk * ((z + d) + kChunk * y)]) {
                                row = false;
                                break;
                            }
                        }
                        if (row) ++d;
                    }
                    bool slab = true;
                    while (y + h < kChunk && slab) {
                        for (int zz = 0; zz < d && slab; ++zz)
                            for (int xx = 0; xx < w; ++xx) {
                                if (ch.at(x + xx, y + h, z + zz) != id
                                    || used[(x + xx) + kChunk * ((z + zz) + kChunk * (y + h))]) {
                                    slab = false;
                                    break;
                                }
                            }
                        if (slab) ++h;
                    }
                    for (int yy = 0; yy < h; ++yy)
                        for (int zz = 0; zz < d; ++zz)
                            for (int xx = 0; xx < w; ++xx)
                                used[(x + xx) + kChunk * ((z + zz) + kChunk * (y + yy))] = true;
                    float s = blockSize_;
                    Quad q;
                    q.size = { w * s, h * s, d * s };
                    q.center = {
                        (ox + x) * s + q.size.x * 0.5f,
                        (oy + y) * s + q.size.y * 0.5f,
                        (oz + z) * s + q.size.z * 0.5f
                    };
                    q.color = colorOf(id);
                    q.id = id;
                    ch.quads.push_back(q);
                }
    }

    // Una faccia del blocco `a` verso il vicino `b` si vede?
    static bool faceVisible(uint8_t a, uint8_t b) {
        if (b == Air) return true;
        if (opaque(b)) return false;
        if (!opaque(a)) return a != b;   // acqua-acqua, vetro-vetro, foglie-foglie: niente faccia interna
        return true;
    }
    // Blocchi "decorazione" (fiori, torce...): non pieni, resi come scatoline.
    static bool decor(uint8_t id) {
        return id != Air && !solid(id) && !fluid(id);
    }

    static void pushVert(std::vector<float>& v, const glm::vec3& p, const glm::vec3& n) {
        v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
        v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
    }
    // Quad p0..p3 in senso antiorario visto dal lato della normale.
    static void pushQuad(std::vector<float>& v, const glm::vec3& p0, const glm::vec3& p1,
                         const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& n) {
        pushVert(v, p0, n); pushVert(v, p1, n); pushVert(v, p2, n);
        pushVert(v, p0, n); pushVert(v, p2, n); pushVert(v, p3, n);
    }
    static void pushBox(std::vector<float>& v, const glm::vec3& c, const glm::vec3& h) {
        const glm::vec3 a = c - h, b = c + h;
        pushQuad(v, {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}, {0, 0, 1});
        pushQuad(v, {b.x, a.y, a.z}, {a.x, a.y, a.z}, {a.x, b.y, a.z}, {b.x, b.y, a.z}, {0, 0, -1});
        pushQuad(v, {b.x, a.y, b.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, {1, 0, 0});
        pushQuad(v, {a.x, a.y, a.z}, {a.x, a.y, b.z}, {a.x, b.y, b.z}, {a.x, b.y, a.z}, {-1, 0, 0});
        pushQuad(v, {a.x, b.y, b.z}, {b.x, b.y, b.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z}, {0, 1, 0});
        pushQuad(v, {a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}, {0, -1, 0});
    }

    // Mesh a facce esposte con greedy 2D per ogni slice/direzione.
    void emitChunkFaces(const Coord& c, Chunk& ch) {
        ChunkMesh& m = ch.mesh;
        m.verts.clear();
        m.ranges.clear();
        const float s = blockSize_;
        const int ox = (int)c.x * kChunk, oy = (int)c.y * kChunk, oz = (int)c.z * kChunk;
        m.bmin = glm::vec3((float)ox, (float)oy, (float)oz) * s;
        m.bmax = m.bmin + glm::vec3((float)kChunk * s);

        std::vector<float> perId[Count];

        // d = asse della normale, u/v = gli altri due (ciclici: e_u x e_v = e_d)
        for (int d = 0; d < 3; ++d) {
            const int u = (d + 1) % 3, v = (d + 2) % 3;
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                for (int k = 0; k < kChunk; ++k) {
                    uint8_t mask[kChunk * kChunk]{};
                    for (int j = 0; j < kChunk; ++j)
                        for (int i = 0; i < kChunk; ++i) {
                            int p[3]; p[d] = k; p[u] = i; p[v] = j;
                            uint8_t a = ch.at(p[0], p[1], p[2]);
                            if (a == Air || decor(a)) continue;
                            int q[3] = { p[0], p[1], p[2] };
                            q[d] += sgn;
                            // Vicino dentro il chunk: accesso diretto, niente
                            // lookup nella mappa dei chunk (6 per voxel).
                            uint8_t b = (q[d] >= 0 && q[d] < kChunk)
                                ? ch.at(q[0], q[1], q[2])
                                : get(q[0] + ox, q[1] + oy, q[2] + oz);
                            if (faceVisible(a, b)) mask[i + kChunk * j] = a;
                        }
                    for (int j = 0; j < kChunk; ++j)
                        for (int i = 0; i < kChunk;) {
                            uint8_t id = mask[i + kChunk * j];
                            if (!id) { ++i; continue; }
                            int w = 1;
                            while (i + w < kChunk && mask[i + w + kChunk * j] == id) ++w;
                            int h = 1;
                            bool ok = true;
                            while (j + h < kChunk && ok) {
                                for (int t = 0; t < w; ++t)
                                    if (mask[i + t + kChunk * (j + h)] != id) { ok = false; break; }
                                if (ok) ++h;
                            }
                            for (int hh = 0; hh < h; ++hh)
                                for (int t = 0; t < w; ++t)
                                    mask[i + t + kChunk * (j + hh)] = 0;

                            float base[3];
                            base[d] = (float)(k + (sgn > 0 ? 1 : 0));
                            base[u] = (float)i;
                            base[v] = (float)j;
                            // posizioni mondo = (cella + offset chunk) * s
                            const float off[3] = { (float)ox, (float)oy, (float)oz };
                            glm::vec3 P0{0}, du{0}, dv{0}, n{0};
                            P0[0] = (base[0] + off[0]) * s;
                            P0[1] = (base[1] + off[1]) * s;
                            P0[2] = (base[2] + off[2]) * s;
                            du[u] = (float)w * s;
                            dv[v] = (float)h * s;
                            n[d] = (float)sgn;
                            std::vector<float>& out = perId[id];
                            if (sgn > 0) pushQuad(out, P0, P0 + du, P0 + du + dv, P0 + dv, n);
                            else         pushQuad(out, P0, P0 + dv, P0 + du + dv, P0 + du, n);
                            i += w;
                        }
                }
            }
        }

        // Decorazioni: scatoline singole (fiori, torce, germogli, scale...)
        for (int y = 0; y < kChunk; ++y)
            for (int z = 0; z < kChunk; ++z)
                for (int x = 0; x < kChunk; ++x) {
                    uint8_t id = ch.at(x, y, z);
                    if (!decor(id)) continue;
                    glm::vec3 cc = cellToWorld(ox + x, oy + y, oz + z);
                    glm::vec3 half{ s * 0.25f, s * 0.35f, s * 0.25f };
                    if (id == Torch) half = { s * 0.09f, s * 0.32f, s * 0.09f };
                    if (id == Ladder || id == DoorOpen) half = { s * 0.5f, s * 0.5f, s * 0.08f };
                    cc.y -= s * 0.5f - half.y;   // appoggiata sul pavimento della cella
                    pushBox(perId[id], cc, half);
                }

        size_t total = 0;
        for (int id = 0; id < Count; ++id) total += perId[id].size();
        m.verts.reserve(total);
        for (int id = 0; id < Count; ++id) {
            if (perId[id].empty()) continue;
            MeshRange r;
            r.id = (uint8_t)id;
            r.first = (int)(m.verts.size() / 6);
            r.count = (int)(perId[id].size() / 6);
            m.verts.insert(m.verts.end(), perId[id].begin(), perId[id].end());
            m.ranges.push_back(r);
        }
        ++m.version;
    }

    static uint32_t hash3(int x, int y, int z) {
        uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)z * 144066263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }

    static float fade(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }
    static float grad(int hash, float x, float y, float z) {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }
    static int perm(int i) {
        static const int p[256] = {
            151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
            8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
            35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
            134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
            55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
            18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
            250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
            189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
            172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
            228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
            107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
            138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
        };
        return p[i & 255];
    }
    static float perlin3(float x, float y, float z) {
        int X = (int)std::floor(x) & 255, Y = (int)std::floor(y) & 255, Z = (int)std::floor(z) & 255;
        x -= std::floor(x); y -= std::floor(y); z -= std::floor(z);
        float u = fade(x), v = fade(y), w = fade(z);
        int A = perm(X) + Y, AA = perm(A) + Z, AB = perm(A + 1) + Z;
        int B = perm(X + 1) + Y, BA = perm(B) + Z, BB = perm(B + 1) + Z;
        return lerp(lerp(lerp(grad(perm(AA), x, y, z), grad(perm(BA), x - 1, y, z), u),
                         lerp(grad(perm(AB), x, y - 1, z), grad(perm(BB), x - 1, y - 1, z), u), v),
                    lerp(lerp(grad(perm(AA + 1), x, y, z - 1), grad(perm(BA + 1), x - 1, y, z - 1), u),
                         lerp(grad(perm(AB + 1), x, y - 1, z - 1), grad(perm(BB + 1), x - 1, y - 1, z - 1), u), v), w);
    }
    static float fbm(float x, float z) {
        float a = 0, f = 1, amp = 1, n = 0;
        for (int i = 0; i < 5; ++i) {
            a += perlin3(x * f, 0.1f * i, z * f) * amp;
            n += amp;
            f *= 2.f;
            amp *= 0.5f;
        }
        return n > 1e-5f ? a / n : 0.f;
    }

    std::unordered_map<Coord, Chunk, CoordHash> chunks_;
    std::vector<Quad> quads_;
    std::vector<Fuse> primed_;
    float blockSize_ = kDefaultBlock;
    bool dirty_ = false;
    bool quadsStale_ = false;
    int blockCount_ = 0;
    int seed_ = 0;
    float tickAcc_ = 0.f;
};
