// Migrazioni: i file migrations/NNN_nome.sql applicati una volta sola, in
// ordine. Ognuno in una transazione insieme alla riga che lo segna come
// applicato: se fallisce a meta', non resta niente di mezzo.

import { readdir, readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import type { Db } from './db.ts';

const here = path.dirname(fileURLToPath(import.meta.url));
export const migrationsDir = path.join(here, '..', 'migrations');

const kName = /^\d{3}_[a-z0-9_]+\.sql$/;

export async function migrate(db: Db, dir = migrationsDir): Promise<string[]> {
  await db.exec(`CREATE TABLE IF NOT EXISTS schema_migrations (
    name text PRIMARY KEY,
    applied_at timestamptz NOT NULL DEFAULT now()
  )`);
  const done = new Set(
    (await db.query<{ name: string }>('SELECT name FROM schema_migrations')).rows.map((r) => r.name),
  );
  const files = (await readdir(dir)).filter((f) => kName.test(f)).sort();
  const applied: string[] = [];
  for (const f of files) {
    if (done.has(f)) continue;
    const sql = await readFile(path.join(dir, f), 'utf8');
    // Il nome entra nell'SQL: e' sicuro perche' kName ammette solo [0-9a-z_.].
    await db.exec(`BEGIN;\n${sql}\nINSERT INTO schema_migrations (name) VALUES ('${f}');\nCOMMIT;`);
    applied.push(f);
  }
  return applied;
}
