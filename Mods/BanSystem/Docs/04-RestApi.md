# BanSystem – REST API

← [Back to index](README.md)

BanSystem starts a local HTTP server on startup (default port **3000**) that exposes the same management endpoints as the original Tools/BanSystem Node.js service. All requests and responses use JSON.

Set `RestApiPort=0` in `DefaultBanSystem.ini` to disable the REST API entirely.

> **Security:** Restrict external access with your server firewall so that only
> trusted machines can reach the port.
>
> Optionally set `RestApiKey` in `DefaultBanSystem.ini` to require an API key
> on all mutating endpoints (`POST`, `DELETE`, `PATCH`). Include the key in the
> `X-Api-Key` request header:
> ```
> X-Api-Key: mysecretkey
> ```
> Read-only `GET` endpoints are never gated.

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
| `playerUID` | ✅ | Raw EOS Product User ID (32-char hex), **or** an IP address when `platform` is `"IP"` |
| `platform` | ✅ | `"EOS"`, `"UNKNOWN"`, or `"IP"` |
| `playerName` | ❌ | Display name at time of ban (informational) |
| `reason` | ❌ | Ban reason text |
| `bannedBy` | ❌ | Admin username or identifier |
| `durationMinutes` | ❌ | Ban duration in minutes; `0` or omit = permanent |

**Response — success:**
```json
{"success": true, "entry": { ... }}
```

**IP ban example:**
```sh
curl -X POST http://localhost:3000/bans \
  -H "Content-Type: application/json" \
  -d '{"playerUID":"1.2.3.4","platform":"IP","reason":"VPN evader","bannedBy":"admin"}'
```

---

### `PATCH /bans/:uid`

Update an existing ban. Only the fields you include are changed; omitted fields
keep their current values.

**URL parameter:** `:uid` — compound UID, URL-encoded.

**Request body (all fields optional):**
```json
{
  "reason":          "Updated reason",
  "bIsPermanent":    true,
  "durationMinutes": 0
}
```

| Field | Description |
|-------|-------------|
| `reason` | New ban reason text |
| `bIsPermanent` | Set to `true` to convert a temp ban to permanent |
| `durationMinutes` | New duration in minutes (ignored when `bIsPermanent` is true; `0` when both are absent = keep existing duration) |

**Response — success:**
```json
{"success": true, "entry": { ... }}
```

**Example — extend a ban to 48 hours:**
```sh
curl -X PATCH "http://localhost:3000/bans/EOS%3A00020aed06f0a6958c3c067fb4b73d51" \
  -H "Content-Type: application/json" \
  -d '{"durationMinutes":2880}'
```

**Example — convert to permanent:**
```sh
curl -X PATCH "http://localhost:3000/bans/EOS%3A00020aed06f0a6958c3c067fb4b73d51" \
  -H "Content-Type: application/json" \
  -d '{"bIsPermanent":true}'
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

# Ban an IP address permanently
curl -X POST http://localhost:3000/bans \
  -H "Content-Type: application/json" \
  -d '{"playerUID":"1.2.3.4","platform":"IP","reason":"VPN evader","bannedBy":"admin"}'

# Ban an IP address for 24 hours
curl -X POST http://localhost:3000/bans \
  -H "Content-Type: application/json" \
  -d '{"playerUID":"1.2.3.4","platform":"IP","reason":"Suspicious traffic","bannedBy":"admin","durationMinutes":1440}'

# Check if an IP is banned
curl "http://localhost:3000/bans/check/IP%3A1.2.3.4"

# Remove an EOS ban
curl -X DELETE "http://localhost:3000/bans/EOS%3A00020aed06f0a6958c3c067fb4b73d51"

# Remove an IP ban
curl -X DELETE "http://localhost:3000/bans/IP%3A1.2.3.4"

# Create a backup
curl -X POST http://localhost:3000/bans/backup
```

---

## Additional endpoints (v1.1.0)

The following endpoints were added in v1.1.0:

### Player sessions

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/players` | List all player sessions |
| `GET` | `/players/search?name=<query>` | Search players by name |
| `GET` | `/players/export-csv` | Export players as CSV |
| `POST` | `/players/prune` | Prune old session records |

### Warnings

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/warnings` | List all warnings (optional `?uid=UID` filter) |
| `GET` | `/warnings/export-csv` | Export warnings as CSV |
| `POST` | `/warnings` | Issue a warning |
| `DELETE` | `/warnings/:uid` | Clear all warnings for a UID |
| `DELETE` | `/warnings/id/:id` | Remove a single warning by ID |

### Audit log

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/audit` | View audit log |
| `GET` | `/audit/export-csv` | Export audit log as CSV |

### Analytics & metrics

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/metrics` | Server statistics JSON |
| `GET` | `/metrics/prometheus` | Prometheus text format metrics |
| `GET` | `/reputation/:uid` | Player reputation score |

### Ban appeals

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/appeals` | Submit a ban appeal |
| `GET` | `/appeals` | List all appeals |
| `GET` | `/appeals/:id` | Get a single appeal |
| `DELETE` | `/appeals/:id` | Dismiss an appeal |
| `GET` | `/appeals/portal` | Self-service HTML appeals form |

### Scheduled bans

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/scheduled` | List scheduled bans |
| `POST` | `/scheduled` | Schedule a future ban |
| `DELETE` | `/scheduled/:id` | Delete a scheduled ban |

### Admin tools

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/dashboard` | Unified admin dashboard SPA |
| `GET` | `/bans/search?name=<query>` | Search bans by name |
| `GET` | `/bans/export-csv` | Export bans as CSV |
| `POST` | `/bans/ip` | Create an IP ban |
| `DELETE` | `/bans/ip/:ip` | Remove an IP ban |
| `POST` | `/bans/bulk` | Bulk ban operations |
| `POST` | `/notes` | Add admin notes |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
