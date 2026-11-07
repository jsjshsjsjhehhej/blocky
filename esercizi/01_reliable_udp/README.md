# Esercizio 01 — Affidabilità sopra UDP

**Obiettivo:** far viaggiare messaggi su UDP con tre garanzie che UDP non dà:
*arrivano*, *arrivano una volta sola*, *arrivano nell'ordine in cui li hai mandati*.
È il mattone su cui Roblox (RakNet) costruisce tutta la replica del mondo, e quello che oggi manca a
`LanNet` di Blocky: un `WorldDelta` perso è perso per sempre.

Non scrivi socket. Scrivi un **canale** che trasforma messaggi in byte e viceversa, e lo provi con una
linea finta che perde, duplica e riordina pacchetti. Quando i test passano, lo collegherai a `LanNet`.

## Come si lavora

```
cd RobloxEngine\esercizi\01_reliable_udp
g++ -std=c++17 -O1 -Wall -Wextra test_main.cpp reliable.cpp -o test.exe && test.exe
```

Parti con tutti i test rossi. I `TODO(n)` in `reliable.cpp` sono in ordine: falli uno alla volta e
ricompila a ogni passo. **Non toccare `test_main.cpp`** e l'interfaccia in `reliable.h` (puoi
aggiungere campi privati).

## 1. Il problema, in due righe

UDP ti dà datagrammi: ognuno può **sparire**, **arrivare due volte**, o **arrivare dopo uno più
recente**. Non c'è altro. Tutto il resto lo costruisci tu mettendo dei numeri nei pacchetti.

## 2. Gli ingredienti

**a) Numeri di sequenza.** Ogni pacchetto che spedisci ha un numero crescente. Chi riceve capisce cosa
è nuovo, vecchio, doppio, mancante. Sono a 16 bit per risparmiare byte, quindi dopo 65535 si torna a
0 (*wrap-around*): è la trappola classica di questo esercizio.

**b) Conferme (ack).** Chi riceve dice a chi ha mandato cosa è arrivato. Le conferme viaggiano
dentro i pacchetti normali, nell'intestazione, non in pacchetti separati.

**c) Rinvio.** Se un messaggio *reliable* non viene confermato dopo un po', lo rimandi.
Il "dopo un po'" è importante: rispedire a ogni tick peggiora la congestione proprio quando la rete
è in difficoltà (il test lo misura).

**d) Riordino in ricezione.** Il messaggio 7 può arrivare prima del 6. Se lo consegni subito rompi
l'ordine; se lo butti spedisci tutto due volte. Va tenuto da parte finché non arriva il 6.

**e) Reliable e unreliable convivono.** La posizione di un giocatore si spedisce 20 volte al secondo e
**non deve** essere affidabile: quella vecchia è inutile. Una chat sì. Stesso pacchetto, due regole.

## 3. Due modi di fare le conferme (scegli tu)

**Metodo A — conferme per messaggio (più semplice).** Il ricevitore dice solo "ho consegnato tutto fino
al messaggio N". Il mittente butta via ciò che è ≤ N. Facile da scrivere; difetto: se il messaggio 5
manca ma 6..9 sono arrivati, il mittente non lo sa e li rimanda tutti.

**Metodo B — conferme per pacchetto, con maschera (quello di Gaffer on Games / RakNet).**
L'intestazione contiene `ack` (ultimo pacchetto ricevuto) e `ackBits` (32 bit: quali dei 32
precedenti sono arrivati). Se una conferma si perde, quella dopo ripete l'informazione per altri 32
pacchetti, quindi le conferme sono robuste *senza rinvii*. Il mittente ricorda quali messaggi erano
dentro quale pacchetto. Più lavoro, più efficiente.

Per i test vanno bene entrambi. Consiglio: fai A, fai passare tutto, e **poi** riscrivi con B
(è una delle varianti qui sotto).

## 4. Domande a cui rispondere PRIMA di scrivere codice

1. Perché `a > b` non basta per confrontare due numeri di sequenza? Prova `a = 1, b = 65535` sulla
   carta. Quale distanza tra i due numeri ti fa dire "a è avanti" invece di "a è dietro"?
2. Cosa devi ricordare **per ogni messaggio reliable ancora non confermato**? (almeno 3 cose)
3. Cosa devi ricordare **lato ricezione** per consegnare in ordine e scartare i doppi? Cosa succede se
   il messaggio 6 non arriva mai più, per sempre? (Nel test non succede: il mittente insiste. Ma
   pensaci: è il motivo per cui nei giochi reali si mette un limite.)
4. Quanti byte restano per i messaggi dopo l'intestazione, se il pacchetto massimo è 1200? Cosa fai
   se i messaggi in attesa non ci stanno tutti?

Scrivi le risposte in 4 righe in un file `note.txt`: è il progetto del tuo codice.

## 5. Indizi (aprili solo se sei bloccato, in ordine)

<details><summary>Indizio 1 — TODO(1), seqGreater</summary>

Calcola la differenza `uint16_t(a - b)`. In aritmetica a 16 bit senza segno il giro avviene da solo.
Cerca quale intervallo di differenze significa "a è avanti". Verifica con le 8 coppie del test.
</details>

<details><summary>Indizio 2 — TODO(4/5), cosa mettere nell'intestazione</summary>

Metodo A: numero di sequenza del pacchetto (serve all'unreliable), "prossimo id reliable che mi
aspetto", quanti reliable seguono, lunghezza dell'unreliable. Per ogni reliable: id, lunghezza,
byte. Scrivi due piccole funzioni di aiuto per mettere/leggere un `uint16_t` in un `vector<uint8_t>`:
ti servono dappertutto.
</details>

<details><summary>Indizio 3 — il rinvio</summary>

Per ogni messaggio in attesa tieni l'ora dell'ultimo invio. In `buildPacket(now)` includi solo quelli
con `now - ultimoInvio >= soglia`. Una soglia di una decina di tick (≈ 0,1 s) basta per i test; una
versione seria la ricava dal tempo di andata e ritorno misurato.
</details>

<details><summary>Indizio 4 — ricezione in ordine</summary>

Una mappa `id → byte` per i messaggi arrivati "troppo presto". A ogni pacchetto, finché in mappa c'è
l'id che aspetti, spostalo nella coda di consegna e incrementa l'id atteso. Occhio al giro anche qui,
e a non accettare id lontanissimi (un pacchetto malevolo non deve poterti riempire la memoria).
</details>

## 6. Come capire se hai davvero finito

- I test passano **tutti** (37 controlli). Ricontrolla con `-fsanitize=address,undefined`:
  `g++ -std=c++17 -g -fsanitize=address,undefined test_main.cpp reliable.cpp -o test_san.exe`
- Sai spiegare a voce perché il test "giro dei numeri" ha `startSeq = 65500`.
- Dai al canale un pacchetto spazzatura da 3 byte e uno da 0 byte: non deve andare in crash.

## 7. Varianti (non guidate: provale da solo, è il punto dell'esercizio)

1. Riscrivi le conferme col **Metodo B** (`ackBits`). Cosa cambia nella banda del test?
2. Misura il **RTT** (andata e ritorno) con le conferme e usalo come soglia di rinvio, al posto del
   numero fisso. Aggiungi `double rttSec() const`.
3. Aggiungi un secondo canale reliable **non ordinato** (consegna appena arriva): quando ti conviene?
4. Il giocatore esce con 40 messaggi in coda mai confermati. Cosa fa il canale? Serve un timeout di
   connessione: scrivi il test prima del codice.
5. Quanto cambia la banda se tieni una finestra massima di messaggi "in volo"?

## 8. Dopo: collegarlo a Blocky

Quando i test passano, il canale non cambia: cambia solo chi lo usa. In `LanNet` un `sendto()` diventa
"prendi i byte da `buildPacket()`", e un `recvfrom()` diventa "dai i byte a `onPacket()`". Si parte
dai messaggi che oggi si perdono in silenzio (`WorldDelta`, `RemoteEventEx`), non da `NetState`: la
posizione resta unreliable. Quel passo lo facciamo insieme, ma solo dopo che questo è tuo.
