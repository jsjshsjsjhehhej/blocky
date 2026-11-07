// Test del backend con un PostgreSQL vero in memoria (PGlite) e app.inject():
// nessuna porta aperta, nessun server da installare.

import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { buildApp } from '../src/app.ts';
import { connectMemory, type Db } from '../src/db.ts';
import { migrate } from '../src/migrate.ts';
import { compareVersions, resolveFlags, rolloutBucket, versionInfo, type FlagRow } from '../src/flags.ts';

const token = 'token-di-prova-abbastanza-lungo-1234';
let db: Db;
let app: ReturnType<typeof buildApp>;

before(async () => {
  db = await connectMemory();
  assert.deepEqual(await migrate(db), ['001_client_settings.sql', '002_games.sql']);
  assert.deepEqual(await migrate(db), [], 'una seconda volta non riapplica niente');
  app = buildApp({ db, adminToken: token });
});

after(async () => {
  await app.close();
  await db.close();
});

const auth = { authorization: `Bearer ${token}` };

async function putFlag(name: string, body: Record<string, unknown>) {
  return app.inject({ method: 'PUT', url: `/v1/admin/flags/${name}`, headers: auth, payload: body });
}

test('versioni confrontate come numeri', () => {
  assert.equal(compareVersions('1.2.10', '1.2.9'), 1);
  assert.equal(compareVersions('0.9.0', '1.0.0'), -1);
  assert.equal(compareVersions('2.0.0', '2.0.0'), 0);
});

test('rollout: stesso dispositivo stesso secchiello, ~percentuale giusta', () => {
  assert.equal(rolloutBucket('FFlagX', 'dev-1'), rolloutBucket('FFlagX', 'dev-1'));
  const rows: FlagRow[] = [{ name: 'FFlagX', platform: 'all', value: true, rollout_percent: 30, min_version: null }];
  let on = 0;
  for (let i = 0; i < 2000; ++i) if (resolveFlags(rows, 'windows', '1.0.0', `dev-${i}`).FFlagX) ++on;
  assert.ok(on > 500 && on < 700, `30% di 2000 ~ 600, avuti ${on}`);
  assert.deepEqual(resolveFlags(rows, 'windows', '1.0.0', null), {}, 'senza id: niente rollout parziali');
});

test('la riga della piattaforma vince su "all", min_version filtra', () => {
  const rows: FlagRow[] = [
    { name: 'FIntFps', platform: 'all', value: 60, rollout_percent: 100, min_version: null },
    { name: 'FIntFps', platform: 'android', value: 30, rollout_percent: 100, min_version: null },
    { name: 'FFlagNuovo', platform: 'all', value: true, rollout_percent: 100, min_version: '0.2.0' },
  ];
  assert.deepEqual(resolveFlags(rows, 'android', '0.1.0', null), { FIntFps: 30 });
  assert.deepEqual(resolveFlags(rows, 'windows', '0.2.0', null), { FFlagNuovo: true, FIntFps: 60 });
});

test('admin: senza token o con token sbagliato e\' rifiutato', async () => {
  let r = await app.inject({ method: 'GET', url: '/v1/admin/flags' });
  assert.equal(r.statusCode, 401);
  r = await app.inject({ method: 'GET', url: '/v1/admin/flags', headers: { authorization: 'Bearer sbagliato' } });
  assert.equal(r.statusCode, 401);
  const off = buildApp({ db });
  r = await off.inject({ method: 'GET', url: '/v1/admin/flags', headers: auth });
  assert.equal(r.statusCode, 404, 'senza ADMIN_TOKEN le rotte admin non esistono');
  await off.close();
});

test('admin: il tipo del valore deve seguire il nome', async () => {
  assert.equal((await putFlag('FFlagPlaceLauncher', { value: 'si' })).statusCode, 400);
  assert.equal((await putFlag('FIntMaxFps', { value: 1.5 })).statusCode, 400);
  assert.equal((await putFlag('Ciao', { value: true })).statusCode, 400);
  assert.equal((await putFlag('FFlagX', { value: true, rolloutPercent: 101 })).statusCode, 400);
});

test('flag impostate dal server arrivano al client', async () => {
  assert.equal((await putFlag('FFlagPlaceLauncher', { value: false, description: 'spegne il launcher' })).statusCode, 200);
  assert.equal((await putFlag('FStringHubMotd', { value: 'Evento stasera alle 21!' })).statusCode, 200);
  assert.equal((await putFlag('FIntMaxFps', { value: 30, platform: 'android' })).statusCode, 200);

  let r = await app.inject({ url: '/v1/client/settings?platform=android&version=0.1.0&device=abc-123' });
  assert.equal(r.statusCode, 200);
  assert.deepEqual(r.json().flags, { FFlagPlaceLauncher: false, FIntMaxFps: 30, FStringHubMotd: 'Evento stasera alle 21!' });

  r = await app.inject({ url: '/v1/client/settings?platform=windows&version=0.1.0' });
  assert.deepEqual(r.json().flags, { FFlagPlaceLauncher: false, FStringHubMotd: 'Evento stasera alle 21!' });

  // Cambiata sul server: il prossimo client la vede subito, senza nuova app.
  await putFlag('FFlagPlaceLauncher', { value: true });
  r = await app.inject({ url: '/v1/client/settings?platform=windows&version=0.1.0' });
  assert.equal(r.json().flags.FFlagPlaceLauncher, true);

  r = await app.inject({ method: 'DELETE', url: '/v1/admin/flags/FStringHubMotd', headers: auth });
  assert.equal(r.statusCode, 200);
  r = await app.inject({ url: '/v1/client/settings?platform=windows&version=0.1.0' });
  assert.equal(r.json().flags.FStringHubMotd, undefined);
});

test('client: parametri sbagliati -> 400', async () => {
  for (const q of ['platform=ios&version=0.1.0', 'platform=windows&version=1', 'platform=windows&version=0.1.0&device=a%20b']) {
    const r = await app.inject({ url: `/v1/client/settings?${q}` });
    assert.equal(r.statusCode, 400, q);
  }
});

test('versioni: aggiornamento disponibile e obbligatorio', async () => {
  let r = await app.inject({ url: '/v1/client/version?platform=windows&version=0.1.0' });
  assert.equal(r.json().latest, null, 'nessuna release: niente da fare');

  const sha = 'a'.repeat(64);
  const post = (body: Record<string, unknown>) =>
    app.inject({ method: 'POST', url: '/v1/admin/releases', headers: auth, payload: body });
  assert.equal((await post({ platform: 'windows', version: '0.2.0', minSupported: '0.1.0', url: 'https://example.com/b-0.2.0.zip', sha256: sha })).statusCode, 201);
  assert.equal((await post({ platform: 'windows', version: '0.10.0', minSupported: '0.2.0', url: 'https://example.com/b-0.10.0.zip', sha256: sha, notes: 'Nuovo Studio' })).statusCode, 201);
  assert.equal((await post({ platform: 'windows', version: '0.10.0', minSupported: '0.2.0', url: 'https://example.com/x', sha256: sha })).statusCode, 409);
  assert.equal((await post({ platform: 'windows', version: '0.11.0', minSupported: '0.2.0', url: 'ftp://x', sha256: sha })).statusCode, 400);

  r = await app.inject({ url: '/v1/client/version?platform=windows&version=0.1.0' });
  const v = r.json();
  assert.equal(v.latest, '0.10.0', '0.10.0 e\' piu\' nuova di 0.2.0');
  assert.equal(v.updateAvailable, true);
  assert.equal(v.mustUpdate, true, '0.1.0 < minSupported 0.2.0');
  assert.equal(v.notes, 'Nuovo Studio');

  r = await app.inject({ url: '/v1/client/version?platform=windows&version=0.2.5' });
  assert.equal(r.json().mustUpdate, false);
  assert.equal(r.json().updateAvailable, true);

  r = await app.inject({ url: '/v1/client/version?platform=android&version=0.1.0' });
  assert.equal(r.json().latest, null, 'le release windows non valgono per android');
});

test('versionInfo senza release', () => {
  assert.equal(versionInfo([], '1.0.0').mustUpdate, false);
});
