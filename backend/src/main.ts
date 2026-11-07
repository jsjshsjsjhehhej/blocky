// Avvio del backend.
//
//   DATABASE_URL     postgres://utente:password@host:5432/blocky  (produzione)
//   BLOCKY_MEMORY_DB 1 = database in memoria (PGlite), per provare senza PostgreSQL
//   PORT             default 8090
//   HOST             default 127.0.0.1 (solo questo PC); 0.0.0.0 per la rete
//                    (su Railway e' gia' 0.0.0.0: lo chiede la piattaforma)
//   ADMIN_TOKEN      token per le rotte /v1/admin (senza: spente)

import { buildApp } from './app.ts';
import { connectMemory, connectPostgres, type Db } from './db.ts';
import { migrate } from './migrate.ts';

const env = process.env;

let db: Db;
if (env.DATABASE_URL) {
  db = await connectPostgres(env.DATABASE_URL);
} else if (env.BLOCKY_MEMORY_DB === '1') {
  console.warn('[blocky] database IN MEMORIA: alla chiusura si perde tutto');
  db = await connectMemory();
} else {
  console.error('[blocky] manca DATABASE_URL (oppure BLOCKY_MEMORY_DB=1 per provare)');
  process.exit(1);
}

const applied = await migrate(db);
if (applied.length) console.log('[blocky] migrazioni applicate:', applied.join(', '));

if (env.ADMIN_TOKEN && env.ADMIN_TOKEN.length < 24) {
  console.error('[blocky] ADMIN_TOKEN troppo corto: almeno 24 caratteri');
  process.exit(1);
}

const app = buildApp({ db, adminToken: env.ADMIN_TOKEN, logger: true });
const port = Number(env.PORT ?? 8090);
// Railway (e simili) mette PORT e vuole l'ascolto su tutte le interfacce.
const host = env.HOST ?? (env.RAILWAY_ENVIRONMENT_NAME ? '0.0.0.0' : '127.0.0.1');
await app.listen({ port, host });

for (const sig of ['SIGINT', 'SIGTERM'] as const) {
  process.on(sig, async () => {
    await app.close();
    await db.close();
    process.exit(0);
  });
}
