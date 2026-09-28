#pragma once


#include <cstddef>
#include <string>

namespace obf {

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

    std::string get() const {
        std::string out;
        out.reserve(N > 0 ? N - 1 : 0);
        for (std::size_t i = 0; i + 1 < N; ++i)
            out.push_back((char)(data[i] ^ key_at(i)));
        return out;
    }
};

}

#define OBF(str)                                                        \
    ([]() -> std::string {                                              \
        static constexpr auto _obf = obf::ObfStr<sizeof(str)>(str);     \
        return _obf.get();                                             \
    }())
