# Backend Blocky su Railway

Il backend (catalogo giochi + impostazioni del client) gira su Railway con
un PostgreSQL. HTTPS e indirizzo pubblico li da' Railway.

I server di gioco (BlockyServer) **non** vanno su Railway: usano UDP, che
Railway non supporta. Restano sul PC (o, piu' avanti, su una VPS).

## Una volta sola

Da `backend/`, in un terminale:

1. Installa la CLI e fai il login (si apre il browser, l'account e' tuo):

   ```
   npm install -g @railway/cli
   railway login
   ```

2. Crea il progetto, aggiungi PostgreSQL e il servizio del backend:

   ```
   railway init --name blocky
   railway add --database postgres
   railway add --service backend
   ```

3. Variabili del servizio `backend` (dal sito: progetto → backend →
   Variables, oppure con la CLI):

   - `DATABASE_URL` = `${{Postgres.DATABASE_URL}}` (il riferimento, scritto
     cosi': Railway lo collega al database)
   - `ADMIN_TOKEN` = una stringa casuale lunga almeno 24 caratteri. Tienila
     per te: serve solo per creare i creatori. Per generarne una:

     ```
     node -e "console.log(require('crypto').randomBytes(24).toString('hex'))"
     ```

4. Carica il codice e dai al servizio un indirizzo pubblico:

   ```
   railway up --service backend
   railway domain --service backend
   ```

   Il secondo comando stampa l'indirizzo, tipo
   `https://backend-production-xxxx.up.railway.app`. Le migrazioni del
   database partono da sole all'avvio. Prova: `<indirizzo>/health` deve
   rispondere `{"ok":true}`.

5. Il tuo token da creatore (lo salva nella cartella di Blocky, non lo
   stampa):

   ```
   set ADMIN_TOKEN=<il token admin del punto 3>
   node scripts/create-publisher.ts https://<indirizzo> walter
   ```

Da qui: pubblica dallo Studio → il gioco va online → compare nell'hub di
ogni Blocky. Perche' anche i telefoni lo trovino senza scrivere niente,
l'indirizzo va messo in `src/engine/CloudCatalog.h` (`kGamesUrl`) e l'app
ricompilata.

## Aggiornare il backend

```
railway up --service backend
```

## Cosa costa

Railway ha un piano di prova; poi si paga a consumo (il backend e
PostgreSQL di Blocky sono piccoli). Il prezzo lo vedi nel sito di Railway.
