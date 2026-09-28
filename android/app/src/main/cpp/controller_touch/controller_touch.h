#pragma once

/**
 * libcontroller_touch — un controller che finge di essere un dito.
 *
 * ── Il problema che risolve ───────────────────────────────────────────────
 * Blocky su Android e' costruito attorno al TOCCO: ogni pulsante, cursore e
 * lista risponde a un dito che si appoggia e si stacca. Un controller invece
 * parla di pulsanti e stick, e non esiste nessun dito da nessuna parte.
 *
 * Il tentativo ovvio — usare la navigazione a controller di ImGui — non
 * funziona bene qui: e' un SECONDO percorso di input, con regole sue (fuoco,
 * finestre figlie in cui bisogna "entrare"), che convive male con quello a
 * tocco. Premere ✕ su Play entrava nel riquadro invece di premere il
 * pulsante, e sembrava che il tasto fosse rotto.
 *
 * ── L'idea, che e' migliore ───────────────────────────────────────────────
 * Non insegnare al gioco cos'e' un controller: insegnare al controller a
 * comportarsi da dito. Lo stick muove un cursore visibile, ✕ appoggia e
 * stacca il dito in quel punto. Tutto cio' che gia' funziona col tocco
 * funziona col pad, il giorno stesso, senza toccare una riga di interfaccia.
 *
 * E' anche il modo in cui le console fanno funzionare le interfacce nate per
 * il mouse.
 *
 * ── Perche' una libreria separata ─────────────────────────────────────────
 * Non dipende da Blocky: entra uno stato di pad, esce una posizione con un
 * "premuto" si/no. Nessun riferimento a GLFW, ImGui, OpenGL o al motore.
 * Puo' servire tale e quale a libblockyvr.so (dove i controller VR hanno lo
 * stesso problema: puntare in un'interfaccia nata per le dita) e a un
 * eventuale porting su console.
 *
 * L'API e' in C, non in C++: fra due .so diverse un'interfaccia C ha un ABI
 * stabile, mentre nomi e layout del C++ cambiano col compilatore.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Quello che il pad sta facendo in questo istante. */
typedef struct CtlTouchInput {
    float stickX;      /* -1..1, destra positiva */
    float stickY;      /* -1..1, ALTO positivo (come il mondo, non come lo schermo) */
    int   confirmDown; /* ✕ su DualSense, A su Xbox: 1 se premuto */
    int   padPresent;  /* 0 = nessun controller: il cursore sparisce */
    float dt;          /* secondi dall'ultimo aggiornamento */
    float screenW;
    float screenH;
} CtlTouchInput;

/** Il "dito" che ne risulta. */
typedef struct CtlTouchOutput {
    float cursorX;     /* pixel */
    float cursorY;
    int   touchDown;   /* 1 = dito appoggiato adesso */
    int   pressed;     /* 1 solo nel frame in cui si appoggia */
    int   released;    /* 1 solo nel frame in cui si stacca */
    int   visible;     /* 1 = disegna il cursore */
} CtlTouchOutput;

/** Rimette il cursore al centro. Da chiamare quando cambia la dimensione. */
void ctlTouchReset(float screenW, float screenH);

/** Un frame. Si puo' chiamare sempre, anche senza controller collegato. */
void ctlTouchUpdate(const CtlTouchInput* in, CtlTouchOutput* out);

/** L'ultimo risultato, per chi disegna e non vuole ricalcolare nulla. */
void ctlTouchLast(CtlTouchOutput* out);

/** Nome e versione, per il log di avvio. */
const char* ctlTouchVersion(void);

#ifdef __cplusplus
}
#endif
