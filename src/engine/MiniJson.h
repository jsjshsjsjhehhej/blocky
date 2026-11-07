#pragma once

/**
 * Parser JSON piccolo e severo, per le risposte del backend.
 *
 * Il resto del motore legge JSON con jsonFindStr & co. (cerca una chiave nel
 * testo): va bene per i file nostri, non per una risposta di rete, dove una
 * chiave dentro una stringa o un oggetto annidato inganna la ricerca. Qui il
 * testo si legge davvero, e qualsiasi cosa malformata fa fallire tutto
 * invece di dare mezzo risultato.
 *
 * Limiti voluti: profondita' massima 32, testo fino a 1 MB.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace blk {
namespace json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    double num = 0.0;
    bool isInteger = false; // scritto senza '.', 'e': per i FInt
    long long integer = 0;
    std::string str;
    std::vector<Value> arr;
    std::map<std::string, Value> obj;

    bool isObject() const { return type == Type::Object; }
    const Value* get(const std::string& k) const {
        if (type != Type::Object) return nullptr;
        const auto it = obj.find(k);
        return it == obj.end() ? nullptr : &it->second;
    }
};

namespace detail {

struct Parser {
    const std::string& s;
    size_t i = 0;
    int depth = 0;

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }

    bool lit(const char* w) {
        size_t n = 0;
        while (w[n]) ++n;
        if (s.compare(i, n, w) != 0) return false;
        i += n;
        return true;
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(uint32_t& out) {
        if (i + 4 > s.size()) return false;
        out = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s[i++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= (uint32_t)(c - '0');
            else if (c >= 'a' && c <= 'f') out |= (uint32_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= (uint32_t)(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    bool string(std::string& out) {
        if (i >= s.size() || s[i] != '"') return false;
        ++i;
        while (i < s.size()) {
            const char c = s[i++];
            if (c == '"') return true;
            if ((unsigned char)c < 0x20) return false; // a capo crudo: non e' JSON
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) return false;
            const char e = s[i++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                uint32_t cp = 0;
                if (!hex4(cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDBFF) { // coppia surrogata
                    uint32_t lo = 0;
                    if (!lit("\\u") || !hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return false;
                }
                appendUtf8(out, cp);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    bool number(Value& v) {
        const size_t start = i;
        if (i < s.size() && s[i] == '-') ++i;
        if (i >= s.size() || s[i] < '0' || s[i] > '9') return false;
        if (s[i] == '0') ++i;
        else while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        bool frac = false;
        if (i < s.size() && s[i] == '.') {
            frac = true;
            ++i;
            if (i >= s.size() || s[i] < '0' || s[i] > '9') return false;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            frac = true;
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            if (i >= s.size() || s[i] < '0' || s[i] > '9') return false;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }
        const std::string text = s.substr(start, i - start);
        v.type = Value::Type::Number;
        v.num = std::strtod(text.c_str(), nullptr);
        if (!frac && text.size() <= 18) {
            v.isInteger = true;
            v.integer = std::strtoll(text.c_str(), nullptr, 10);
        }
        return true;
    }

    bool value(Value& v) {
        if (++depth > 32) return false;
        ws();
        if (i >= s.size()) return false;
        bool ok = false;
        const char c = s[i];
        if (c == '{') {
            ++i;
            v.type = Value::Type::Object;
            ws();
            if (i < s.size() && s[i] == '}') { ++i; ok = true; }
            else {
                for (;;) {
                    ws();
                    std::string key;
                    if (!string(key)) break;
                    ws();
                    if (i >= s.size() || s[i] != ':') break;
                    ++i;
                    Value child;
                    if (!value(child)) break;
                    v.obj[key] = std::move(child);
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == '}') { ++i; ok = true; }
                    break;
                }
            }
        } else if (c == '[') {
            ++i;
            v.type = Value::Type::Array;
            ws();
            if (i < s.size() && s[i] == ']') { ++i; ok = true; }
            else {
                for (;;) {
                    Value child;
                    if (!value(child)) break;
                    v.arr.push_back(std::move(child));
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == ']') { ++i; ok = true; }
                    break;
                }
            }
        } else if (c == '"') {
            v.type = Value::Type::String;
            ok = string(v.str);
        } else if (c == 't') {
            ok = lit("true");
            v.type = Value::Type::Bool;
            v.b = true;
        } else if (c == 'f') {
            ok = lit("false");
            v.type = Value::Type::Bool;
        } else if (c == 'n') {
            ok = lit("null");
        } else {
            ok = number(v);
        }
        --depth;
        return ok;
    }
};

} // namespace detail

/** Falso se il testo non e' JSON valido (anche con roba dopo la fine). */
inline bool parse(const std::string& text, Value& out) {
    out = Value{};
    if (text.size() > 1024 * 1024) return false;
    detail::Parser p{text};
    if (!p.value(out)) return false;
    p.ws();
    return p.i == text.size();
}

/** Stringa JSON con le virgolette, per scrivere la cache. */
inline std::string quote(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if ((unsigned char)c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(unsigned char)c);
                out += buf;
            } else {
                out += c;
            }
        }
    }
    return out + "\"";
}

} // namespace json
} // namespace blk
