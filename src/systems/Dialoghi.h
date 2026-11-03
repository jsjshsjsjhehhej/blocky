#pragma once

// Dialoghi: albero di battute NPC. Script: AddNode / Link / AddChoice / Start / Next / Choose.

#include <string>
#include <vector>

class Dialoghi {
public:
    struct Choice {
        std::string text;
        std::string nextId;
    };
    struct Node {
        std::string id;
        std::string speaker;
        std::string text;
        std::string nextId;
        std::vector<Choice> choices;
    };

    static constexpr int kMaxNodes = 64;
    static constexpr int kMaxChoices = 4;

    void reset() {
        nodes_.clear();
        current_.clear();
        title_.clear();
        open_ = false;
    }
    void update(float) {}

    bool addNode(const std::string& id, const std::string& speaker, const std::string& text) {
        if (id.empty() || id.size() > 32 || (int)nodes_.size() >= kMaxNodes) return false;
        if (find(id)) return false;
        Node n;
        n.id = id;
        n.speaker = speaker.empty() ? "NPC" : speaker.substr(0, 24);
        n.text = text.substr(0, 160);
        nodes_.push_back(n);
        return true;
    }

    bool link(const std::string& from, const std::string& to) {
        Node* n = find(from);
        if (!n || !find(to)) return false;
        n->nextId = to;
        return true;
    }

    bool addChoice(const std::string& nodeId, const std::string& text, const std::string& to) {
        Node* n = find(nodeId);
        if (!n || text.empty() || !find(to) || (int)n->choices.size() >= kMaxChoices)
            return false;
        Choice c;
        c.text = text.substr(0, 48);
        c.nextId = to;
        n->choices.push_back(c);
        return true;
    }

    bool start(const std::string& id, const std::string& title) {
        if (!find(id)) return false;
        current_ = id;
        title_ = title.empty() ? "Dialogo" : title.substr(0, 32);
        open_ = true;
        return true;
    }

    bool next() {
        Node* n = find(current_);
        if (!n || !open_) return false;
        if (!n->choices.empty()) return false;
        if (n->nextId.empty()) {
            close();
            return true;
        }
        if (!find(n->nextId)) return false;
        current_ = n->nextId;
        return true;
    }

    bool choose(int index) {
        Node* n = find(current_);
        if (!n || index < 0 || index >= (int)n->choices.size()) return false;
        const std::string to = n->choices[(size_t)index].nextId;
        if (!find(to)) {
            close();
            return true;
        }
        current_ = to;
        return true;
    }

    void close() {
        open_ = false;
        current_.clear();
    }

    bool open() const { return open_; }
    const std::string& title() const { return title_; }
    const Node* current() const { return findConst(current_); }
    int nodeCount() const { return (int)nodes_.size(); }

private:
    Node* find(const std::string& id) {
        for (Node& n : nodes_)
            if (n.id == id) return &n;
        return nullptr;
    }
    const Node* findConst(const std::string& id) const {
        for (const Node& n : nodes_)
            if (n.id == id) return &n;
        return nullptr;
    }

    std::vector<Node> nodes_;
    std::string current_;
    std::string title_;
    bool open_ = false;
};
