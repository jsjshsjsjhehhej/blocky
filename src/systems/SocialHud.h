#pragma once

#include "systems/Social.h"
#include "engine/StudioProApis.h"

#include <imgui.h>

inline void socialFireReceipt(lua_State* L) {
    auto& s = social();
    if (!s.receiptPending || !L) return;
    const int product = s.receiptProduct;
    s.receiptPending = false;
    auto it = studio_pro_apis::processReceiptHooks().find("default");
    if (it == studio_pro_apis::processReceiptHooks().end() || it->second < 0) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    lua_newtable(L);
    lua_pushinteger(L, s.userId); lua_setfield(L, -2, "PlayerId");
    lua_pushinteger(L, product); lua_setfield(L, -2, "ProductId");
    lua_pushinteger(L, 0); lua_setfield(L, -2, "PlaceIdWherePurchased");
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

inline void drawSocialPurchase(lua_State* L) {
    socialFireReceipt(L);
    auto& s = social();
    if (!s.prompt) return;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({ds.x * 0.5f, ds.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({320.f, 0.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.12f, 0.2f, 0.96f});
    if (ImGui::Begin("Marketplace##buy", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(s.promptPass ? "Game Pass" : "Prodotto");
        ImGui::Separator();
        ImGui::Text("%s", s.promptName.c_str());
        ImGui::Text("%d B$", s.promptPrice);
        ImGui::TextDisabled("Hai %d B$  ·  %s", ecoBalance(s.user.c_str()), s.user.c_str());
        if (!s.promptError.empty())
            ImGui::TextColored({1.f, 0.45f, 0.4f, 1.f}, "%s", s.promptError.c_str());
        if (ImGui::Button("Compra", {140.f, 32.f})) socialBuy();
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {140.f, 32.f})) s.prompt = false;
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
