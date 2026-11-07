// Accesso al database. In produzione PostgreSQL vero (pg); nei test e in
// sviluppo PGlite, che e' lo stesso PostgreSQL compilato in WebAssembly e gira
// dentro Node: stesse query, stesso SQL, niente server da installare.

export interface QueryResult<T> {
  rows: T[];
}

export interface Db {
  query<T = Record<string, unknown>>(sql: string, params?: unknown[]): Promise<QueryResult<T>>;
  /** Piu' istruzioni in una volta (le migrazioni). Senza parametri. */
  exec(sql: string): Promise<void>;
  /** Tutto o niente: se fn lancia un errore, nessuna delle sue query resta. */
  transaction<T>(fn: (tx: Db) => Promise<T>): Promise<T>;
  close(): Promise<void>;
}

export async function connectPostgres(url: string): Promise<Db> {
  const pg = await import('pg');
  const pool = new pg.default.Pool({ connectionString: url, max: 10 });
  // Senza questo, una connessione che cade da sola fa crashare il processo.
  pool.on('error', (err: Error) => console.error('[db] connessione persa:', err.message));
  return {
    async query<T>(sql: string, params: unknown[] = []) {
      const r = await pool.query(sql, params as unknown[]);
      return { rows: r.rows as T[] };
    },
    async exec(sql: string) {
      await pool.query(sql);
    },
    async transaction<T>(fn: (tx: Db) => Promise<T>) {
      // Una connessione sola per tutta la transazione: con il pool ogni query
      // potrebbe finire su una connessione diversa.
      const client = await pool.connect();
      const tx: Db = {
        async query<U>(sql: string, params: unknown[] = []) {
          const r = await client.query(sql, params as unknown[]);
          return { rows: r.rows as U[] };
        },
        async exec(sql: string) { await client.query(sql); },
        transaction: () => Promise.reject(new Error('transazioni annidate non supportate')),
        close: async () => {},
      };
      try {
        await client.query('BEGIN');
        const out = await fn(tx);
        await client.query('COMMIT');
        return out;
      } catch (e) {
        await client.query('ROLLBACK').catch(() => {});
        throw e;
      } finally {
        client.release();
      }
    },
    close: () => pool.end(),
  };
}

export async function connectMemory(): Promise<Db> {
  const { PGlite } = await import('@electric-sql/pglite');
  const db = new PGlite();
  return {
    async query<T>(sql: string, params: unknown[] = []) {
      const r = await db.query<T>(sql, params as unknown[]);
      return { rows: r.rows };
    },
    async exec(sql: string) {
      await db.exec(sql);
    },
    async transaction<T>(fn: (tx: Db) => Promise<T>) {
      return db.transaction(async (t) => {
        const tx: Db = {
          async query<U>(sql: string, params: unknown[] = []) {
            const r = await t.query<U>(sql, params as unknown[]);
            return { rows: r.rows };
          },
          async exec(sql: string) { await t.exec(sql); },
          transaction: () => Promise.reject(new Error('transazioni annidate non supportate')),
          close: async () => {},
        };
        return fn(tx);
      });
    },
    close: () => db.close(),
  };
}
