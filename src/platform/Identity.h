#pragma once

/**
 * Chi sta giocando — con una prova, non sulla fiducia.
 *
 * ── Cosa c'era prima, e perche' non bastava ───────────────────────────────
 * engine/GoogleAuth.h apriva la pagina di Google nel browser e poi l'utente
 * SCRIVEVA LA SUA EMAIL A MANO dentro Blocky. Google non diceva mai niente al
 * gioco: chiunque poteva digitare l'email di chiunque, e il gioco ci credeva.
 * Non era un'autenticazione, era una scenografia.
 *
 * ── Come funziona davvero ─────────────────────────────────────────────────
 * 1. Il sistema mostra il selettore di account e consegna un ID TOKEN: un
 *    documento firmato da Google che dice "questo e' davvero lui".
 * 2. Qualcuno deve VERIFICARE quella firma.
 *
 * Il passo 2 e' tutto. Se la verifica avviene qui nel client, chi modifica il
 * client la salta ed entra come chiunque. Quindi la verifica va fatta sul
 * server, che il giocatore non controlla. Ne segue una conseguenza che vale
 * la pena scrivere: IL LOGIN VERO DIPENDE DAL SERVER. Senza, si puo' avere un
 * nome da mostrare, non un'identita' su cui fondare monete o proprieta'.
 *
 * Per questo Account distingue `verified`: finche' il server non ha
 * controllato il token, e' `false` e chi legge deve trattarlo come "dice di
 * chiamarsi cosi'", non come "e' lui".
 *
 * ── La forma ──────────────────────────────────────────────────────────────
 * Identica a platform/FilePick.h: si chiede con una richiamata, perche' la
 * risposta arriva dopo — l'utente esce dal gioco, sceglie l'account, magari
 * inserisce la password. La richiamata arriva SEMPRE, anche in caso di
 * rinuncia o errore, con il motivo scritto: un'attesa che non finisce e' il
 * peggior modo di fallire.
 *
 * ── Non configurato = lo dice ─────────────────────────────────────────────
 * Senza i Client ID (vedi GOOGLE_SETUP.md) signIn risponde con un errore
 * esplicito, non con un silenzio. E' la stessa regola di PlatformServices.h,
 * scritta col sangue sulla porta Android: un ripiego muto e' peggio di un no.
 */

#include <functional>
#include <string>

#include "platform/Platform.h"

namespace plat {
namespace auth {

struct Account {
    std::string id;           // identificativo stabile ("sub" del token Google)
    std::string email;
    std::string displayName;
    std::string idToken;      // il documento firmato, da far verificare al server

    /**
     * Il SERVER ha controllato la firma del token?
     *
     * false non vuol dire "falso": vuol dire "non ancora provato". Un client
     * da solo non puo' stabilirlo, perche' e' proprio il client la cosa di cui
     * non ci si puo' fidare.
     */
    bool verified = false;

    bool valid() const { return !id.empty(); }
};

using SignInCallback = std::function<void(const Account&, const std::string& error)>;

namespace detail {

struct State {
    SignInCallback pending;
    Account        result;
    std::string    error;
    bool           hasResult = false;
    bool           waiting = false;

    Account current;          // chi e' connesso adesso
    std::string webClientId;  // dalla configurazione; vuoto = non configurato
};

inline State& state() { static State s; return s; }

} // namespace detail

/** Chi e' connesso adesso (id vuoto = nessuno). */
inline const Account& currentAccount() { return detail::state().current; }

/** I Client ID sono stati impostati? Senza, il login non puo' funzionare. */
inline bool configured() { return !detail::state().webClientId.empty(); }

/** Impostata all'avvio leggendo la configurazione. Vedi GOOGLE_SETUP.md. */
inline void setWebClientId(const std::string& id) { detail::state().webClientId = id; }
inline const std::string& webClientId() { return detail::state().webClientId; }

/** Il lato piattaforma deposita qui il risultato. Vuoto + errore = fallito. */
inline void deliverSignIn(const Account& acc, const std::string& error) {
    auto& s = detail::state();
    s.result = acc;
    s.error = error;
    s.hasResult = true;
}

inline bool signInPending() { return detail::state().waiting; }

/**
 * Chiede l'accesso. La richiamata arriva da pumpAuth(), mai da qui dentro.
 */
inline void signIn(SignInCallback cb) {
    auto& s = detail::state();
    if (s.waiting) return;
    s.pending = std::move(cb);
    s.waiting = true;
    s.hasResult = false;
    s.error.clear();
    s.result = Account{};

    if (!configured()) {
        deliverSignIn(Account{},
                      "Accesso Google non configurato: manca il Client ID "
                      "(vedi GOOGLE_SETUP.md)");
        return;
    }

#if defined(__ANDROID__)
    if (androidSignInHook()) androidSignInHook()(s.webClientId.c_str());
    else deliverSignIn(Account{}, "Accesso Google non disponibile su questo dispositivo");
#else
    // Su desktop non esiste un SDK di sistema: servira' il giro OAuth nel
    // browser con la risposta raccolta su loopback, e un server che verifichi
    // il token. Finche' non c'e', lo si dice invece di fingere.
    deliverSignIn(Account{}, "Accesso Google su PC non ancora disponibile");
#endif
}

inline void signOut() {
    auto& s = detail::state();
    s.current = Account{};
}

/** Consegna il risultato dentro il frame. Una volta per frame. */
inline void pumpAuth() {
    auto& s = detail::state();
    if (!s.hasResult) return;

    SignInCallback cb = std::move(s.pending);
    const Account acc = s.result;
    const std::string err = s.error;
    s.pending = nullptr;
    s.hasResult = false;
    s.waiting = false;
    s.result = Account{};
    s.error.clear();

    if (acc.valid() && err.empty()) s.current = acc;
    if (cb) cb(acc, err);
}

} // namespace auth
} // namespace plat
