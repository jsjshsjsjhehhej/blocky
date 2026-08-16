#pragma once

/**
 * Stringhe offuscate a compile-time (XOR).
 *
 * A cosa serve: le liste di RILEVAMENTO ("cheatengine", "x64dbg", ...) oggi
 * stanno in chiaro nel binario. Un autore di cheat apre l'exe, cerca il nome
 * del suo strumento, lo trova in lista e lo rinomina per non farsi vedere.
 * Con questo, nel binario ci sono byte XOR-ati: il nome in chiaro non compare
 * finche' il gioco non lo ricostruisce in memoria al momento del confronto.
 *
 * A cosa NON serve: le stringhe mostrate all'utente (menu, messaggi del gate)
 * DEVONO restare leggibili. Non offuscarle. Questo file e' solo per i termini
 * di rilevamento interni.
 *
 * Limite onesto: e' un XOR, si rompe a runtime con un debugger. Serve ad
 * alzare il costo del reverse casuale, non a fermare chi sa il fatto suo —
 * come tutto cio' che gira sul client. La difesa vera resta il server.
 *
 * Uso:
 *     static constexpr auto s = OBF("cheatengine");
 *     std::string in_chiaro = s.get();   // ricostruita solo qui, in RAM
 */

#include <cstddef>
#include <string>

namespace obf {

// Chiave dipendente dalla posizione: due caratteri uguali danno byte diversi,
// cosi' non si riconosce il pattern nel binario.
constexpr unsigned char key_at(std::size_t i) {
    return (unsigned char)(0x5Au ^ (unsigned char)(i * 7u + 0x1Du));
}

template <std::size_t N>
struct ObfStr {
    unsigned char data[N];

    constexpr ObfStr(const char (&s)[N]) : data{} {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = (unsigned char)s[i] ^ key_at(i);
    }

    // Ricostruisce la stringa in chiaro in memoria. Chiamata solo al confronto.
    std::string get() const {
        std::string out;
        out.reserve(N > 0 ? N - 1 : 0);
        for (std::size_t i = 0; i + 1 < N; ++i)
            out.push_back((char)(data[i] ^ key_at(i)));
        return out;
    }
};

} // namespace obf

// `static constexpr` dentro una lambda invocata subito: OBBLIGA il calcolo a
// compile-time. Senza il `static constexpr` il compilatore costruisce a
// runtime dal letterale e il testo in chiaro resta in .rodata (verificato:
// senza questo, "cheatengine" compariva ancora nel binario).
#define OBF(str)                                                        \
    ([]() -> std::string {                                              \
        static constexpr auto _obf = obf::ObfStr<sizeof(str)>(str);     \
        return _obf.get();                                             \
    }())
