// ESERCIZIO 01 - qui scrivi tu la logica. Ogni funzione ha il suo contratto
// nei commenti di reliable.h; qui sotto trovi cosa pensare, non come farlo.
#include "reliable.h"

namespace blk {

// ---------------------------------------------------------------------------
// TODO(1): aritmetica dei numeri di sequenza.
//   Contratto: seqGreater(1, 65535) == true  (1 viene DOPO 65535, giro compreso)
//              seqGreater(65535, 1) == false
//              seqGreater(5, 5)     == false
//   Idea: due numeri a 16 bit sono "vicini" se la loro distanza e' meno di
//   meta' dello spazio (32768). Dentro quella finestra, uno dei due e' avanti.
//   Scrivila a mano e provala con 6-7 coppie sulla carta PRIMA di compilare.
// ---------------------------------------------------------------------------
bool seqGreater(uint16_t a, uint16_t b) {
    (void)a; (void)b;
    return false; // TODO(1)
}

ReliableChannel::ReliableChannel(uint16_t startSeq) {
    (void)startSeq;
    // TODO(2): inizializza i campi che hai deciso in reliable.h (TODO A).
}

// ---------------------------------------------------------------------------
// TODO(3): invio.
//   sendReliable   -> da qualche parte devi tenere il messaggio finche' non
//                     sai che e' arrivato. Dagli un identificativo crescente
//                     (anche lui a 16 bit: stesso problema del giro!).
//   sendUnreliable -> tieni solo l'ultimo; il precedente non interessa piu'.
// ---------------------------------------------------------------------------
void ReliableChannel::sendReliable(const std::vector<uint8_t>& msg) {
    (void)msg; // TODO(3a)
}

void ReliableChannel::sendUnreliable(const std::vector<uint8_t>& msg) {
    (void)msg; // TODO(3b)
}

// ---------------------------------------------------------------------------
// TODO(4): costruire un pacchetto.
//   Intestazione (formato a tua scelta, ma deve dire almeno):
//     - il numero di sequenza di QUESTO pacchetto
//     - "l'ultimo pacchetto che ho ricevuto da te" + una maschera dei 32
//       precedenti (e' il trucco che rende le conferme robuste alla perdita
//       delle conferme stesse: README, sezione 3)
//   Poi il corpo: i reliable ancora non confermati che ci stanno (kMaxPacket!),
//   piu' l'eventuale unreliable.
//   Attenzione: NON rispedire un reliable a ogni tick. Aspetta un po' (qualche
//   decina di ms) prima di riprovare, altrimenti inondi la rete proprio quando
//   e' in difficolta'. Ti serve nowSec.
// ---------------------------------------------------------------------------
std::vector<uint8_t> ReliableChannel::buildPacket(double nowSec) {
    (void)nowSec;
    return {}; // TODO(4)
}

// ---------------------------------------------------------------------------
// TODO(5): ricevere un pacchetto.
//   1. controlla la lunghezza minima (spazzatura = ignora, senza crash)
//   2. aggiorna cio' che sai di aver ricevuto (per le conferme in uscita)
//   3. leggi le conferme che l'altro ti manda: quali tuoi pacchetti sono
//      arrivati? Da li' ricavi quali reliable non devi piu' rispedire
//   4. consegna i reliable NUOVI, nell'ordine giusto, senza duplicati
//      (un messaggio con id 7 puo' arrivare prima del 6: che ne fai?)
//   5. l'unreliable vale solo se il pacchetto e' piu' recente di quelli
//      gia' accettati (usa seqGreater)
// ---------------------------------------------------------------------------
void ReliableChannel::onPacket(const uint8_t* data, size_t len, double nowSec) {
    (void)data; (void)len; (void)nowSec; // TODO(5)
}

bool ReliableChannel::popReliable(std::vector<uint8_t>& out) {
    (void)out;
    return false; // TODO(6)
}

bool ReliableChannel::popUnreliable(std::vector<uint8_t>& out) {
    (void)out;
    return false; // TODO(6)
}

size_t ReliableChannel::pendingReliable() const {
    return 0; // TODO(7)
}

} // namespace blk
