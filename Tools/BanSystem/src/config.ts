/**
 * Configuration loader.
 * Values are read from environment variables with sensible defaults.
 * For production use, place a `.env` file next to the binary or export
 * the variables in the shell that launches the service.
 */

import * as fs from "node:fs";
import * as path from "node:path";
import type { BanSystemConfig } from "./models.js";

/** Load and validate configuration from environment variables. */
export function loadConfig(): BanSystemConfig {
  // Optional: load a .env file from the working directory
  const envFile = path.join(process.cwd(), ".env");
  if (fs.existsSync(envFile)) {
    const lines = fs.readFileSync(envFile, "utf-8").split("\n");
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith("#")) continue;
      const eqIdx = trimmed.indexOf("=");
      if (eqIdx === -1) continue;
      const key = trimmed.slice(0, eqIdx).trim();
      const value = trimmed.slice(eqIdx + 1).trim().replace(/^["']|["']$/g, "");
      if (key && !(key in process.env)) {
        process.env[key] = value;
      }
    }
  }

  const dsApiUrl = process.env["DS_API_URL"] ?? "https://127.0.0.1:7777";
  const dsAdminPassword = process.env["DS_ADMIN_PASSWORD"] ?? "";
  const dbPath = process.env["BAN_DB_PATH"] ?? path.join(process.cwd(), "bans.db");
  const pollIntervalSeconds = parseInt(process.env["POLL_INTERVAL_SECONDS"] ?? "15", 10);
  const apiPort = parseInt(process.env["API_PORT"] ?? "3000", 10);
  const maxBackups = parseInt(process.env["MAX_BACKUPS"] ?? "5", 10);
  const allowSelfSignedTls = (process.env["ALLOW_SELF_SIGNED_TLS"] ?? "true") === "true";

  if (!dsAdminPassword) {
    process.stderr.write(
      "[config] DS_ADMIN_PASSWORD is not set. DS API calls that require authentication will fail.\n"
    );
  }

  return {
    dsApiUrl,
    dsAdminPassword,
    dbPath,
    pollIntervalSeconds,
    apiPort,
    maxBackups,
    allowSelfSignedTls,
  };
}

/** Singleton configuration instance (loaded once on first call) */
let _config: BanSystemConfig | null = null;
export function getConfig(): BanSystemConfig {
  if (!_config) _config = loadConfig();
  return _config;
}
