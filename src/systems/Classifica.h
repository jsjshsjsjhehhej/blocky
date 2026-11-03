#pragma once

// Classifica: punteggi dei giocatori (leaderstats) in alto a destra.
// Generato da tools/genera.py il 2026-09-30.
//
// Le colonne (Monete, Uccisioni...) nascono la prima volta che uno script le
// usa, nell'ordine in cui le usa. In rete ogni giocatore manda solo la sua
// riga; l'host la firma con il nome vero e la rilancia (vedi LanNet.h,
// kStatsEventName). La logica qui non sa niente di rete: riceve e produce
// testo, cosi' si prova da sola.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

class Classifica {
public:
    static constexpr int kMaxStats = 6;
    static constexpr size_t kMaxStatName = 12;

    struct Row {
        std::string player;
        std::vector<double> values;   // stesso ordine di stats()
        bool local = false;
    };

    void reset() {
        stats_.clear();
        rows_.clear();
        dirty_ = false;
        sendClock_ = 0.f;
        refreshClock_ = 0.f;
    }

    void update(float dt) {
        sendClock_ -= dt;
        refreshClock_ -= dt;
    }

    void setLocalPlayer(const std::string& name) {
        if (name == localName_) return;
        auto it = rows_.find(localName_);
        if (it != rows_.end()) {
            rows_[name] = it->second;
            rows_.erase(it);
        }
        localName_ = name;
    }
    const std::string& localPlayer() const { return localName_; }

    // Nome colonna valido: lettere, numeri, spazio, _ (niente separatori di rete).
    static bool validStatName(const std::string& s) {
        if (s.empty() || s.size() > kMaxStatName) return false;
        for (char c : s)
            if (!(std::isalnum((unsigned char)c) || c == ' ' || c == '_')) return false;
        return true;
    }

    // Si scrive SOLO la propria riga: ognuno e' l'unico padrone della sua,
    // altrimenti due giocatori che la scrivono si sovrascriverebbero a vicenda.
    // false = nome non valido o troppe colonne.
    bool set(const std::string& stat, double value) {
        const int col = column(stat, true);
        if (col < 0 || !std::isfinite(value)) return false;
        auto& v = rows_[localName_];
        if (v.size() < stats_.size()) v.resize(stats_.size(), 0.0);
        if (v[(size_t)col] == value) return true;
        v[(size_t)col] = value;
        dirty_ = true;
        return true;
    }

    bool add(const std::string& stat, double delta, double& out) {
        out = get(stat) + delta;
        return set(stat, out);
    }

    double get(const std::string& stat, const std::string& player = {}) const {
        const int col = column(stat);
        if (col < 0) return 0.0;
        auto it = rows_.find(player.empty() ? localName_ : player);
        if (it == rows_.end() || (size_t)col >= it->second.size()) return 0.0;
        return it->second[(size_t)col];
    }

    bool removeStat(const std::string& stat) {
        const int col = column(stat);
        if (col < 0) return false;
        stats_.erase(stats_.begin() + col);
        for (auto& r : rows_)
            if ((size_t)col < r.second.size()) r.second.erase(r.second.begin() + col);
        dirty_ = true;
        return true;
    }

    const std::vector<std::string>& stats() const { return stats_; }

    // ---- Rete (testo, la spedizione la fa chi chiama) --------------------

    // "Monete=10;Uccisioni=2"
    std::string encodeRow(const std::string& player) const {
        std::string out;
        auto it = rows_.find(player);
        for (size_t i = 0; i < stats_.size(); ++i) {
            double v = (it != rows_.end() && i < it->second.size()) ? it->second[i] : 0.0;
            char num[32];
            std::snprintf(num, sizeof(num), "%.10g", v);
            if (!out.empty()) out += ';';
            out += stats_[i] + "=" + num;
        }
        return out;
    }

    // Riga arrivata dalla rete per quel giocatore. Colonne sconosciute nascono.
    // La propria riga dalla rete si ignora: quella vera e' quella locale.
    void applyRow(const std::string& player, const std::string& payload) {
        if (player.empty() || player == localName_) return;
        size_t start = 0;
        while (start < payload.size()) {
            size_t end = payload.find(';', start);
            if (end == std::string::npos) end = payload.size();
            const std::string item = payload.substr(start, end - start);
            const size_t eq = item.find('=');
            if (eq != std::string::npos) {
                const std::string stat = item.substr(0, eq);
                char* stop = nullptr;
                const double v = std::strtod(item.c_str() + eq + 1, &stop);
                const int col = column(stat, true);
                if (col >= 0 && std::isfinite(v)) {
                    auto& row = rows_[player];
                    if (row.size() < stats_.size()) row.resize(stats_.size(), 0.0);
                    row[(size_t)col] = v;
                }
            }
            start = end + 1;
        }
    }

    // Da mandare adesso? Subito dopo un cambio (max 2 volte al secondo) e
    // comunque ogni 5 s, cosi' chi entra dopo vede la riga anche se non cambia.
    bool takeLocalRowToSend(std::string& payload) {
        if (stats_.empty()) return false;
        const bool due = (dirty_ && sendClock_ <= 0.f) || refreshClock_ <= 0.f;
        if (!due) return false;
        dirty_ = false;
        sendClock_ = 0.5f;
        refreshClock_ = 5.f;
        payload = encodeRow(localName_);
        return true;
    }


    // Chi e' in partita adesso: le righe di chi e' uscito spariscono.
    void setPresentPlayers(const std::vector<std::string>& names) {
        for (auto it = rows_.begin(); it != rows_.end();) {
            const bool here = it->first == localName_
                || std::find(names.begin(), names.end(), it->first) != names.end();
            it = here ? std::next(it) : rows_.erase(it);
        }
    }

    // Righe da mostrare, ordinate per la prima colonna (dal piu' alto).
    std::vector<Row> sortedRows() const {
        std::vector<Row> out;
        for (const auto& r : rows_) {
            Row row{r.first, r.second, r.first == localName_};
            row.values.resize(stats_.size(), 0.0);
            out.push_back(std::move(row));
        }
        if (rows_.find(localName_) == rows_.end() && !stats_.empty())
            out.push_back({localName_, std::vector<double>(stats_.size(), 0.0), true});
        std::stable_sort(out.begin(), out.end(), [](const Row& a, const Row& b) {
            const double va = a.values.empty() ? 0.0 : a.values[0];
            const double vb = b.values.empty() ? 0.0 : b.values[0];
            return va > vb;
        });
        return out;
    }

private:
    int column(const std::string& stat) const {
        for (size_t i = 0; i < stats_.size(); ++i)
            if (stats_[i] == stat) return (int)i;
        return -1;
    }

    int column(const std::string& stat, bool create) {
        const int found = column(stat);
        if (found >= 0 || !create) return found;
        if (!validStatName(stat) || (int)stats_.size() >= kMaxStats) return -1;
        stats_.push_back(stat);
        return (int)stats_.size() - 1;
    }

    std::vector<std::string> stats_;
    std::map<std::string, std::vector<double>> rows_;
    std::string localName_ = "Player";
    bool dirty_ = false;
    float sendClock_ = 0.f;
    float refreshClock_ = 0.f;
};
