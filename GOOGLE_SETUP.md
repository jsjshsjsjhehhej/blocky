# Servizi Google in Blocky — cosa serve, in che ordine

Questo file elenca **cosa devi fare tu** (account, chiavi, costi) e **cosa faccio
io** (codice). La divisione non è arbitraria: creare account e inserire
credenziali richiede le tue password, e quelle non le tocco.

---

## Dati del tuo progetto, già pronti

| | valore |
|---|---|
| Nome pacchetto | `com.blocky.preview` |
| SHA-1 (chiave di **debug**) | `98:51:1A:94:5C:A8:C2:D1:F4:BB:35:E1:E2:A7:82:D0:BF:4E:4F:1C` |
| minSdk / targetSdk | 24 / 37 |

> La SHA-1 qui sopra è quella di **debug**, quella con cui l'APK è firmato
> adesso. Quando pubblicherai davvero servirà anche la SHA-1 della chiave di
> **rilascio**, che è un'altra: Google le vuole entrambe registrate, altrimenti
> il login funziona in sviluppo e smette di funzionare il giorno della
> pubblicazione. È un classico.

---

## Il vincolo che decide l'ordine dei lavori

Un login Google funziona così:

1. Android mostra il selettore account e restituisce all'app un **ID token**,
   cioè un documento firmato da Google che dice "questo è davvero lui".
2. Qualcuno deve **verificare quella firma**.

Il punto 2 è tutto. Se la verifica avviene nel client, chi modifica il client
salta il controllo ed entra come chiunque — inclusi i tuoi account. La verifica
deve stare su una macchina che il giocatore non controlla: **il server**.

**Quindi il login vero non è indipendente dal VPS: ne dipende.**

Quello che c'è oggi in `src/engine/GoogleAuth.h` non verifica niente: apre la
pagina di Google nel browser e poi l'utente **scrive l'email a mano**. Chiunque
può digitare l'email di chiunque.

---

## Ordine consigliato

### 1. Firebase Crashlytics — *fallo per primo*

**Perché per primo:** è l'unico che non ha prerequisiti a pagamento, e risolve
un problema aperto vero — il crash al primo avvio che non siamo mai riusciti a
riprodurre. Con Crashlytics il rapporto arriva dal dispositivo reale, con lo
stack.

**Cosa fai tu:**
1. <https://console.firebase.google.com> → *Aggiungi progetto* (gratis)
2. Aggiungi un'app **Android**, nome pacchetto `com.blocky.preview`
3. Incolla la SHA-1 qui sopra
4. Scarica `google-services.json` e mettilo in `android/app/`

**Costo:** zero (piano Spark).

**Cosa faccio io:** plugin Gradle, NDK Crashlytics per i crash nativi (i nostri
sono in C++, non in Java — serve l'integrazione NDK o non si vede niente),
caricamento automatico dei simboli.

---

### 2. Google Sign-In — *dopo il VPS*

**Cosa fai tu:**
1. <https://console.cloud.google.com> → progetto (lo stesso di Firebase va bene)
2. *API e servizi → Schermata consenso OAuth* → compila
3. *Credenziali → Crea → ID client OAuth*, e creane **due**:
   - tipo **Android** → nome pacchetto + SHA-1
   - tipo **Applicazione web** → serve al server per verificare il token, **e
     serve anche all'app Android** (è il valore da passare a Credential Manager)
4. Mandami i due Client ID — **non sono segreti**, stanno dentro l'app.
   Il *client secret* invece è segreto: **non incollarmelo qui**, va solo nella
   configurazione del server.

**Costo:** zero.

**Cosa faccio io:** `androidx.credentials` + Google ID nell'APK, il ponte JNI,
la verifica del token sul server, e su PC il flusso OAuth via browser con
risposta su loopback.

---

### 3. Play Games Services

**Cosa fai tu:**
1. Account **Play Console**: **25 € una tantum**
2. Crea l'app, poi *Play Games Services → Configura*
3. Collega il progetto Google Cloud del punto 2

**Cosa dà:** profilo giocatore, obiettivi, classifiche, salvataggi nel cloud.

**Nota onesta:** i salvataggi cloud di Play Games valgono **solo su Android**.
Su PC non esistono. Se vuoi un account unico fra PC e telefono — ed è quello
che hai chiesto — la fonte di verità deve essere il **tuo** server, non Google.
Play Games diventa un extra, non la base.

---

### 4. Play Billing — *verifica prima se puoi*

**Prerequisiti che non dipendono dal codice:**
- Account Play Console (i 25 € di sopra)
- **Un profilo pagamenti intestato a chi può incassare**: Google versa su un
  conto con dati fiscali. Serve maggiore età e, in molti casi, una posizione
  fiscale.
- Google trattiene **15–30%**

**Controlla questo punto prima che io scriva qualunque riga**: se il profilo
pagamenti non si può aprire, il codice è inutile.

Oggi `src/engine/Purchase.h` apre un link di pagamento di ripiego. Funziona,
ma fuori dal Play Store — e vendere valuta di gioco in un'app Android
aggirando Billing **viola le regole di Google** e porta alla rimozione
dell'app. Non è un rischio teorico.

---

## Riassunto dei costi

| | costo | blocca cosa |
|---|---|---|
| Firebase Crashlytics | gratis | niente |
| Google Sign-In | gratis | serve il VPS per verificare |
| Play Games | 25 € una tantum | — |
| Play Billing | 25 € + profilo fiscale + 15-30% | potrebbe essere impossibile |

---

## Cosa NON ti chiederò mai

Password, codici a due fattori, il *client secret* OAuth, dati di pagamento.
Se una guida qualsiasi ti dice di incollarmi uno di questi, è sbagliata: il
secret vive solo nella configurazione del server, dove nessun giocatore lo
vede.
