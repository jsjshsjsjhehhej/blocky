#!/usr/bin/env python3
"""Generatore di sistemi per Blocky.

    py tools/genera.py sistema Classifica
    py tools/genera.py sistema Classifica --descrizione "Punteggi in alto a destra"
    py tools/genera.py sistema Classifica --prova      (mostra cosa farebbe, non scrive)

Crea lo scheletro di un sistema nuovo, gia' collegato al motore:
  src/systems/<Nome>.h       logica (niente ImGui ne' Lua: si testa da sola)
  src/systems/<Nome>Lua.h    tabella Lua globale <Nome> con le sue funzioni
  src/systems/<Nome>Hud.h    disegno a schermo durante il gioco
  src/tools/test_<nome>.cpp  test, gia' aggiunto a CMake

e scrive le righe di collegamento sotto i segnaposto "@genera:" di main.cpp e
CMakeLists.txt (include, istanza, API Lua, reset all'entrata in Play,
aggiornamento a ogni frame, HUD, test).

I file generati sono codice normale da salvare nel progetto: la build non ha
mai bisogno di Python, solo chi genera. Non sovrascrive mai un file che esiste
gia' e non duplica righe gia' presenti, quindi rilanciarlo e' innocuo.
"""

import argparse
import datetime
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

MAIN_CPP = ROOT / "src" / "main.cpp"
CMAKE = ROOT / "CMakeLists.txt"


def lower_first(name: str) -> str:
    return name[0].lower() + name[1:]


def snake(name: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def templates(name: str, desc: str) -> dict:
    var = "g_" + lower_first(name)
    ns = name.lower() + "lua"
    today = datetime.date.today().isoformat()
    desc_line = desc or "(descrivi qui cosa fa il sistema)"
    files = {}

    files[f"src/systems/{name}.h"] = f"""#pragma once

// {name}: {desc_line}
// Generato da tools/genera.py il {today}. Da qui in poi e' codice normale.
//
// Solo logica: niente ImGui, niente Lua, niente globali del motore. Quello che
// serve arriva come parametro, cosi' il sistema si prova da solo in
// src/tools/test_{snake(name)}.cpp.

class {name} {{
public:
    // Entrata in Play o in Test (prima che partano gli script).
    void reset() {{
        time_ = 0.f;
    }}

    // Ogni frame mentre si gioca.
    void update(float dt) {{
        time_ += dt;
    }}

    float time() const {{ return time_; }}

private:
    float time_ = 0.f;
}};
"""

    files[f"src/systems/{name}Lua.h"] = f"""#pragma once

// API Lua di {name}: la tabella globale "{name}" negli script.
// Ogni funzione riceve il sistema come upvalue 1 (vedi self()).

#include "systems/{name}.h"

#include "lua.h"
#include "lualib.h"

namespace {ns} {{

inline {name}* self(lua_State* L) {{
    return static_cast<{name}*>(lua_touserdata(L, lua_upvalueindex(1)));
}}

// {name}.Time() -> secondi da quando e' iniziata la partita
inline int time(lua_State* L) {{
    lua_pushnumber(L, self(L)->time());
    return 1;
}}

inline void add(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {{
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}}

inline void registerLua(lua_State* L, void* ctx) {{
    lua_newtable(L);
    add(L, ctx, time, "Time", "{name}.Time");
    lua_setglobal(L, "{name}");
}}

}} // namespace {ns}
"""

    files[f"src/systems/{name}Hud.h"] = f"""#pragma once

// Cosa mostra {name} a schermo durante il gioco (Play e Test in Studio).

#include "systems/{name}.h"

#include <imgui.h>

inline void draw{name}Hud(const {name}& s) {{
    (void)s;
}}
"""

    files[f"src/tools/test_{snake(name)}.cpp"] = f"""// Test di {name} (src/systems/{name}.h), senza finestra ne' Lua.

#include "systems/{name}.h"

#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* what) {{
    std::printf("  [%s] %s\\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}}

int main() {{
    std::printf("=== {name} ===\\n");
    {name} s;
    s.reset();
    s.update(0.5f);
    s.update(0.25f);
    check(s.time() > 0.74f && s.time() < 0.76f, "il tempo avanza con update");
    s.reset();
    check(s.time() == 0.f, "reset riparte da zero");

    std::printf("\\n%s (%d falliti)\\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}}
"""

    hooks = {
        MAIN_CPP: {
            "@genera:include": [
                f'#include "systems/{name}.h"',
                f'#include "systems/{name}Lua.h"',
                f'#include "systems/{name}Hud.h"',
            ],
            "@genera:globals": [f"static {name} {var};"],
            "@genera:lua": [f'g_lua.extensions.push_back({{"{name}", &{ns}::registerLua, &{var}}});'],
            "@genera:reset": [f"{var}.reset();"],
            "@genera:update": [f"{var}.update(dt);"],
            "@genera:hud": [f"draw{name}Hud({var});"],
        },
        CMAKE: {
            "@genera:tests": [
                f"add_executable(test_{snake(name)} src/tools/test_{snake(name)}.cpp)",
                f"target_include_directories(test_{snake(name)} PRIVATE ${{CMAKE_SOURCE_DIR}}/src)",
                f"target_link_libraries(test_{snake(name)} PRIVATE glm::glm)",
            ],
        },
    }
    return files, hooks


def insert_under_markers(path: pathlib.Path, hooks: dict, dry: bool) -> list:
    """Scrive le righe subito sotto al segnaposto, con la sua indentazione.
    Salta quelle gia' presenti nel file. Ritorna la lista delle modifiche."""
    raw = path.read_bytes().decode("utf-8")
    newline = "\r\n" if "\r\n" in raw else "\n"
    lines = raw.split(newline)
    changes = []
    for marker, new_lines in hooks.items():
        idx = next((i for i, l in enumerate(lines) if l.strip().endswith(marker)), None)
        if idx is None:
            raise SystemExit(f"Segnaposto '{marker}' non trovato in {path.name}: "
                             "e' stato cancellato o spostato?")
        indent = lines[idx][: len(lines[idx]) - len(lines[idx].lstrip())]
        existing = {l.strip() for l in lines}
        to_add = [indent + l for l in new_lines if l.strip() not in existing]
        if not to_add:
            continue
        # Dopo le righe gia' generate da altri sistemi, cosi' restano in ordine.
        at = idx + 1
        while at < len(lines) and lines[at].strip() and "@genera:" not in lines[at] \
                and lines[at].startswith(indent) and not lines[at].strip().startswith(("}", "static void")):
            at += 1
        lines[at:at] = to_add
        changes += [f"{path.relative_to(ROOT)}  {marker}:  {l.strip()}" for l in to_add]
    if changes and not dry:
        path.write_bytes(newline.join(lines).encode("utf-8"))
    return changes


def cmd_sistema(args) -> int:
    name = args.nome
    if not re.fullmatch(r"[A-Z][A-Za-z0-9]{1,40}", name):
        print("Il nome deve essere in CamelCase e iniziare con la maiuscola, es. Classifica, AudioMixer.")
        return 2

    files, hooks = templates(name, args.descrizione)

    existing = [p for p in files if (ROOT / p).exists()]
    if existing:
        print("Esistono gia', non li sovrascrivo:")
        for p in existing:
            print("   ", p)
        print("Se vuoi rigenerare, cancellali prima (o scegli un altro nome).")
        return 1

    for rel, text in files.items():
        print(("[prova] creerei  " if args.prova else "creato  ") + rel)
        if not args.prova:
            out = ROOT / rel
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(text, encoding="utf-8", newline="\n")

    for path, h in hooks.items():
        for c in insert_under_markers(path, h, args.prova):
            print(("[prova] aggiungerei  " if args.prova else "collegato  ") + c)

    if not args.prova:
        print(f"\nFatto. Prossimi passi:\n"
              f"  1. scrivi la logica in src/systems/{name}.h\n"
              f"  2. aggiungi le funzioni Lua in src/systems/{name}Lua.h\n"
              f"  3. cmake --build build --target test_{snake(name)} blocky")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(prog="genera.py", description="Generatore di sistemi per Blocky")
    sub = p.add_subparsers(dest="cosa", required=True)
    s = sub.add_parser("sistema", help="crea un sistema nuovo gia' collegato al motore")
    s.add_argument("nome", help="nome in CamelCase, es. Classifica")
    s.add_argument("--descrizione", default="", help="una riga su cosa fa")
    s.add_argument("--prova", action="store_true", help="mostra cosa farebbe senza scrivere")
    s.set_defaults(fn=cmd_sistema)
    args = p.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
