#pragma once

// Crafting: ricette 3 ingredienti + set alpha (tavole, picconi, forno, TNT).

#include "systems/Inventario.h"

#include <string>
#include <vector>

class Crafting {
public:
    struct Ing {
        std::string id;
        int n = 1;
    };
    struct Recipe {
        std::string id;
        std::string outId;
        std::string outName;
        int outCount = 1;
        Ing ins[3];
        int inCount = 0;
    };

    static constexpr int kMaxRecipes = 80;

    void reset() {
        recipes_.clear();
        seedAlpha();
    }

    void attach(Inventario* inv) { inv_ = inv; }
    void update(float) {}

    bool addRecipe(const std::string& id, const std::string& outId, const std::string& outName,
                   int outCount) {
        if (id.empty() || outId.empty() || (int)recipes_.size() >= kMaxRecipes)
            return false;
        if (find(id)) return false;
        Recipe r;
        r.id = id;
        r.outId = outId;
        r.outName = outName.empty() ? outId : outName;
        r.outCount = outCount > 0 ? outCount : 1;
        recipes_.push_back(r);
        return true;
    }

    bool addIngredient(const std::string& recipeId, const std::string& itemId, int n) {
        Recipe* r = find(recipeId);
        if (!r || itemId.empty() || n <= 0 || r->inCount >= 3) return false;
        r->ins[r->inCount].id = itemId;
        r->ins[r->inCount].n = n;
        r->inCount += 1;
        return true;
    }

    bool canCraft(const std::string& id) const {
        const Recipe* r = get(id);
        if (!r || !inv_ || r->inCount <= 0) return false;
        for (int i = 0; i < r->inCount; ++i)
            if (!inv_->has(r->ins[i].id, r->ins[i].n)) return false;
        return true;
    }

    bool tryCraft(const std::string& id) {
        if (!canCraft(id)) return false;
        const Recipe* r = get(id);
        for (int i = 0; i < r->inCount; ++i)
            inv_->remove(r->ins[i].id, r->ins[i].n);
        inv_->add(r->outId, r->outName, r->outCount);
        return true;
    }

    const Recipe* get(const std::string& id) const {
        for (const Recipe& r : recipes_)
            if (r.id == id) return &r;
        return nullptr;
    }

    const std::vector<Recipe>& all() const { return recipes_; }
    int count() const { return (int)recipes_.size(); }

    void seedAlpha() {
        rec("planks", "planks", "Planks", 4, "wood", 1, "", 0, "", 0);
        rec("sticks", "stick", "Stick", 4, "planks", 2, "", 0, "", 0);
        rec("table", "table", "CraftingTable", 1, "planks", 4, "", 0, "", 0);
        rec("chest", "chest", "Chest", 1, "planks", 8, "", 0, "", 0);
        rec("furnace", "furnace", "Furnace", 1, "cobble", 8, "", 0, "", 0);
        rec("torch", "torch", "Torch", 4, "coal", 1, "stick", 1, "", 0);
        rec("woodpick", "woodpick", "WoodPickaxe", 1, "planks", 3, "stick", 2, "", 0);
        rec("stonepick", "stonepick", "StonePickaxe", 1, "cobble", 3, "stick", 2, "", 0);
        rec("ironpick", "ironpick", "IronPickaxe", 1, "ironbar", 3, "stick", 2, "", 0);
        rec("diamondpick", "diamondpick", "DiamondPickaxe", 1, "diamond", 3, "stick", 2, "", 0);
        rec("shovel", "shovel", "Shovel", 1, "planks", 1, "stick", 2, "", 0);
        rec("axe", "axe", "Axe", 1, "planks", 3, "stick", 2, "", 0);
        rec("sword", "sword", "Sword", 1, "planks", 2, "stick", 1, "", 0);
        rec("door", "door", "Door", 1, "planks", 6, "", 0, "", 0);
        rec("ladder", "ladder", "Ladder", 3, "stick", 7, "", 0, "", 0);
        rec("bed", "bed", "Bed", 1, "planks", 3, "wool", 3, "", 0);
        rec("tnt", "tnt", "Tnt", 1, "sand", 4, "coal", 5, "", 0);
        rec("bread", "bread", "Bread", 1, "wheat", 3, "", 0, "", 0);
        rec("stew", "stew", "MushroomStew", 1, "mushroom", 2, "bowl", 1, "", 0);
        rec("bowl", "bowl", "Bowl", 4, "planks", 3, "", 0, "", 0);
        rec("ironblock", "ironblock", "IronBlock", 1, "ironbar", 9, "", 0, "", 0);
        rec("goldblock", "goldblock", "GoldBlock", 1, "goldbar", 9, "", 0, "", 0);
        rec("diamondblock", "diamondblock", "DiamondBlock", 1, "diamond", 9, "", 0, "", 0);
        rec("bucket", "bucket", "Bucket", 1, "ironbar", 3, "", 0, "", 0);
        rec("shears", "shears", "Shears", 1, "ironbar", 2, "", 0, "", 0);
        rec("cookie", "cookie", "Cookie", 8, "wheat", 2, "", 0, "", 0);
    }

private:
    void rec(const char* id, const char* out, const char* name, int n,
             const char* a, int na, const char* b, int nb, const char* c, int nc) {
        if (!addRecipe(id, out, name, n)) return;
        if (na > 0) addIngredient(id, a, na);
        if (nb > 0) addIngredient(id, b, nb);
        if (nc > 0) addIngredient(id, c, nc);
    }

    Recipe* find(const std::string& id) {
        for (Recipe& r : recipes_)
            if (r.id == id) return &r;
        return nullptr;
    }

    std::vector<Recipe> recipes_;
    Inventario* inv_ = nullptr;
};
