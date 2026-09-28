# Attribuzioni e licenze di terzi

Questo file elenca il materiale di terzi distribuito con Blocky e le
condizioni che lo accompagnano.

**Non e' una formalita':** la licenza CC-BY dell'avatar predefinito *obbliga*
a citare l'autore. Distribuire senza questa pagina violerebbe la licenza, ed
e' l'unico motivo per cui la pubblicazione era bloccata.

Questo file deve essere incluso nel pacchetto di distribuzione, non solo nel
repository.

---

## Modelli 3D

### Avatar predefinito — `assets/roblox-noob.glb`

| | |
|---|---|
| Titolo | Roblox-Noob |
| Autore | **Roblox** — <https://sketchfab.com/Robloxs> |
| Fonte | <https://sketchfab.com/3d-models/roblox-noob-d5cd875b17d345b78a34d24296af0ceb> |
| Licenza | **CC-BY-4.0** — <http://creativecommons.org/licenses/by/4.0/> |

Pubblicato da Roblox stessa su Sketchfab. La licenza CC-BY-4.0 consente uso
commerciale, modifica e redistribuzione **a condizione di citare l'autore**,
che e' quello che fa questa pagina.

I dati di licenza non sono stati dedotti: sono scritti dentro al file, nel
blocco `asset.extras` del glTF. Si rileggono in qualsiasi momento con:

    head -c 2000 assets/roblox-noob.glb | strings | grep license

> **Nota sul marchio, che la licenza NON copre.** CC-BY riguarda il diritto
> d'autore, non i marchi registrati. "Roblox" resta un marchio di Roblox
> Corporation: la licenza permette di usare il modello, non di far credere
> che Blocky sia un prodotto Roblox o ad essa collegato. Vedi la sezione
> finale.

### `assets/character.fbx`

Esportato con Blender (nei metadati compare "Blender Foundation", che e'
l'autore dell'esportatore, non del modello). Nessuna traccia di provenienza
da terzi: niente scheletro Mixamo, nessuna intestazione di copyright.

Lo strumento `asset_audit` lo segnala solo perche' il **nome** e' generico e
potrebbe indicare materiale altrui — e' un avviso prudenziale, non un
problema riscontrato.

---

## Librerie

Compilate dentro Blocky, ognuna con la propria licenza:

| Libreria | Licenza | A cosa serve |
|---|---|---|
| Dear ImGui | MIT | tutta l'interfaccia |
| GLFW | Zlib/libpng | finestra e input su desktop |
| GLM | MIT | matematica 3D |
| Assimp | BSD 3-Clause | importazione modelli (.glb/.fbx/.obj) |
| Luau | MIT | esecuzione degli script |
| Lua | MIT | idem, percorso alternativo |
| stb (`stb_image`) | MIT / dominio pubblico | caricamento immagini |
| glad | MIT | caricamento funzioni OpenGL |
| ImGuiColorTextEdit | MIT | editor degli script in Studio |
| kuba--/zip | Unlicense | archivi per pubblicare i giochi |

Sono tutte licenze permissive: consentono uso commerciale e redistribuzione
anche in forma compilata. MIT e BSD chiedono di **conservare l'avviso di
copyright**, ed e' cio' che fa questa tabella.

Su Android si aggiungono componenti Google (GameActivity, `androidx.games`)
sotto **Apache 2.0**, e OpenSL ES, che fa parte del sistema operativo.

---

## Sul rapporto con Roblox

Blocky e' un motore indipendente, scritto da zero. **Non e' un prodotto
Roblox, non e' approvato da Roblox e non e' collegato a Roblox Corporation.**

Due cose restano da decidere prima di una pubblicazione ampia, e sono
questioni di **marchio**, non di codice:

1. **Il nome del file** `roblox-noob.glb` contiene un marchio registrato. La
   licenza non impone di mantenere quel nome: rinominarlo (per esempio
   `avatar_base.glb`) toglie ogni ambiguita' e non viola nulla, purche'
   questa pagina continui a citare autore, fonte e licenza.

2. **L'aspetto dell'interfaccia.** Blocky richiama volutamente lo stile
   classico di Roblox. Il codice e' tuo e questo non si discute, ma la
   somiglianza di nome, logo e aspetto e' l'area dove un marchio si difende.
   Un'identita' visiva propria elimina il rischio e non costa una riga di
   codice.
