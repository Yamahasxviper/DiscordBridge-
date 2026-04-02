/**
 * SQLite database layer using the Node.js built-in `node:sqlite` module.
 * Requires Node >=22.5.0 with --experimental-sqlite flag (Node <24) or
 * Node >=24 where it is stable.
 *
 * No native add-ons, no npm dependencies.
 *
 * Schema (mirrors the problem statement):
 *   PlayerUID  TEXT   – raw platform user ID (Steam64 or EOS PUID)
 *   Platform   TEXT   – "STEAM" | "EOS" | "UNKNOWN"
 *   Reason     TEXT
 *   BannedBy   TEXT
 *   BanDate    TEXT   – ISO-8601 UTC
 *   ExpireDate TEXT   – ISO-8601 UTC, NULL for permanent bans
 */

/* eslint-disable @typescript-eslint/no-require-imports */
const { DatabaseSync, backup: sqliteBackup } =
  require("node:sqlite") as typeof import("node:sqlite");
/* eslint-enable */

import * as fs from "node:fs";
import * as path from "node:path";
import type { BanRecord, CreateBanInput, Platform } from "./models.js";

// ── Type alias for the node:sqlite DB instance ──────────────────────────────
type DB = InstanceType<typeof DatabaseSync>;

// ── Internal row shape returned by node:sqlite ───────────────────────────────
interface RawRow {
  id: bigint | number;
  uid: string;
  playerUID: string;
  platform: string;
  playerName: string;
  reason: string;
  bannedBy: string;
  banDate: string;
  expireDate: string | null;
  isPermanent: bigint | number;
}

// ── Schema ───────────────────────────────────────────────────────────────────
const SCHEMA = `
PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS bans (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  uid         TEXT    NOT NULL UNIQUE,
  playerUID   TEXT    NOT NULL,
  platform    TEXT    NOT NULL,
  playerName  TEXT    NOT NULL DEFAULT '',
  reason      TEXT    NOT NULL DEFAULT 'No reason given',
  bannedBy    TEXT    NOT NULL DEFAULT 'system',
  banDate     TEXT    NOT NULL,
  expireDate  TEXT,
  isPermanent INTEGER NOT NULL DEFAULT 1
);

CREATE INDEX IF NOT EXISTS idx_bans_uid      ON bans(uid);
CREATE INDEX IF NOT EXISTS idx_bans_platform ON bans(platform);
CREATE INDEX IF NOT EXISTS idx_bans_expire   ON bans(expireDate);
`;

// ── BanDatabase ───────────────────────────────────────────────────────────────
export class BanDatabase {
  private readonly db: DB;
  private readonly dbPath: string;

  constructor(dbPath: string) {
    const dir = path.dirname(dbPath);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
    this.dbPath = dbPath;
    this.db = new DatabaseSync(dbPath);
    this.db.exec(SCHEMA);
  }

  // ── Helpers ────────────────────────────────────────────────────────────────

  /** Build compound UID key "PLATFORM:rawId" */
  static makeUid(platform: Platform, playerUID: string): string {
    return `${platform}:${playerUID}`;
  }

  /** Parse compound UID key back to its parts */
  static parseUid(uid: string): { platform: Platform; playerUID: string } {
    const colon = uid.indexOf(":");
    if (colon === -1) return { platform: "UNKNOWN", playerUID: uid };
    const platform = uid.slice(0, colon).toUpperCase() as Platform;
    const playerUID = uid.slice(colon + 1);
    return { platform, playerUID };
  }

  // ── Write ──────────────────────────────────────────────────────────────────

  /**
   * Insert or replace a ban record (upsert on uid).
   * Returns the fresh record from the DB.
   */
  addBan(input: CreateBanInput): BanRecord {
    const platform: Platform = input.platform ?? "UNKNOWN";
    const uid = BanDatabase.makeUid(platform, input.playerUID);
    const now = new Date().toISOString();
    const permanent = !input.durationMinutes || input.durationMinutes <= 0;
    const expireDate = permanent
      ? null
      : new Date(Date.now() + input.durationMinutes! * 60_000).toISOString();

    const stmt = this.db.prepare(`
      INSERT INTO bans
        (uid, playerUID, platform, playerName, reason, bannedBy, banDate, expireDate, isPermanent)
      VALUES
        (?, ?, ?, ?, ?, ?, ?, ?, ?)
      ON CONFLICT(uid) DO UPDATE SET
        playerName  = excluded.playerName,
        reason      = excluded.reason,
        bannedBy    = excluded.bannedBy,
        banDate     = excluded.banDate,
        expireDate  = excluded.expireDate,
        isPermanent = excluded.isPermanent
    `);

    stmt.run(
      uid,
      input.playerUID,
      platform,
      input.playerName ?? "",
      input.reason ?? "No reason given",
      input.bannedBy ?? "system",
      now,
      expireDate,
      permanent ? 1 : 0
    );

    return this.getBanByUid(uid)!;
  }

  /** Remove a ban by compound UID. Returns true if a row was deleted. */
  removeBan(uid: string): boolean {
    const stmt = this.db.prepare("DELETE FROM bans WHERE uid = ?");
    const result = stmt.run(uid) as { changes: number };
    return result.changes > 0;
  }

  /** Remove a ban by integer id. Returns true if a row was deleted. */
  removeBanById(id: number): boolean {
    const stmt = this.db.prepare("DELETE FROM bans WHERE id = ?");
    const result = stmt.run(id) as { changes: number };
    return result.changes > 0;
  }

  /** Delete all expired temporary bans. Returns count removed. */
  pruneExpiredBans(): number {
    const now = new Date().toISOString();
    const stmt = this.db.prepare(
      "DELETE FROM bans WHERE isPermanent = 0 AND expireDate IS NOT NULL AND expireDate <= ?"
    );
    const result = stmt.run(now) as { changes: number };
    return result.changes;
  }

  // ── Read ───────────────────────────────────────────────────────────────────

  /** Fetch a ban by compound UID (regardless of expiry). */
  getBanByUid(uid: string): BanRecord | undefined {
    const row = this.db.prepare("SELECT * FROM bans WHERE uid = ?").get(uid) as
      | RawRow
      | undefined;
    return row ? toRecord(row) : undefined;
  }

  /**
   * Return the ban record if the player is currently banned
   * (permanent or expiry in the future). Returns undefined if not banned.
   */
  isCurrentlyBanned(uid: string): BanRecord | undefined {
    const now = new Date().toISOString();
    const row = this.db
      .prepare(
        `SELECT * FROM bans
         WHERE uid = ?
           AND (isPermanent = 1
                OR (expireDate IS NOT NULL AND expireDate > ?))`
      )
      .get(uid, now) as RawRow | undefined;
    return row ? toRecord(row) : undefined;
  }

  /** All currently-active bans (permanent + unexpired temporary). */
  getActiveBans(): BanRecord[] {
    const now = new Date().toISOString();
    const rows = this.db
      .prepare(
        `SELECT * FROM bans
         WHERE isPermanent = 1
            OR (expireDate IS NOT NULL AND expireDate > ?)
         ORDER BY banDate DESC`
      )
      .all(now) as unknown as RawRow[];
    return rows.map(toRecord);
  }

  /** All ban rows including expired ones. */
  getAllBans(): BanRecord[] {
    const rows = this.db
      .prepare("SELECT * FROM bans ORDER BY banDate DESC")
      .all() as unknown as RawRow[];
    return rows.map(toRecord);
  }

  // ── Backup ─────────────────────────────────────────────────────────────────

  /**
   * Copy the live database to `<backupDir>/bans_YYYY-MM-DD_HH-MM-SS.db`.
   * Uses the built-in `node:sqlite` async backup function which does a safe
   * online backup while the DB is open and being written.
   * Returns the destination path (resolved after the async copy completes).
   */
  async backup(backupDir: string, maxKeep = 5): Promise<string> {
    if (!fs.existsSync(backupDir)) {
      fs.mkdirSync(backupDir, { recursive: true });
    }

    const stamp = new Date()
      .toISOString()
      .replace(/[:.]/g, "-")
      .replace("T", "_")
      .slice(0, 19);
    const dest = path.join(backupDir, `bans_${stamp}.db`);

    await sqliteBackup(this.db, dest, {});

    // Prune oldest backups beyond maxKeep
    const files = fs
      .readdirSync(backupDir)
      .filter((f) => f.startsWith("bans_") && f.endsWith(".db"))
      .sort();
    while (files.length > maxKeep) {
      fs.rmSync(path.join(backupDir, files.shift()!), { force: true });
    }

    return dest;
  }

  /** Close the underlying database connection. */
  close(): void {
    this.db.close();
  }
}

// ── Private helper ────────────────────────────────────────────────────────────
function toRecord(row: RawRow): BanRecord {
  return {
    id: Number(row.id),
    uid: row.uid,
    playerUID: row.playerUID,
    platform: row.platform as Platform,
    playerName: row.playerName,
    reason: row.reason,
    bannedBy: row.bannedBy,
    banDate: row.banDate,
    expireDate: row.expireDate,
    isPermanent: Number(row.isPermanent) === 1,
  };
}
