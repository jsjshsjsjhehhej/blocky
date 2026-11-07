// Crea un creatore sul backend online e salva il suo token dove Blocky lo
// cerca per pubblicare, senza stamparlo.
//
//   ADMIN_TOKEN=... node scripts/create-publisher.ts https://<tuo>.up.railway.app walter
//
// Scrive, nella cartella dati di Blocky di questo PC (%APPDATA%\Blocky):
//   publish_token.txt  il token del creatore (resta segreto: non va condiviso)
//   cloud_url.txt      l'indirizzo del catalogo, cosi' Studio e hub lo usano
//
// Il token si vede una volta sola: nel database c'e' solo il suo hash. Se si
// perde, se ne crea un altro con un altro nome.

import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';

const [, , baseArg, name] = process.argv;
const admin = process.env.ADMIN_TOKEN ?? '';
if (!baseArg || !name || !admin) {
  console.error('uso: ADMIN_TOKEN=... node scripts/create-publisher.ts <indirizzo del backend> <nome creatore>');
  process.exit(1);
}
const base = baseArg.replace(/\/+$/, '');

const health = await fetch(`${base}/health`).catch((e: Error) => {
  console.error(`[blocky] ${base} non risponde: ${e.message}`);
  process.exit(1);
});
if (!health.ok) {
  console.error(`[blocky] ${base}/health ha risposto ${health.status}`);
  process.exit(1);
}

const r = await fetch(`${base}/v1/admin/publishers`, {
  method: 'POST',
  headers: { authorization: `Bearer ${admin}`, 'content-type': 'application/json' },
  body: JSON.stringify({ name }),
});
const body = (await r.json().catch(() => ({}))) as { token?: string; error?: string };
if (!r.ok || !body.token) {
  console.error(`[blocky] creatore non creato (${r.status}): ${body.error ?? 'risposta inattesa'}`);
  process.exit(1);
}

const dataDir = process.env.APPDATA ? path.join(process.env.APPDATA, 'Blocky') : path.resolve('blocky-data');
mkdirSync(dataDir, { recursive: true });
writeFileSync(path.join(dataDir, 'publish_token.txt'), body.token);
writeFileSync(path.join(dataDir, 'cloud_url.txt'), base);
console.log(`[blocky] creatore "${name}" creato.`);
console.log(`[blocky] token salvato in ${path.join(dataDir, 'publish_token.txt')} (non condividerlo)`);
console.log(`[blocky] catalogo impostato: ${base}`);
