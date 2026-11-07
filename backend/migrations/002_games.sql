-- Catalogo dei giochi pubblicati (come i giochi di Roblox): chi pubblica,
-- la scheda del gioco e i suoi file. I client lo leggono con GET /games, gli
-- stessi percorsi che usava il PC in rete locale.

-- Chi puo' pubblicare. Del token si tiene solo l'hash: se il database finisce
-- in mani sbagliate, i token restano segreti.
CREATE TABLE publishers (
  id           bigserial   PRIMARY KEY,
  name         text        NOT NULL UNIQUE CHECK (name ~ '^[A-Za-z0-9_]{3,24}$'),
  token_sha256 text        NOT NULL UNIQUE CHECK (token_sha256 ~ '^[0-9a-f]{64}$'),
  banned       boolean     NOT NULL DEFAULT false,
  created_at   timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE games (
  id             text        PRIMARY KEY CHECK (id ~ '^[a-z0-9_]{1,48}$'),
  owner_id       bigint      NOT NULL REFERENCES publishers(id),
  title          text        NOT NULL CHECK (length(title) BETWEEN 1 AND 60),
  description    text        NOT NULL DEFAULT '' CHECK (length(description) <= 500),
  max_players    integer     NOT NULL DEFAULT 8 CHECK (max_players BETWEEN 1 AND 32),
  -- Sale a ogni pubblicazione: i client capiscono che c'e' una versione nuova.
  version        integer     NOT NULL DEFAULT 1 CHECK (version >= 1),
  place_file     text        NOT NULL,
  thumbnail_file text        NOT NULL DEFAULT '',
  total_bytes    bigint      NOT NULL DEFAULT 0,
  -- Falso = tolto dal catalogo (dal creatore o da un admin), file conservati.
  listed         boolean     NOT NULL DEFAULT true,
  published_at   timestamptz NOT NULL DEFAULT now(),
  updated_at     timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX games_listed_updated ON games (listed, updated_at DESC);

-- I file della versione attuale. Dentro il database: un solo servizio da
-- mettere online, niente archivio di file a parte (i giochi Blocky sono
-- piccoli: il limite e' 30 MB per gioco).
CREATE TABLE game_files (
  game_id text    NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  path    text    NOT NULL CHECK (length(path) BETWEEN 1 AND 200),
  size    integer NOT NULL CHECK (size >= 0),
  sha256  text    NOT NULL CHECK (sha256 ~ '^[0-9a-f]{64}$'),
  data    bytea   NOT NULL,
  PRIMARY KEY (game_id, path)
);
