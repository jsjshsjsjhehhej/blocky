// Logica delle impostazioni del client, senza database: quale valore di ogni
// flag riceve un certo dispositivo.

import { createHash } from 'node:crypto';

export const kPlatforms = ['windows', 'android'] as const;
export type Platform = (typeof kPlatforms)[number];

export const kVersion = /^\d+\.\d+\.\d+$/;
export const kFlagName = /^(FFlag|FInt|FString)[A-Za-z0-9_]{1,80}$/;
export const kDeviceId = /^[A-Za-z0-9-]{1,64}$/;

export type FlagValue = boolean | number | string;

export interface FlagRow {
  name: string;
  platform: 'all' | Platform;
  value: FlagValue;
  rollout_percent: number;
  min_version: string | null;
}

export function isPlatform(p: unknown): p is Platform {
  return typeof p === 'string' && (kPlatforms as readonly string[]).includes(p);
}

/** -1, 0, 1 come strcmp, per "1.2.10" > "1.2.9". */
export function compareVersions(a: string, b: string): number {
  const pa = a.split('.').map(Number);
  const pb = b.split('.').map(Number);
  for (let i = 0; i < 3; ++i) {
    if (pa[i] !== pb[i]) return pa[i] < pb[i] ? -1 : 1;
  }
  return 0;
}

/** Il valore va bene per il tipo che il nome promette? */
export function valueMatchesName(name: string, value: unknown): value is FlagValue {
  if (name.startsWith('FFlag')) return typeof value === 'boolean';
  if (name.startsWith('FInt')) return Number.isSafeInteger(value);
  if (name.startsWith('FString')) return typeof value === 'string' && value.length <= 2000;
  return false;
}

/**
 * Secchiello 0..99 del dispositivo per quella flag. Dipende anche dal nome:
 * chi e' tra il primo 10% di una flag non lo e' per forza di tutte le altre.
 */
export function rolloutBucket(flag: string, device: string): number {
  const h = createHash('sha256').update(`${flag}:${device}`).digest();
  return h.readUInt32BE(0) % 100;
}

/**
 * Le flag che vede un client. La riga della sua piattaforma vince su 'all';
 * una flag in rollout parziale arriva solo ai dispositivi del suo secchiello
 * (senza id di dispositivo: solo le flag al 100%).
 */
export function resolveFlags(
  rows: FlagRow[],
  platform: Platform,
  version: string,
  device: string | null,
): Record<string, FlagValue> {
  const chosen = new Map<string, FlagRow>();
  for (const r of rows) {
    if (r.platform !== 'all' && r.platform !== platform) continue;
    const prev = chosen.get(r.name);
    if (!prev || (prev.platform === 'all' && r.platform !== 'all')) chosen.set(r.name, r);
  }
  const out: Record<string, FlagValue> = {};
  for (const [name, r] of [...chosen].sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0))) {
    if (r.min_version && compareVersions(version, r.min_version) < 0) continue;
    if (r.rollout_percent < 100) {
      if (!device || rolloutBucket(name, device) >= r.rollout_percent) continue;
    }
    out[name] = r.value;
  }
  return out;
}

export interface ReleaseRow {
  version: string;
  min_supported: string;
  download_url: string;
  sha256: string;
  signature: string;
  notes: string;
}

export interface VersionInfo {
  latest: string | null;
  minSupported: string | null;
  updateAvailable: boolean;
  mustUpdate: boolean;
  url: string;
  sha256: string;
  signature: string;
  notes: string;
}

/** Cosa deve fare un client alla versione `version` con queste release. */
export function versionInfo(releases: ReleaseRow[], version: string): VersionInfo {
  let latest: ReleaseRow | null = null;
  for (const r of releases) if (!latest || compareVersions(r.version, latest.version) > 0) latest = r;
  if (!latest) {
    return { latest: null, minSupported: null, updateAvailable: false, mustUpdate: false, url: '', sha256: '', signature: '', notes: '' };
  }
  return {
    latest: latest.version,
    minSupported: latest.min_supported,
    updateAvailable: compareVersions(version, latest.version) < 0,
    mustUpdate: compareVersions(version, latest.min_supported) < 0,
    url: latest.download_url,
    sha256: latest.sha256,
    signature: latest.signature,
    notes: latest.notes,
  };
}
