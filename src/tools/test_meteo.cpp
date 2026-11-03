#include "systems/Meteo.h"
#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("=== Meteo ===\n");
    Meteo m;
    m.reset();
    m.setHours(22.f);
    check(m.isNight(), "22 e' notte");
    m.setHours(12.f);
    check(!m.isNight(), "12 e' giorno");
    m.setSpeed(2.f);
    m.update(0.5f);
    check(m.hours() > 12.9f && m.hours() < 13.2f, "il ciclo avanza");
    m.setWeather(Meteo::parseWeather("pioggia"));
    check(m.weather() == Meteo::Weather::Pioggia, "pioggia");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
