// ESERCIZIO 01 - affidabilita' sopra UDP
//
// Leggi prima README.md. Questo file e' l'INTERFACCIA: non devi cambiarla
// (il banco di prova in test_main.cpp la usa cosi' com'e'). Cio' che scrivi
// tu sta in reliable.cpp e nei campi privati qui sotto.
//
// Il canale NON conosce i socket: riceve e produce byte. Cosi' lo puoi
// provare senza rete, e piu' avanti lo colleghi a LanNet cambiando solo chi
// chiama buildPacket()/onPacket().
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace blk {

// Un datagramma non deve superare l'MTU tipico di Internet (~1500) meno le
// intestazioni IP/UDP. Sopra questo valore l'IP frammenta il pacchetto, e se
// ne perde un frammento si perde tutto. Tieniti sotto.
static constexpr size_t kMaxPacket = 1200;

// Un messaggio applicativo piu' grande di questo non e' supportato.
static constexpr size_t kMaxMessage = 1000;

// Numeri di sequenza a 16 bit: dopo 65535 si torna a 0 ("wrap-around").
// true se `a` e' PIU' RECENTE di `b`, tenendo conto del giro.
// (Perche' non basta `a > b`? Rispondi alla domanda 1 del README.)
bool seqGreater(uint16_t a, uint16_t b);

class ReliableChannel {
public:
    // startSeq: da dove parte il contatore dei pacchetti in uscita. Il banco
    // di prova lo mette vicino a 65535 per costringerti a gestire il giro.
    explicit ReliableChannel(uint16_t startSeq = 0);

    // ---------------- lato invio ----------------

    // Messaggio che DEVE arrivare: consegnato una volta sola, nello stesso
    // ordine in cui l'hai spedito, anche se la rete perde/duplica/riordina.
    void sendReliable(const std::vector<uint8_t>& msg);

    // Messaggio "usa e getta" (es. la posizione): se si perde, pazienza.
    // Se ne accumulano piu' d'uno tra due pacchetti conta solo l'ultimo.
    void sendUnreliable(const std::vector<uint8_t>& msg);

    // Da chiamare a ogni tick (il banco di prova lo fa a 60 Hz). Restituisce
    // SEMPRE un pacchetto, anche se non c'e' niente da dire: le conferme
    // (ack) viaggiano nell'intestazione e devono poter partire. Mai piu' di
    // kMaxPacket byte.
    std::vector<uint8_t> buildPacket(double nowSec);

    // ---------------- lato ricezione ----------------

    // Da chiamare per ogni datagramma arrivato. Puo' ricevere pacchetti
    // duplicati, vecchi, fuori ordine, o spazzatura troppo corta: non deve
    // mai andare in crash ne' consegnare due volte lo stesso messaggio.
    void onPacket(const uint8_t* data, size_t len, double nowSec);

    // Messaggi reliable arrivati, in ordine. false = non ce ne sono.
    bool popReliable(std::vector<uint8_t>& out);

    // Ultimo messaggio unreliable valido. Mai uno piu' vecchio di uno gia'
    // consegnato. false = niente di nuovo.
    bool popUnreliable(std::vector<uint8_t>& out);

    // ---------------- diagnostica ----------------

    // Quanti messaggi reliable spediti non sono ancora stati confermati.
    // A fine prova, con la rete di nuovo buona, deve tornare a 0.
    size_t pendingReliable() const;

private:
    // TODO(A): decidi TU quali campi servono. Domande guida nel README
    // (sezione "Cosa devi ricordare"). Non c'e' una sola risposta giusta.
};

} // namespace blk
