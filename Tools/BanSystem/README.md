# Satisfactory Ban System

A **fully standalone** ban management service for the Satisfactory Dedicated Server.

- **Zero Unreal Engine code** – no hooks, no mods, no UE dependencies.
- **Zero native addons** – uses the built-in `node:sqlite` module (Node ≥ 22.5).
- **SQLite database** with the exact schema from the problem statement.
- **CLI tool** for manual ban management from the server shell.
- **REST API** for remote/programmatic ban management.
- **Auto-enforcer** that polls the Dedicated Server's own HTTP API and kicks any banned player that connects.

---

## Requirements

| Requirement | Version |
|-------------|---------|
| Node.js     | **≥ 22.5.0** (for built-in `node:sqlite`) |
| npm         | ≥ 8 |
| Satisfactory DS | Any version with the HTTPS management API |

---

## Installation

```bash
cd Tools/BanSystem
npm install
npm run build
```

This produces compiled JavaScript in `dist/`. There are **no native compilation steps**.

---

## Configuration

All settings are read from environment variables. Create a `.env` file next to the
binary, or export them before running:

```bash
# URL of the Satisfactory Dedicated Server management API (required for enforcer)
DS_API_URL=https://127.0.0.1:7777

# Admin password for the DS management API (required for enforcer)
DS_ADMIN_PASSWORD=your_admin_password_here

# Path to the SQLite database file (default: ./bans.db)
BAN_DB_PATH=/srv/satisfactory/bans.db

# How often (seconds) the enforcer checks for banned players online (default: 15)
POLL_INTERVAL_SECONDS=15

# Port for the local REST management API (default: 3000)
API_PORT=3000

# Number of automatic database backups to retain (default: 5)
MAX_BACKUPS=5

# Accept self-signed TLS certificates from the DS (default: true)
ALLOW_SELF_SIGNED_TLS=true
```

---

## Database Schema

The SQLite database stores exactly the fields described in the problem statement:

| Column       | Type      | Description |
|--------------|-----------|-------------|
| `id`         | INTEGER   | Auto-increment primary key |
| `uid`        | TEXT (PK) | Compound key: `"PLATFORM:rawId"` e.g. `"STEAM:76561198012345678"` |
| `playerUID`  | TEXT      | Raw platform ID (Steam64 or EOS Product User ID) |
| `platform`   | TEXT      | `"STEAM"` \| `"EOS"` \| `"UNKNOWN"` |
| `playerName` | TEXT      | Display name at time of ban (informational) |
| `reason`     | TEXT      | Why the player was banned |
| `bannedBy`   | TEXT      | Admin username or `"system"` |
| `banDate`    | TEXT      | ISO-8601 UTC timestamp when the ban was created |
| `expireDate` | TEXT/NULL | ISO-8601 UTC expiry. **NULL = permanent ban** |
| `isPermanent`| INTEGER   | `1` = permanent, `0` = temporary |

---

## CLI Usage

Run with:
```bash
node --experimental-sqlite --no-warnings=ExperimentalWarning dist/index.js <command>
```

Or add an alias to your shell:
```bash
alias banctl='node --experimental-sqlite --no-warnings=ExperimentalWarning /path/to/dist/index.js'
```

### Commands

#### `start` – Launch the enforcer daemon + REST API
```bash
banctl start
```
Starts:
- The **ban enforcer** polling loop (checks players online every `POLL_INTERVAL_SECONDS`).
- The **REST management API** on `http://127.0.0.1:API_PORT`.
- An initial **database backup** on startup.

---

#### `ban` – Permanent ban by raw ID
```bash
banctl ban <playerUID> <platform> [reason] [--name <displayName>] [--by <admin>]

# Examples
banctl ban 76561198012345678 STEAM "Griefing the factory" --name "BadPlayer" --by "Admin"
banctl ban EOSPUID00abc123   EOS   "Hacking"
```

---

#### `banid` – Permanent ban by compound UID
```bash
banctl banid <PLATFORM:rawId> [reason] [--name <displayName>] [--by <admin>]

# Example
banctl banid "STEAM:76561198012345678" "Repeated griefing"
```

---

#### `tempban` – Temporary ban
```bash
banctl tempban <playerUID> <platform> <minutes> [reason] [--name <displayName>] [--by <admin>]

# Examples
banctl tempban 76561198012345678 STEAM 60  "Spamming – 1 hour"
banctl tempban 76561198012345678 STEAM 1440 "24-hour cool-down"
```

---

#### `unban` – Remove a ban
```bash
banctl unban <PLATFORM:rawId>

# Example
banctl unban "STEAM:76561198012345678"
```
Returns exit code `0` if the ban was found and removed, `1` if it was not found.

---

#### `check` – Check ban status
```bash
banctl check <PLATFORM:rawId>

# Example
banctl check "STEAM:76561198012345678"
```
Outputs JSON. Exit code `1` = banned, `0` = not banned. Useful in scripts:
```bash
if banctl check "STEAM:76561198012345678" > /dev/null 2>&1; then
  echo "Not banned"
else
  echo "Banned"
fi
```

---

#### `list` – List bans
```bash
banctl list            # active bans only (table format)
banctl list --all      # all bans including expired
banctl list --json     # JSON output (active only)
banctl list --all --json
```

---

#### `prune` – Remove expired bans
```bash
banctl prune
```

---

#### `backup` – Create a database backup
```bash
banctl backup
banctl backup --dir /srv/backups --keep 10
```

---

## REST API

When running `banctl start`, a REST API is available at `http://127.0.0.1:3000`
(or the port set in `API_PORT`).

> **Security note:** The API binds to `127.0.0.1` (localhost) only.
> Use a reverse proxy (nginx, caddy) with authentication if remote access is needed.

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Liveness probe |
| GET | `/bans` | List active bans |
| GET | `/bans/all` | List all bans (incl. expired) |
| GET | `/bans/check/:uid` | Check if `PLATFORM:rawId` is banned |
| POST | `/bans` | Create a ban (see body below) |
| DELETE | `/bans/:uid` | Remove ban by compound UID |
| POST | `/bans/prune` | Delete expired temporary bans |
| POST | `/bans/backup` | Create database backup |

### POST `/bans` – Create a ban

```json
{
  "playerUID":       "76561198012345678",
  "platform":        "STEAM",
  "playerName":      "BadPlayer",
  "reason":          "Griefing",
  "bannedBy":        "admin",
  "durationMinutes": 0
}
```

- `playerUID` **required**
- `platform` **required** – `"STEAM"` | `"EOS"` | `"UNKNOWN"`
- `durationMinutes` optional – `0` or omitted = permanent ban; positive integer = temporary ban

### Example with curl

```bash
# Permanent ban
curl -s -X POST http://localhost:3000/bans \
  -H 'Content-Type: application/json' \
  -d '{"playerUID":"76561198012345678","platform":"STEAM","reason":"Griefing","bannedBy":"admin"}'

# Check a ban
curl -s http://localhost:3000/bans/check/STEAM:76561198012345678 | jq .

# Unban
curl -s -X DELETE "http://localhost:3000/bans/STEAM:76561198012345678"

# List all active bans
curl -s http://localhost:3000/bans | jq .
```

---

## How Ban Enforcement Works

1. The enforcer calls the DS `QueryServerState` endpoint every N seconds.
2. For each connected player it builds their compound UID (`PLATFORM:rawId`).
3. If the player appears in the ban database (active ban), it calls `KickPlayer`.
4. If `KickPlayer` fails, it falls back to `RunCommand "KickPlayer <id> <reason>"`.
5. Expired temporary bans are pruned on every polling cycle.

> **Note on player join:** Because the enforcer uses polling, a banned player may
> briefly appear in-game before being kicked. The DS management API does not
> currently expose a pre-login hook for external tools. For instant enforcement,
> reduce `POLL_INTERVAL_SECONDS` (e.g. `5`).

---

## Backup & Recovery

- On `banctl start`, a backup is automatically created.
- Backups are stored in `<dbdir>/backups/bans_YYYY-MM-DD_HH-MM-SS.db`.
- The `MAX_BACKUPS` setting controls how many are retained (default: 5).
- To restore: stop the service, replace `bans.db` with the backup file, restart.

---

## Running as a systemd service (Linux)

```ini
# /etc/systemd/system/satisfactory-ban.service
[Unit]
Description=Satisfactory Ban System
After=network.target

[Service]
Type=simple
User=satisfactory
WorkingDirectory=/srv/satisfactory/ban-system
EnvironmentFile=/srv/satisfactory/ban-system/.env
ExecStart=/usr/bin/node --experimental-sqlite --no-warnings=ExperimentalWarning dist/index.js start
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
systemctl daemon-reload
systemctl enable --now satisfactory-ban.service
systemctl status satisfactory-ban.service
