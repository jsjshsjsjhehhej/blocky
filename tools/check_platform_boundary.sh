#!/usr/bin/env bash
#
# Il confine di piattaforma di Blocky, verificato invece che sperato.
#
# Regola: fuori da src/platform/ il motore non chiama il sistema operativo.
# Chiama plat::sys::*, che e' l'unico posto dove ogni piattaforma deve
# riempire la sua casella.
#
# Perche' serve un controllo automatico e non basta la buona volonta':
# src/gfx/GfxCore.h e' un layer grafico scritto bene, con la stessa
# intenzione, e viene usato da 1 file su 20 — le altre 699 chiamate lo
# scavalcano. Nessuno l'ha deciso: e' semplicemente quello che succede a un
# layer che nessuno e' obbligato ad attraversare.
#
# Le eccezioni non sono nascoste: sono elencate qui sotto con il motivo, e
# il conteggio viene stampato ogni volta. Un debito che si vede e' un debito
# che prima o poi si paga.
#
# Uso:  bash tools/check_platform_boundary.sh
# Esce 1 se trova una chiamata nuova fuori dal confine.

set -uo pipefail
cd "$(dirname "$0")/.."

# API di sistema che devono passare da plat::sys::*
BANNED='ShellExecuteA?W?|WSAStartup|GetModuleFileNameA?W?|PlaySoundA?W?|mciSendStringA?W?|XInputGetState|MessageBoxA?W?'

# File ancora non migrati, con il motivo. Da accorciare, mai da allungare.
declare -A EXEMPT=(
  ["src/engine/SoundService.h"]="audio Windows non ancora dietro plat::sys"
  ["src/platform/GamepadInput.h"]="XInput: in platform/, ma senza voce nel facade"
  ["src/engine/LanNet.h"]="WSAStartup via netEnsureSockets(), da sostituire con plat::sys::netInit"
  ["src/engine/HttpClient.h"]="idem, socket portabili con init proprio"
  ["src/engine/HttpMini.h"]="idem"
  ["src/blocky_servers/blocky_server.cpp"]="idem, lato server"
  ["src/blocky_servers/blocky_hostd.cpp"]="idem, lato server"
  ["src/engine/Integrity.h"]="anti-tamper: legge l'eseguibile, per forza specifico"
  ["src/engine/AntiCheat.h"]="idem"
  ["src/platform/CrashHandler.h"]="gestore crash, dentro platform/"
  ["src/platform/BlockyFileAssoc.h"]="registro di Windows, dentro platform/"
  ["src/render/WindowIcon.h"]="icona finestra Win32, da spostare"
  ["src/vk_main.cpp"]="demo Vulkan autonomo, fuori dal motore"
  ["src/blocky_servers/blocky_pool.cpp"]="WSAStartup lato server, come gli altri server"
  ["src/tools/AutoUpdater.h"]="lanciatori minimi: non hanno src/ nell include path (provato)"
  ["src/tools/blocky_launcher.cpp"]="lanciatore .exe: e per definizione il guscio Windows"
  ["src/tools/blocky_editor_launcher.cpp"]="idem"
  ["src/tools/blocky_studio_launcher.cpp"]="idem"
)

viol=0
exempt_hits=0

while IFS= read -r f; do
  case "$f" in
    src/platform/SystemApi.h|src/platform/Platform.h) continue ;;
    *.tmp.*) continue ;;
  esac

  n=$(grep -cE "\b($BANNED)\s*\(" "$f" 2>/dev/null || true)
  [ "${n:-0}" -eq 0 ] && continue

  if [ -n "${EXEMPT[$f]:-}" ]; then
    exempt_hits=$((exempt_hits + n))
    printf '  debito  %-42s %3d  (%s)\n' "$f" "$n" "${EXEMPT[$f]}"
  else
    printf 'VIOLAZIONE %-42s %3d chiamate diverte al sistema\n' "$f" "$n"
    grep -nE "\b($BANNED)\s*\(" "$f" | sed 's/^/           /'
    viol=$((viol + n))
  fi
done < <(find src -name "*.h" -o -name "*.cpp" | sort)

echo ""
if [ "$viol" -gt 0 ]; then
  echo "FALLITO: $viol chiamate al sistema fuori dal confine."
  echo "Passa da plat::sys::* (src/platform/SystemApi.h), oppure aggiungi"
  echo "il file alle eccezioni QUI SOPRA con un motivo scritto."
  exit 1
fi

echo "OK: nessuna chiamata nuova fuori dal confine."
echo "Debito noto: $exempt_hits chiamate in ${#EXEMPT[@]} file elencati sopra."
