// Catalogo giochi: pubblicare, elencare, scaricare, permessi e limiti.
// PostgreSQL vero in memoria (PGlite) e app.inject(): nessuna rete.

import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { buildApp } from '../src/app.ts';
import { connectMemory, type Db } from '../src/db.ts';
import { migrate } from '../src/migrate.ts';
import { parsePublish, safePath } from '../src/games.ts';

const adminToken = 'token-admin-di-prova-abbastanza-lungo';
let db: Db;
let app: ReturnType<typeof buildApp>;
let tokenA = '';
let tokenB = '';

const b64 = (s: string | Buffer) => Buffer.from(s).toString('base64');
const place = JSON.stringify({ format: 'BlockyPlace', version: 2, placeName: 'Prova', parts: [] });

function publish(id: string, token: string, extra: Record<string, unknown> = {}) {
  return app.inject({
    method: 'PUT',
    url: `/v1/games/${id}`,
    headers: { authorization: `Bearer ${token}` },
    payload: {
      title: 'Laboratorio di Fisica',
      description: '20 prove di fisica',
      maxPlayers: 8,
      place: 'place.json',
      thumbnail: 'thumbnail.png',
      files: [
        { path: 'place.json', data: b64(place) },
        { path: 'thumbnail.png', data: b64(Buffer.from([0x89, 0x50, 0x4e, 0x47, 1, 2, 3])) },
        { path: 'scripts/main.lua', data: b64('print("ciao")') },
      ],
      ...extra,
    },
  });
}

before(async () => {
  db = await connectMemory();
  await migrate(db);
  app = buildApp({ db, adminToken });
  for (const [name, set] of [['walter', (t: string) => { tokenA = t; }], ['altro', (t: string) => { tokenB = t; }]] as const) {
    const r = await app.inject({
      method: 'POST', url: '/v1/admin/publishers',
      headers: { authorization: `Bearer ${adminToken}` }, payload: { name },
    });
    assert.equal(r.statusCode, 200, r.body);
    set(r.json().token);
  }
});

after(async () => {
  await app.close();
  await db.close();
});

test('percorsi dei file: solo relativi e semplici', () => {
  assert.ok(safePath('place.json'));
  assert.ok(safePath('assets/meshes/auto.glb'));
  for (const p of ['../x', '/etc/passwd', 'a\\b', 'a/../b', '', 'a//b', '.', 'a/./b'])
    assert.ok(!safePath(p), p);
});

test('una pubblicazione senza il place viene rifiutata', () => {
  const r = parsePublish({ title: 'x', files: [{ path: 'a.txt', data: b64('a') }] });
  assert.equal(typeof r, 'string');
});

test('creatori: token solo all\'admin, nome unico', async () => {
  const noAdmin = await app.inject({ method: 'POST', url: '/v1/admin/publishers', payload: { name: 'ladro' } });
  assert.equal(noAdmin.statusCode, 401);
  const dup = await app.inject({
    method: 'POST', url: '/v1/admin/publishers',
    headers: { authorization: `Bearer ${adminToken}` }, payload: { name: 'walter' },
  });
  assert.equal(dup.statusCode, 409);
  assert.match(tokenA, /^blk_[0-9a-f]{64}$/);
  const stored = (await db.query<{ token_sha256: string }>('SELECT token_sha256 FROM publishers')).rows;
  assert.ok(stored.every((r) => !r.token_sha256.includes(tokenA)), 'nel database solo l\'hash');
});

test('pubblicare richiede un token valido', async () => {
  assert.equal((await publish('laboratorio', '')).statusCode, 401);
  assert.equal((await publish('laboratorio', 'blk_finto')).statusCode, 401);
  assert.equal((await publish('NON_VALIDO', tokenA)).statusCode, 400);
});

test('pubblica, poi il gioco e\' nell\'elenco con i suoi file', async () => {
  const r = await publish('laboratorio', tokenA);
  assert.equal(r.statusCode, 200, r.body);
  assert.equal(r.json().version, 1);
  assert.equal(r.json().creator, 'walter');

  const list = await app.inject({ method: 'GET', url: '/games' });
  assert.equal(list.statusCode, 200);
  const games = list.json();
  assert.equal(games.length, 1);
  assert.deepEqual(Object.keys(games[0]).slice(0, 5), ['id', 'title', 'description', 'thumbnail', 'place'],
    'stesso formato del PC in rete locale');
  assert.equal(games[0].id, 'laboratorio');
  assert.equal(games[0].place, 'place.json');

  const files = await app.inject({ method: 'GET', url: '/games/laboratorio/files' });
  assert.deepEqual(files.json(), ['place.json', 'scripts/main.lua', 'thumbnail.png']);

  const p = await app.inject({ method: 'GET', url: '/games/laboratorio/place.json' });
  assert.equal(p.statusCode, 200);
  assert.equal(p.body, place);
  assert.equal(p.headers['content-length'], String(Buffer.byteLength(place)));

  const s = await app.inject({ method: 'GET', url: '/games/laboratorio/scripts/main.lua' });
  assert.equal(s.body, 'print("ciao")');
  const png = await app.inject({ method: 'GET', url: '/games/laboratorio/thumbnail.png' });
  assert.deepEqual([...png.rawPayload], [0x89, 0x50, 0x4e, 0x47, 1, 2, 3], 'byte intatti');

  assert.equal((await app.inject({ method: 'GET', url: '/games/laboratorio/../x' })).statusCode >= 400, true);
  assert.equal((await app.inject({ method: 'GET', url: '/games/laboratorio/nonce.txt' })).statusCode, 404);
});

test('ripubblicare aggiorna la versione e sostituisce i file', async () => {
  const r = await publish('laboratorio', tokenA, {
    title: 'Laboratorio 2',
    thumbnail: '',
    files: [{ path: 'place.json', data: b64('{"nuovo":true}') }],
  });
  assert.equal(r.statusCode, 200, r.body);
  assert.equal(r.json().version, 2);
  const files = await app.inject({ method: 'GET', url: '/games/laboratorio/files' });
  assert.deepEqual(files.json(), ['place.json'], 'i file vecchi non restano');
  const games = (await app.inject({ method: 'GET', url: '/games' })).json();
  assert.equal(games[0].title, 'Laboratorio 2');
  assert.equal(games[0].version, 2);
});

test('un altro creatore non puo\' sovrascrivere o togliere il gioco', async () => {
  assert.equal((await publish('laboratorio', tokenB)).statusCode, 403);
  const del = await app.inject({
    method: 'DELETE', url: '/v1/games/laboratorio', headers: { authorization: `Bearer ${tokenB}` },
  });
  assert.equal(del.statusCode, 403);
  const p = await app.inject({ method: 'GET', url: '/games/laboratorio/place.json' });
  assert.equal(p.body, '{"nuovo":true}', 'il gioco e\' rimasto quello di walter');
});

test('limiti: file troppi, percorsi cattivi', async () => {
  const many = Array.from({ length: 301 }, (_, i) => ({ path: `f${i}.txt`, data: b64('x') }));
  assert.equal((await publish('troppi', tokenA, { files: [{ path: 'place.json', data: b64(place) }, ...many] })).statusCode, 400);
  assert.equal((await publish('cattivo', tokenA, {
    files: [{ path: 'place.json', data: b64(place) }, { path: '../fuori.txt', data: b64('x') }],
  })).statusCode, 400);
});

test('il creatore toglie il gioco: sparisce da elenco e file', async () => {
  const del = await app.inject({
    method: 'DELETE', url: '/v1/games/laboratorio', headers: { authorization: `Bearer ${tokenA}` },
  });
  assert.equal(del.statusCode, 200);
  assert.deepEqual((await app.inject({ method: 'GET', url: '/games' })).json(), []);
  assert.equal((await app.inject({ method: 'GET', url: '/games/laboratorio/place.json' })).statusCode, 404);
});
