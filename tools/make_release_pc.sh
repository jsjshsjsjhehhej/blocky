#!/usr/bin/env bash
#
# Prepara il pacchetto di Blocky per PC.
#
# ── Perche' serve uno script e non "copio build/bin" ──────────────────────
# In build/bin non c'e' solo il gioco: ci sono eseguibili di test, file di
# lavoro, la configurazione locale dell'interfaccia, i giochi ospitati in
# prova — e soprattutto blocky_dev_offline.txt.
#
# Quest'ultimo e' il motivo per cui lo script esiste. La sua sola presenza
# accanto all'eseguibile DISATTIVA il gate di rete (vedi NetGate.h:27):
# chiunque lo avesse nel pacchetto salterebbe i controlli d'integrita'.
# E' un file vuoto, invisibile in mezzo agli altri, e copiando a mano una
# cartella ci finisce dentro senza che nessuno se ne accorga.
#
# Qui la lista di cosa entra e' ESPLICITA: si copia solo cio' che e' elencato.
# Tutto il resto, per costruzione, resta fuori.
#
# Uso:  bash tools/make_release_pc.sh [cartella_destinazione]

set -uo pipefail
cd "$(dirname "$0")/.."

BIN="build/bin"
OUT="${1:-dist/Blocky-PC}"
VERSION="$(cat version.txt 2>/dev/null || echo "0.0.0")"

echo "Blocky $VERSION -> $OUT"
echo ""

if [ ! -f "$BIN/libblocky.dll" ]; then
    echo "ERRORE: $BIN/libblocky.dll non trovato. Compila prima:"
    echo "  cmake --build build"
    exit 1
fi

# ── Controllo: e' una build di distribuzione? ─────────────────────────────
# Con BLOCKY_DEV attivo i log di sicurezza restano nel binario e raccontano
# come funzionano le difese. Non e' una build da pubblicare.
if grep -q "BLOCKY_STRIP:BOOL=ON" build/CMakeCache.txt 2>/dev/null; then
    echo "  build di distribuzione (BLOCKY_STRIP=ON)  OK"
else
    echo "  ATTENZIONE: BLOCKY_STRIP non e' ON."
    echo "  I log di sicurezza finirebbero nel binario pubblicato."
    echo "  Riconfigura con:  cmake -S . -B build -DBLOCKY_STRIP=ON"
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT"

# ── Cosa entra, elencato a uno a uno ─────────────────────────────────────
PROGRAMMI=(
    Blocky.exe              # lanciatore principale
    BlockyStudio.exe        # editor
    BlockyGameEngine.exe    # client di gioco
    libblocky.dll           # il motore
    libblocky.dll.sha256    # impronta, la verifica l'auto-update
    version.txt
)

# Server: utili a chi ospita, inutili a chi gioca e basta.
SERVER=(
    BlockyServer.exe
    blocky_hostd.exe
    BlockyPool.exe
    scan_servers.exe
)

mancanti=0
for f in "${PROGRAMMI[@]}"; do
    if [ -f "$BIN/$f" ]; then
        cp "$BIN/$f" "$OUT/"
    else
        echo "  MANCA (obbligatorio): $f"
        mancanti=$((mancanti + 1))
    fi
done
[ "$mancanti" -gt 0 ] && { echo ""; echo "FALLITO: $mancanti file obbligatori mancanti."; exit 1; }

mkdir -p "$OUT/server"
for f in "${SERVER[@]}"; do
    [ -f "$BIN/$f" ] && cp "$BIN/$f" "$OUT/server/"
done

# Gli asset: cartella intera, ma senza gli scarti delle modifiche fallite.
cp -r assets "$OUT/assets"
find "$OUT/assets" -name "*.tmp.*" -delete 2>/dev/null

# Le licenze DEVONO viaggiare col pacchetto: la CC-BY dell'avatar obbliga a
# citare l'autore, e citarlo solo nel repository non basta a chi scarica.
for f in ATTRIBUZIONI.md README.md; do
    [ -f "$f" ] && cp "$f" "$OUT/"
done

# ── Controllo finale: nulla di pericoloso e' passato ─────────────────────
# Si verifica il RISULTATO, non le intenzioni: una riga aggiunta per sbaglio
# alla lista qui sopra verrebbe comunque fermata qui.
echo ""
VIETATI=(
    blocky_dev_offline.txt   # disattiva il gate di rete
    imgui.ini                # la disposizione delle finestre di CHI ha compilato
    .token                   # segreto del server dei giochi
    accounts.txt             # account locali
)
trovati=0
for v in "${VIETATI[@]}"; do
    if find "$OUT" -name "$v" | grep -q .; then
        echo "  PERICOLO: nel pacchetto c'e' $v"
        trovati=$((trovati + 1))
    fi
done
for pattern in "test_*.exe" "*.tmp.*" "*.pdb" "*.obj"; do
    n=$(find "$OUT" -name "$pattern" | wc -l)
    [ "$n" -gt 0 ] && { echo "  PERICOLO: $n file $pattern nel pacchetto"; trovati=$((trovati + n)); }
done

if [ "$trovati" -gt 0 ]; then
    echo ""
    echo "FALLITO: il pacchetto contiene file che non devono essere pubblicati."
    exit 1
fi

echo "  nessun file di sviluppo, test o segreto  OK"

TOT=$(du -sh "$OUT" | cut -f1)
NFILE=$(find "$OUT" -type f | wc -l)
echo ""
echo "Pronto: $OUT  ($NFILE file, $TOT)"
echo ""
echo "Da provare su un PC che NON ha mai avuto Blocky, prima di pubblicare:"
echo "  - si avvia senza chiedere librerie mancanti"
echo "  - il gate di rete e' attivo (senza blocky_dev_offline.txt deve chiederlo)"
