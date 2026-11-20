# Ultimo Binario

Gioco originale per Blocky, scritto **solo in Luau** come progetto Rojo.

Porta il treno fino al Faro, a 10 000 stud. Lungo i binari ci sono città fantasma,
forti, accampamenti di banditi e avamposti, pieni di nemici e di cose da prendere.
La caldaia consuma carbone: se si svuota, il treno si ferma. Di notte arriva l'orda,
di giorno a volte una tempesta di sabbia.

## Come si gioca
| Tasto | Cosa fa |
|---|---|
| clic e trascina | prendi carbone, lingotti, munizioni e bende e portali dove vuoi |
| E | leva di marcia, apri le casse, prendi gli oggetti, mercante, officina, bacheca delle taglie |
| F | butta il carbone che hai in mano nella caldaia |
| siediti in cabina | sedile del macchinista: W e S alzano e abbassano il regolatore, salta per alzarti |
| 1 / 2 / 3 / 4 | revolver, fucile a leva, dinamite, pala |
| 5 | fucile a pompa, se lo compri dal mercante |
| clic | spara, lanci la dinamite, o dai un colpo di pala (alla testa il danno raddoppia) |
| H | usa una benda (+40 vita) |
| R | ricomincia, a partita finita |

- **Trascinare**: un oggetto lasciato sul treno viaggia con lui. Il carbone portato
  vicino alla bocca della caldaia la riempie; un lingotto portato sulla cassaforte del
  vagone si vende (15 gettoni).
- **Casse**: dentro trovi oggetti veri, che escono accanto alla cassa.
- **Mercante** (alla stazione delle città): munizioni, carbone, bende, dinamite e fucile a pompa.
- **Officina** (sul tender): corazza, caldaia grande, pistoni, torretta automatica, riparazioni.
  Se gli zombie rompono il treno (vita a zero), il treno si ferma finché non lo ripari.
- **Taglie**: la bacheca dello sceriffo nelle città dà una missione per volta.
- **Nemici**: Zombie, Corridore (veloce), Bruto (resistente), Soldato (nei forti),
  Bandito (spara da lontano e a volte lascia un lingotto).
- Il treno non parte senza di te: se tutti sono rimasti a terra, si ferma e aspetta.
  Se cadi sotto il terreno torni sul treno (-15 vita).
- **Record**: la distanza migliore si salva (DataStore) e compare nella classifica.

## Aprirlo nello Studio di Blocky
Menu **Script → Rojo (VS Code / AI)**, poi apri `default.project.json` e premi Play.
I file si aggiornano da soli mentre lo Studio è aperto.

Per una prova senza mouse: metti `Config.PROVA_AUTOMATICA = true` (il treno parte da
solo e ogni 5 secondi stampa in Output quanto costa ogni sistema), poi:
```
BLOCKY_SHOT=prova.png BLOCKY_SHOT_SEC=40 BLOCKY_SHOT_PLACE=baseplate BLOCKY_SHOT_PLAY=1 BLOCKY_ROJO=giochi/UltimoBinario/default.project.json build/bin/BlockyGameEngine.exe
```
Con `BLOCKY_PROFILE=1` il motore stampa anche, fase per fase, ogni frame oltre 25 ms.

## File
| File | Cosa contiene |
|---|---|
| `src/shared/Config.luau` | tutti i numeri del gioco (velocità, prezzi, nemici, armi, colori) |
| `src/shared/Costruisci.luau` | aiuti per creare le part (lascia passare un frame ogni 25) |
| `src/server/Main.server.luau` | la partita: ciclo, giorno e notte, comandi, chi sta sul treno, fine |
| `src/server/Treno.luau` | locomotiva, tender, vagone, cabina con sedile, leva e regolatore |
| `src/server/Mondo.luau` | binari, deserto, fiumi con ponti, mesas, a pezzi attorno al treno |
| `src/server/Insediamenti.luau` | città, forti, accampamenti, avamposti, il Faro |
| `src/server/Zombie.luau` | i nemici, l'orda notturna, colpi ed esplosioni |
| `src/server/Armi.luau` | revolver, fucile, pompa, dinamite (controlli sul server) |
| `src/server/Trasporto.luau` | gli oggetti da trascinare e le consegne |
| `src/server/Bottino.luau` | casse, lingotti, mercante, cassaforte, bende |
| `src/server/Officina.luau` | vita del treno, potenziamenti, torretta |
| `src/server/Missioni.luau` | le taglie dello sceriffo |
| `src/server/Meteo.luau` | le tempeste di sabbia |
| `src/server/Record.luau` | classifica e record salvato |
| `src/client/Hud.client.luau` | l'HUD e la schermata finale |
| `src/client/Arma.client.luau` | armi, tasti, scie dei colpi, esplosioni |
| `src/client/Guida.client.luau` | guidare dal sedile del macchinista |
| `src/client/Suoni.client.luau` | i suoni |

## Note su Blocky
Quello che non va come su Roblox è in `docs/limiti_trovati.md`, con il modo in cui il
gioco lo aggira. Per esempio i cilindri di Blocky hanno l'asse lungo Y (su Roblox X),
e gli zombie non hanno collisioni con i muri.
