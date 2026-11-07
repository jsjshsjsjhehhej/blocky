// Le rotte HTTP. buildApp non apre porte: main.ts fa listen, i test usano
// app.inject() senza rete.
//
//   GET    /health
//   GET    /v1/client/settings?platform=&version=&device=   flag per quel client
//   GET    /v1/client/version?platform=&version=            aggiornamenti
//   GET    /v1/admin/flags                                   (token admin)
//   PUT    /v1/admin/flags/:name                             (token admin)
//   DELETE /v1/admin/flags/:name?platform=                   (token admin)
//   POST   /v1/admin/releases                                (token admin)
//   giochi: vedi games.ts (PUT /v1/games/:id, GET /games, ...)

import Fastify, { type FastifyInstance, type FastifyReply, type FastifyRequest } from 'fastify';
import { createHash, timingSafeEqual } from 'node:crypto';
import type { Db } from './db.ts';
import { registerGames } from './games.ts';
import {
  type FlagRow,
  type ReleaseRow,
  isPlatform,
  kDeviceId,
  kFlagName,
  kVersion,
  resolveFlags,
  valueMatchesName,
  versionInfo,
} from './flags.ts';

export interface AppOptions {
  db: Db;
  /** Vuoto = rotte admin spente (rispondono 404, come se non esistessero). */
  adminToken?: string;
  logger?: boolean;
}

type Query = Record<string, string | undefined>;

function badRequest(reply: FastifyReply, error: string) {
  return reply.code(400).send({ error });
}

function tokenMatches(given: string, expected: string): boolean {
  // Confronto a tempo costante sugli hash: niente indizi dalla durata.
  const a = createHash('sha256').update(given).digest();
  const b = createHash('sha256').update(expected).digest();
  return timingSafeEqual(a, b);
}

export function buildApp(opts: AppOptions): FastifyInstance {
  const { db } = opts;
  const adminToken = opts.adminToken ?? '';
  const app = Fastify({ logger: opts.logger ?? false, bodyLimit: 64 * 1024 });

  app.get('/health', async () => ({ ok: true }));

  registerGames(app, db, adminToken, tokenMatches);

  // ---- Client ----

  app.get('/v1/client/settings', async (req: FastifyRequest<{ Querystring: Query }>, reply) => {
    const { platform, version, device } = req.query;
    if (!isPlatform(platform)) return badRequest(reply, 'platform: windows o android');
    if (!version || !kVersion.test(version)) return badRequest(reply, 'version: formato 1.2.3');
    if (device !== undefined && !kDeviceId.test(device)) return badRequest(reply, 'device non valido');
    const rows = (await db.query<FlagRow>(
      'SELECT name, platform, value, rollout_percent, min_version FROM fast_flags',
    )).rows;
    reply.header('cache-control', 'no-store');
    return { flags: resolveFlags(rows, platform, version, device ?? null) };
  });

  app.get('/v1/client/version', async (req: FastifyRequest<{ Querystring: Query }>, reply) => {
    const { platform, version } = req.query;
    if (!isPlatform(platform)) return badRequest(reply, 'platform: windows o android');
    if (!version || !kVersion.test(version)) return badRequest(reply, 'version: formato 1.2.3');
    const rows = (await db.query<ReleaseRow>(
      'SELECT version, min_supported, download_url, sha256, signature, notes FROM client_releases WHERE platform = $1',
      [platform],
    )).rows;
    reply.header('cache-control', 'no-store');
    return versionInfo(rows, version);
  });

  // ---- Admin ----

  app.register(async (admin) => {
    admin.addHook('onRequest', async (req, reply) => {
      if (!adminToken) return reply.code(404).send({ error: 'Not Found' });
      const auth = req.headers.authorization ?? '';
      const given = auth.startsWith('Bearer ') ? auth.slice(7) : '';
      if (!given || !tokenMatches(given, adminToken)) return reply.code(401).send({ error: 'token admin mancante o sbagliato' });
    });

    admin.get('/v1/admin/flags', async () => {
      const rows = (await db.query(
        'SELECT name, platform, value, rollout_percent, min_version, description, updated_at FROM fast_flags ORDER BY name, platform',
      )).rows;
      return { flags: rows };
    });

    admin.put('/v1/admin/flags/:name', async (req: FastifyRequest<{ Params: { name: string }; Body: Record<string, unknown> }>, reply) => {
      const { name } = req.params;
      const body = req.body ?? {};
      if (!kFlagName.test(name)) return badRequest(reply, 'nome: FFlag..., FInt... o FString...');
      if (!valueMatchesName(name, body.value)) return badRequest(reply, 'value: del tipo che il nome promette');
      const platform = body.platform ?? 'all';
      if (platform !== 'all' && !isPlatform(platform)) return badRequest(reply, 'platform: all, windows o android');
      const rollout = body.rolloutPercent ?? 100;
      if (!Number.isInteger(rollout) || (rollout as number) < 0 || (rollout as number) > 100)
        return badRequest(reply, 'rolloutPercent: 0..100');
      const minVersion = body.minVersion ?? null;
      if (minVersion !== null && (typeof minVersion !== 'string' || !kVersion.test(minVersion)))
        return badRequest(reply, 'minVersion: formato 1.2.3');
      const description = typeof body.description === 'string' ? body.description.slice(0, 500) : '';
      await db.query(
        `INSERT INTO fast_flags (name, platform, value, rollout_percent, min_version, description, updated_at)
         VALUES ($1, $2, $3::jsonb, $4, $5, $6, now())
         ON CONFLICT (name, platform) DO UPDATE SET
           value = EXCLUDED.value, rollout_percent = EXCLUDED.rollout_percent,
           min_version = EXCLUDED.min_version, description = EXCLUDED.description, updated_at = now()`,
        [name, platform, JSON.stringify(body.value), rollout, minVersion, description],
      );
      return { ok: true };
    });

    admin.delete('/v1/admin/flags/:name', async (req: FastifyRequest<{ Params: { name: string }; Querystring: Query }>, reply) => {
      const platform = req.query.platform ?? 'all';
      if (platform !== 'all' && !isPlatform(platform)) return badRequest(reply, 'platform: all, windows o android');
      const r = await db.query('DELETE FROM fast_flags WHERE name = $1 AND platform = $2 RETURNING name', [req.params.name, platform]);
      if (r.rows.length === 0) return reply.code(404).send({ error: 'flag non trovata' });
      return { ok: true };
    });

    admin.post('/v1/admin/releases', async (req: FastifyRequest<{ Body: Record<string, unknown> }>, reply) => {
      const b = req.body ?? {};
      if (!isPlatform(b.platform)) return badRequest(reply, 'platform: windows o android');
      for (const k of ['version', 'minSupported'] as const) {
        if (typeof b[k] !== 'string' || !kVersion.test(b[k] as string)) return badRequest(reply, `${k}: formato 1.2.3`);
      }
      if (typeof b.url !== 'string' || !/^https?:\/\/\S+$/.test(b.url)) return badRequest(reply, 'url: http(s)://...');
      if (typeof b.sha256 !== 'string' || !/^[0-9a-f]{64}$/.test(b.sha256)) return badRequest(reply, 'sha256: 64 cifre esadecimali');
      const signature = typeof b.signature === 'string' ? b.signature : '';
      const notes = typeof b.notes === 'string' ? b.notes.slice(0, 2000) : '';
      try {
        await db.query(
          `INSERT INTO client_releases (platform, version, min_supported, download_url, sha256, signature, notes)
           VALUES ($1, $2, $3, $4, $5, $6, $7)`,
          [b.platform, b.version, b.minSupported, b.url, b.sha256, signature, notes],
        );
      } catch (e) {
        if ((e as { code?: string }).code === '23505') return reply.code(409).send({ error: 'versione gia pubblicata' });
        throw e;
      }
      return reply.code(201).send({ ok: true });
    });
  });

  return app;
}
