// Catalogo dei giochi: pubblicare dallo Studio, vedere e scaricare dall'hub.
//
//   PUT    /v1/games/:id                 pubblica o aggiorna (token del creatore)
//   DELETE /v1/games/:id                 toglie dal catalogo (token del creatore)
//   POST   /v1/admin/publishers          crea un creatore e il suo token (token admin)
//   GET    /games                        elenco (pubblico)
//   GET    /games/:id/files              elenco dei file del gioco (pubblico)
//   GET    /games/:id/<percorso>         un file del gioco (pubblico)
//
// GET /games e i file usano gli stessi percorsi e lo stesso formato del PC
// che serve i giochi in rete locale (engine/GamesHost.h): il downloader del
// client e' lo stesso, cambia solo l'indirizzo.

import type { FastifyInstance, FastifyReply, FastifyRequest } from 'fastify';
import { createHash, randomBytes } from 'node:crypto';
import type { Db } from './db.ts';

export const kGameId = /^[a-z0-9_]{1,48}$/;
export const kPublisherName = /^[A-Za-z0-9_]{3,24}$/;
const kSegment = /^[A-Za-z0-9_.-]{1,80}$/;

export const kLimits = {
  maxFiles: 300,
  maxFileBytes: 16 * 1024 * 1024,
  maxTotalBytes: 30 * 1024 * 1024,
  // base64 pesa 4/3 dei byte, piu' il resto del JSON.
  bodyBytes: 45 * 1024 * 1024,
};

/** Percorso relativo sicuro: niente .., niente / iniziale, niente \, segmenti semplici. */
export function safePath(p: unknown): p is string {
  if (typeof p !== 'string' || p.length < 1 || p.length > 200) return false;
  const parts = p.split('/');
  if (parts.length > 6) return false;
  return parts.every((s) => kSegment.test(s) && s !== '.' && s !== '..');
}

export function sha256Hex(data: Uint8Array | string): string {
  return createHash('sha256').update(data).digest('hex');
}

export function newPublisherToken(): string {
  return 'blk_' + randomBytes(32).toString('hex');
}

function contentType(path: string): string {
  const ext = path.slice(path.lastIndexOf('.') + 1).toLowerCase();
  switch (ext) {
    case 'json': case 'blocky': case 'blockyplace': return 'application/json';
    case 'png': return 'image/png';
    case 'jpg': case 'jpeg': return 'image/jpeg';
    case 'lua': case 'luau': case 'txt': return 'text/plain; charset=utf-8';
    case 'wav': return 'audio/wav';
    case 'ogg': return 'audio/ogg';
    case 'mp3': return 'audio/mpeg';
    default: return 'application/octet-stream';
  }
}

function bad(reply: FastifyReply, code: number, error: string) {
  return reply.code(code).send({ error });
}

interface PublishFile { path: string; data: Buffer; sha256: string }

/** Legge e controlla il corpo di una pubblicazione. Stringa = cosa non va. */
export function parsePublish(body: unknown): string | {
  title: string; description: string; maxPlayers: number; place: string; thumbnail: string; files: PublishFile[];
} {
  if (!body || typeof body !== 'object') return 'corpo JSON mancante';
  const b = body as Record<string, unknown>;
  const title = typeof b.title === 'string' ? b.title.trim() : '';
  if (title.length < 1 || title.length > 60) return 'title: da 1 a 60 caratteri';
  const description = typeof b.description === 'string' ? b.description.slice(0, 500) : '';
  const maxPlayers = b.maxPlayers === undefined ? 8 : b.maxPlayers;
  if (!Number.isInteger(maxPlayers) || (maxPlayers as number) < 1 || (maxPlayers as number) > 32)
    return 'maxPlayers: da 1 a 32';
  if (!Array.isArray(b.files) || b.files.length < 1) return 'files: almeno il place';
  if (b.files.length > kLimits.maxFiles) return `files: al massimo ${kLimits.maxFiles}`;
  const files: PublishFile[] = [];
  const seen = new Set<string>();
  let total = 0;
  for (const f of b.files as unknown[]) {
    const ff = (f ?? {}) as Record<string, unknown>;
    if (!safePath(ff.path)) return `percorso non valido: ${String(ff.path).slice(0, 80)}`;
    if (seen.has(ff.path)) return `file ripetuto: ${ff.path}`;
    seen.add(ff.path);
    if (typeof ff.data !== 'string') return `data mancante: ${ff.path}`;
    const data = Buffer.from(ff.data, 'base64');
    if (data.length > kLimits.maxFileBytes) return `file troppo grande: ${ff.path}`;
    total += data.length;
    if (total > kLimits.maxTotalBytes) return `gioco troppo grande: al massimo ${kLimits.maxTotalBytes / 1024 / 1024} MB`;
    files.push({ path: ff.path, data, sha256: sha256Hex(data) });
  }
  const place = typeof b.place === 'string' ? b.place : 'place.json';
  if (!seen.has(place)) return `il place (${place}) non e' tra i file`;
  const thumbnail = typeof b.thumbnail === 'string' ? b.thumbnail : '';
  if (thumbnail && !seen.has(thumbnail)) return `l'anteprima (${thumbnail}) non e' tra i file`;
  return { title, description, maxPlayers: maxPlayers as number, place, thumbnail, files };
}

interface GameRow {
  id: string; title: string; description: string; max_players: number; version: number;
  place_file: string; thumbnail_file: string; creator: string; updated_at: string | Date;
}

export function registerGames(app: FastifyInstance, db: Db, adminToken: string,
                              tokenMatches: (given: string, expected: string) => boolean) {
  // Il creatore dal token "Authorization: Bearer blk_...".
  async function publisherOf(req: FastifyRequest): Promise<{ id: number; name: string } | null> {
    const auth = req.headers.authorization ?? '';
    const token = auth.startsWith('Bearer ') ? auth.slice(7).trim() : '';
    if (!token.startsWith('blk_') || token.length > 200) return null;
    const rows = (await db.query<{ id: string; name: string; banned: boolean }>(
      'SELECT id, name, banned FROM publishers WHERE token_sha256 = $1', [sha256Hex(token)],
    )).rows;
    if (!rows.length || rows[0].banned) return null;
    return { id: Number(rows[0].id), name: rows[0].name };
  }

  // ---- Pubblicare ----

  app.put('/v1/games/:id', { bodyLimit: kLimits.bodyBytes },
    async (req: FastifyRequest<{ Params: { id: string }; Body: unknown }>, reply) => {
      const { id } = req.params;
      if (!kGameId.test(id)) return bad(reply, 400, 'id: minuscole, cifre e _ (max 48)');
      const who = await publisherOf(req);
      if (!who) return bad(reply, 401, 'token del creatore mancante o non valido');
      const parsed = parsePublish(req.body);
      if (typeof parsed === 'string') return bad(reply, 400, parsed);

      const total = parsed.files.reduce((n, f) => n + f.data.length, 0);
      const result = await db.transaction(async (tx) => {
        const cur = (await tx.query<{ owner_id: string; version: number }>(
          'SELECT owner_id, version FROM games WHERE id = $1 FOR UPDATE', [id],
        )).rows[0];
        if (cur && Number(cur.owner_id) !== who.id) return { forbidden: true as const };
        const version = cur ? cur.version + 1 : 1;
        if (cur) {
          await tx.query(
            `UPDATE games SET title = $2, description = $3, max_players = $4, version = $5,
               place_file = $6, thumbnail_file = $7, total_bytes = $8, listed = true, updated_at = now()
             WHERE id = $1`,
            [id, parsed.title, parsed.description, parsed.maxPlayers, version, parsed.place, parsed.thumbnail, total],
          );
          await tx.query('DELETE FROM game_files WHERE game_id = $1', [id]);
        } else {
          await tx.query(
            `INSERT INTO games (id, owner_id, title, description, max_players, version, place_file, thumbnail_file, total_bytes)
             VALUES ($1, $2, $3, $4, $5, 1, $6, $7, $8)`,
            [id, who.id, parsed.title, parsed.description, parsed.maxPlayers, parsed.place, parsed.thumbnail, total],
          );
        }
        for (const f of parsed.files) {
          await tx.query(
            'INSERT INTO game_files (game_id, path, size, sha256, data) VALUES ($1, $2, $3, $4, $5)',
            [id, f.path, f.data.length, f.sha256, f.data],
          );
        }
        return { forbidden: false as const, version };
      });
      if (result.forbidden) return bad(reply, 403, `il gioco "${id}" e' di un altro creatore`);
      return { ok: true, id, version: result.version, files: parsed.files.length, bytes: total, creator: who.name };
    });

  app.delete('/v1/games/:id', async (req: FastifyRequest<{ Params: { id: string } }>, reply) => {
    const { id } = req.params;
    if (!kGameId.test(id)) return bad(reply, 400, 'id non valido');
    const who = await publisherOf(req);
    if (!who) return bad(reply, 401, 'token del creatore mancante o non valido');
    const rows = (await db.query<{ owner_id: string }>('SELECT owner_id FROM games WHERE id = $1', [id])).rows;
    if (!rows.length) return bad(reply, 404, 'gioco non trovato');
    if (Number(rows[0].owner_id) !== who.id) return bad(reply, 403, 'non e\' un tuo gioco');
    await db.query('UPDATE games SET listed = false, updated_at = now() WHERE id = $1', [id]);
    return { ok: true };
  });

  // ---- Creatori (admin) ----

  app.post('/v1/admin/publishers', async (req: FastifyRequest<{ Body: Record<string, unknown> }>, reply) => {
    if (!adminToken) return bad(reply, 404, 'Not Found');
    const auth = req.headers.authorization ?? '';
    const given = auth.startsWith('Bearer ') ? auth.slice(7) : '';
    if (!given || !tokenMatches(given, adminToken)) return bad(reply, 401, 'token admin mancante o sbagliato');
    const name = (req.body ?? {}).name;
    if (typeof name !== 'string' || !kPublisherName.test(name)) return bad(reply, 400, 'name: 3-24 lettere, cifre o _');
    const token = newPublisherToken();
    const ins = await db.query(
      'INSERT INTO publishers (name, token_sha256) VALUES ($1, $2) ON CONFLICT (name) DO NOTHING RETURNING id',
      [name, sha256Hex(token)],
    );
    if (!ins.rows.length) return bad(reply, 409, `il creatore "${name}" esiste gia'`);
    // Il token si vede solo adesso: nel database c'e' solo il suo hash.
    return { name, token };
  });

  // ---- Elenco e file (pubblici) ----

  app.get('/games', async (_req, reply) => {
    const rows = (await db.query<GameRow>(
      `SELECT g.id, g.title, g.description, g.max_players, g.version, g.place_file, g.thumbnail_file,
              p.name AS creator, g.updated_at
         FROM games g JOIN publishers p ON p.id = g.owner_id
        WHERE g.listed AND NOT p.banned
        ORDER BY g.updated_at DESC
        LIMIT 200`,
    )).rows;
    reply.header('cache-control', 'no-store');
    // Tutte le chiavi sempre presenti e nello stesso ordine: il client cerca
    // ogni campo a partire da "id" (vedi RemoteGames.h).
    return rows.map((r) => ({
      id: r.id,
      title: r.title,
      description: r.description || `di ${r.creator}`,
      thumbnail: r.thumbnail_file,
      place: r.place_file,
      creator: r.creator,
      maxPlayers: r.max_players,
      version: r.version,
      updatedAt: new Date(r.updated_at).toISOString(),
    }));
  });

  async function listedGame(id: string) {
    return (await db.query<{ id: string }>(
      `SELECT g.id FROM games g JOIN publishers p ON p.id = g.owner_id
        WHERE g.id = $1 AND g.listed AND NOT p.banned`, [id],
    )).rows.length > 0;
  }

  app.get('/games/:id/*', async (req: FastifyRequest<{ Params: { id: string; '*': string } }>, reply) => {
    const { id } = req.params;
    const rel = req.params['*'];
    if (!kGameId.test(id)) return bad(reply, 400, 'id non valido');
    if (!(await listedGame(id))) return bad(reply, 404, 'gioco non trovato');
    if (rel === 'files' || rel === 'files.json') {
      const rows = (await db.query<{ path: string }>(
        'SELECT path FROM game_files WHERE game_id = $1 ORDER BY path', [id],
      )).rows;
      reply.header('cache-control', 'no-store');
      return rows.map((r) => r.path);
    }
    if (!safePath(rel)) return bad(reply, 400, 'percorso non valido');
    const rows = (await db.query<{ data: Uint8Array; sha256: string }>(
      'SELECT data, sha256 FROM game_files WHERE game_id = $1 AND path = $2', [id, rel],
    )).rows;
    if (!rows.length) return bad(reply, 404, 'file non trovato');
    reply.header('content-type', contentType(rel));
    reply.header('etag', `"${rows[0].sha256}"`);
    return reply.send(Buffer.from(rows[0].data));
  });
}
