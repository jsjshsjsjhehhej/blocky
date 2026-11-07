-- Impostazioni del client decise dal server (come i FastFlag di Roblox) e
-- versioni pubblicate dell'app.

-- Il prefisso del nome dice il tipo del valore:
--   FFlag...   vero/falso      FInt...   numero intero      FString...  testo
CREATE TABLE fast_flags (
  name            text        NOT NULL CHECK (name ~ '^(FFlag|FInt|FString)[A-Za-z0-9_]{1,80}$'),
  -- 'all' vale ovunque; una riga 'windows' o 'android' la sostituisce lì.
  platform        text        NOT NULL DEFAULT 'all' CHECK (platform IN ('all', 'windows', 'android')),
  value           jsonb       NOT NULL,
  -- Quanti dispositivi su 100 la ricevono (sempre gli stessi, scelti dall'id).
  rollout_percent integer     NOT NULL DEFAULT 100 CHECK (rollout_percent BETWEEN 0 AND 100),
  -- Solo per i client da questa versione in su (NULL = tutti).
  min_version     text                 CHECK (min_version IS NULL OR min_version ~ '^\d+\.\d+\.\d+$'),
  description     text        NOT NULL DEFAULT '',
  updated_at      timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (name, platform)
);

CREATE TABLE client_releases (
  id            bigserial   PRIMARY KEY,
  platform      text        NOT NULL CHECK (platform IN ('windows', 'android')),
  version       text        NOT NULL CHECK (version ~ '^\d+\.\d+\.\d+$'),
  -- Sotto questa versione il client deve aggiornarsi per poter giocare.
  min_supported text        NOT NULL CHECK (min_supported ~ '^\d+\.\d+\.\d+$'),
  download_url  text        NOT NULL CHECK (download_url ~ '^https?://'),
  sha256        text        NOT NULL CHECK (sha256 ~ '^[0-9a-f]{64}$'),
  -- Firma del file (base64), verificata dal client con la chiave pubblica.
  signature     text        NOT NULL DEFAULT '',
  notes         text        NOT NULL DEFAULT '',
  published_at  timestamptz NOT NULL DEFAULT now(),
  UNIQUE (platform, version)
);
