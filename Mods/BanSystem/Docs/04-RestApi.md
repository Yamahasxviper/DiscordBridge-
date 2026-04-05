# BanSystem – REST API

← [Back to index](README.md)

BanSystem starts a local HTTP server on startup (default port **3000**) that exposes the same management endpoints as the original Tools/BanSystem Node.js service. All requests and responses use JSON.

Set `RestApiPort=0` in `DefaultBanSystem.ini` to disable the REST API entirely.

> **Security:** The REST API has no authentication. Restrict access with your server firewall so that only trusted machines can reach the port.

---

## Endpoints

### `GET /health`

Liveness probe. Returns `200 OK` when the server is running.

**Response:**
```json
{"status": "ok"}
```

---

### `GET /bans`

Returns all currently active bans (permanent + unexpired temporary).

**Response:**
```json
[
  {
    "Id": 1,
    "Uid": "EOS:00020aed06f0a6958c3c067fb4b73d51",
    "PlayerUID": "00020aed06f0a6958c3c067fb4b73d51",
    "Platform": "EOS",
    "PlayerName": "SomePlayer",
    "Reason": "Cheating",
    "BannedBy": "admin",
    "BanDate": "2025-01-01T00:00:00.000Z",
    "ExpireDate": "0001-01-01T00:00:00.000Z",
    "bIsPermanent": true,
    "LinkedUids": []
  }
]
```

---

### `GET /bans/all`

Returns all ban records, including expired temporary bans.

---

### `GET /bans/check/:uid`

Check whether a compound UID is currently banned.

**URL parameter:** `:uid` — compound UID in the format `EOS:xxx` (URL-encode the colon as `%3A`).

**Response — banned:**
```json
{"banned": true, "entry": { ... }}
```

**Response — not banned:**
```json
{"banned": false}
```

**Example:**
```sh
curl "http://localhost:3000/bans/check/EOS%3A00020aed06f0a6958c3c067fb4b73d51"
```

---

### `POST /bans`

Create a new ban.

**Request body:**
```json
{
  "playerUID":       "00020aed06f0a6958c3c067fb4b73d51",
  "platform":        "EOS",
  "playerName":      "SomePlayer",
  "reason":          "Griefing",
  "bannedBy":        "admin",
  "durationMinutes": 0
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `playerUID` | ✅ | Raw EOS Product User ID (32-char hex) |
| `platform` | ✅ | `"EOS"` or `"UNKNOWN"` |
| `playerName` | ❌ | Display name at time of ban (informational) |
| `reason` | ❌ | Ban reason text |
| `bannedBy` | ❌ | Admin username or identifier |
| `durationMinutes` | ❌ | Ban duration in minutes; `0` or omit = permanent |

**Response — success:**
```json
{"success": true, "entry": { ... }}
```

---

### `DELETE /bans/:uid`

Remove a ban by compound UID.

**URL parameter:** `:uid` — compound UID, URL-encoded.

**Response — success:**
```json
{"success": true}
```

**Response — not found:**
```json
{"success": false, "error": "Not found"}
```

**Example:**
```sh
curl -X DELETE "http://localhost:3000/bans/EOS%3A00020aed06f0a6958c3c067fb4b73d51"
```

---

### `DELETE /bans/id/:id`

Remove a ban by its integer row ID (the `Id` field from `GET /bans`).

**Example:**
```sh
curl -X DELETE "http://localhost:3000/bans/id/1"
```

---

### `POST /bans/prune`

Delete all expired temporary bans and return the count removed.

**Response:**
```json
{"pruned": 3}
```

---

### `POST /bans/backup`

Create a timestamped backup of `bans.json`. Backups beyond the `MaxBackups` limit are deleted automatically.

**Response — success:**
```json
{"success": true, "path": "/path/to/bans_2025-01-01_12-00-00.json"}
```

---

## Shell examples

```sh
# Check server health
curl http://localhost:3000/health

# List active bans
curl http://localhost:3000/bans

# Ban an EOS player permanently
curl -X POST http://localhost:3000/bans \
  -H "Content-Type: application/json" \
  -d '{"playerUID":"00020aed06f0a6958c3c067fb4b73d51","platform":"EOS","reason":"Cheating","bannedBy":"admin"}'

# Ban an EOS player for 24 hours
curl -X POST http://localhost:3000/bans \
  -H "Content-Type: application/json" \
  -d '{"playerUID":"00020aed06f0a6958c3c067fb4b73d51","platform":"EOS","reason":"Toxic","bannedBy":"admin","durationMinutes":1440}'

# Remove a ban
curl -X DELETE "http://localhost:3000/bans/EOS%3A00020aed06f0a6958c3c067fb4b73d51"

# Create a backup
curl -X POST http://localhost:3000/bans/backup
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
