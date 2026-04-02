/**
 * BanService – the core business logic layer.
 *
 * Provides all ban/unban/check/list operations on top of BanDatabase.
 * This layer is intentionally free of Express or CLI concerns so it
 * can be consumed equally by the REST API and the CLI.
 */

import { BanDatabase } from "./database.js";
import type {
  BanCheckResult,
  BanRecord,
  CreateBanInput,
  Platform,
} from "./models.js";

export class BanService {
  constructor(private readonly db: BanDatabase) {}

  // ------------------------------------------------------------------
  // Ban / unban
  // ------------------------------------------------------------------

  /**
   * Permanently ban a player.
   * If a ban already exists for this UID it is replaced.
   */
  ban(
    playerUID: string,
    platform: Platform,
    options: { playerName?: string; reason?: string; bannedBy?: string } = {}
  ): BanRecord {
    const input: CreateBanInput = {
      playerUID,
      platform,
      playerName: options.playerName ?? "",
      reason: options.reason ?? "No reason given",
      bannedBy: options.bannedBy ?? "admin",
      durationMinutes: 0, // permanent
    };
    const record = this.db.addBan(input);
    console.log(
      `[ban] Permanently banned ${platform}:${playerUID} (${options.playerName ?? "?"}) – ${input.reason}`
    );
    return record;
  }

  /**
   * Temporarily ban a player for `durationMinutes` minutes.
   * If a ban already exists it is replaced.
   */
  tempBan(
    playerUID: string,
    platform: Platform,
    durationMinutes: number,
    options: { playerName?: string; reason?: string; bannedBy?: string } = {}
  ): BanRecord {
    if (durationMinutes <= 0) throw new Error("durationMinutes must be positive");
    const input: CreateBanInput = {
      playerUID,
      platform,
      playerName: options.playerName ?? "",
      reason: options.reason ?? "Temporarily banned",
      bannedBy: options.bannedBy ?? "admin",
      durationMinutes,
    };
    const record = this.db.addBan(input);
    const expires = new Date(record.expireDate!).toLocaleString();
    console.log(
      `[ban] Temp-banned ${platform}:${playerUID} for ${durationMinutes} min (expires ${expires})`
    );
    return record;
  }

  /**
   * Unban a player by their compound UID ("PLATFORM:rawId").
   * Returns true if a ban was found and removed.
   */
  unban(uid: string): boolean {
    const existed = this.db.removeBan(uid);
    if (existed) {
      console.log(`[ban] Unbanned ${uid}`);
    } else {
      console.log(`[ban] No active ban found for ${uid}`);
    }
    return existed;
  }

  // ------------------------------------------------------------------
  // Queries
  // ------------------------------------------------------------------

  /**
   * Check whether a player is currently banned.
   * Expired temporary bans return `isBanned: false`.
   */
  checkBan(uid: string): BanCheckResult {
    const record = this.db.isCurrentlyBanned(uid);
    if (!record) {
      return { isBanned: false, message: "Player is not banned." };
    }

    let message: string;
    if (record.isPermanent) {
      message = `Banned permanently. Reason: ${record.reason}`;
    } else {
      const exp = new Date(record.expireDate!).toUTCString();
      message = `Banned until ${exp}. Reason: ${record.reason}`;
    }

    return { isBanned: true, record, message };
  }

  /** Build the kick message shown to the banned player. */
  buildKickMessage(record: BanRecord): string {
    if (record.isPermanent) {
      return `You have been permanently banned. Reason: ${record.reason}`;
    }
    const exp = new Date(record.expireDate!).toUTCString();
    return `You are temporarily banned until ${exp}. Reason: ${record.reason}`;
  }

  /** Return all currently-active bans. */
  listActiveBans(): BanRecord[] {
    return this.db.getActiveBans();
  }

  /** Return all bans including expired ones. */
  listAllBans(): BanRecord[] {
    return this.db.getAllBans();
  }

  // ------------------------------------------------------------------
  // Maintenance
  // ------------------------------------------------------------------

  /** Remove expired temporary bans and return the count removed. */
  pruneExpired(): number {
    const count = this.db.pruneExpiredBans();
    if (count > 0) {
      console.log(`[ban] Pruned ${count} expired ban(s)`);
    }
    return count;
  }

  /**
   * Create a backup of the database.
   * Returns a Promise resolving to the path of the backup file.
   */
  async backup(backupDir: string, maxKeep = 5): Promise<string> {
    const dest = await this.db.backup(backupDir, maxKeep);
    console.log(`[ban] Database backed up to ${dest}`);
    return dest;
  }
}
