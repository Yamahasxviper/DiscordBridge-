/**
 * Core data models for the Ban System.
 * These map directly to the SQLite schema and the REST API contracts.
 */

/** Platform identifiers returned by the Satisfactory DS player list */
export type Platform = "STEAM" | "EOS" | "UNKNOWN";

/**
 * A single ban record stored in the database.
 * The primary key is the compound (playerUID, platform) pair, expressed
 * as the single unique key stored in the `uid` column in the form
 * "PLATFORM:rawId" (e.g. "STEAM:76561198012345678").
 */
export interface BanRecord {
  /** Auto-incremented integer primary key */
  id: number;
  /** Unique key: "<PLATFORM>:<rawId>" (e.g. "STEAM:76561198012345678") */
  uid: string;
  /** Raw platform user identifier (Steam64 or EOS Product User ID) */
  playerUID: string;
  /** "STEAM" | "EOS" | "UNKNOWN" */
  platform: Platform;
  /** Human-readable name at time of ban (informational, may be stale) */
  playerName: string;
  /** Why this player was banned */
  reason: string;
  /** Admin username or "system" */
  bannedBy: string;
  /** ISO-8601 UTC timestamp when the ban was created */
  banDate: string;
  /**
   * ISO-8601 UTC timestamp when the ban expires.
   * NULL / empty string means the ban is permanent.
   */
  expireDate: string | null;
  /** true if this is a permanent ban (expireDate IS NULL) */
  isPermanent: boolean;
}

/** Input for creating a new ban */
export interface CreateBanInput {
  playerUID: string;
  platform: Platform;
  playerName?: string;
  reason?: string;
  bannedBy?: string;
  /** Duration in minutes. Omit or set to 0 for a permanent ban. */
  durationMinutes?: number;
}

/** Result returned when checking whether a player is currently banned */
export interface BanCheckResult {
  isBanned: boolean;
  record?: BanRecord;
  /** Human-readable message suitable for a kick reason */
  message: string;
}

/** A connected player as reported by the Satisfactory DS API */
export interface ConnectedPlayer {
  /** Platform-prefixed UID: "STEAM:xxx" or "EOS:xxx" */
  uid: string;
  playerName: string;
  platform: Platform;
  playerId: string;
}

/** Satisfactory DS server state snapshot */
export interface ServerState {
  serverName: string;
  activeSessionName: string;
  numConnectedPlayers: number;
  playerLimit: number;
  techTier: number;
  isGameRunning: boolean;
  connectedPlayers: ConnectedPlayer[];
}

/** Configuration loaded from environment variables or a config file */
export interface BanSystemConfig {
  /** Full URL of the Satisfactory DS HTTPS API, e.g. "https://127.0.0.1:7777" */
  dsApiUrl: string;
  /** Admin Bearer token obtained via PasswordLogin */
  dsAdminPassword: string;
  /** Path to the SQLite database file */
  dbPath: string;
  /** How often (in seconds) the enforcer polls the server for connected players */
  pollIntervalSeconds: number;
  /** Port for the local REST management API */
  apiPort: number;
  /** Number of database backups to retain */
  maxBackups: number;
  /** Whether to accept self-signed TLS certificates from the DS */
  allowSelfSignedTls: boolean;
}
