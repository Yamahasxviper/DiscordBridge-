#!/usr/bin/env node
/**
 * CLI entry point for the Satisfactory Ban System.
 *
 * Usage examples:
 *   banctl start                                    – start daemon (enforcer + REST API)
 *   banctl ban 76561198012345678 STEAM "Griefing"   – permanent ban
 *   banctl tempban 76561198012345678 STEAM 60 "AFK" – temp-ban 60 minutes
 *   banctl banid STEAM:76561198012345678 "Griefing"  – ban by compound UID
 *   banctl unban STEAM:76561198012345678             – remove ban
 *   banctl check STEAM:76561198012345678             – check ban status
 *   banctl list                                     – list active bans
 *   banctl list --all                               – list all bans incl. expired
 *   banctl prune                                    – delete expired bans
 *   banctl backup                                   – backup the database
 */

import { Command } from "commander";
import * as path from "node:path";
import * as os from "node:os";

import { getConfig } from "./config.js";
import { BanDatabase } from "./database.js";
import { BanService } from "./banService.js";
import { DSApiClient } from "./dsApiClient.js";
import { Enforcer } from "./enforcer.js";
import { createApiServer } from "./apiServer.js";
import type { Platform, BanRecord } from "./models.js";

const program = new Command();

program
  .name("banctl")
  .description("Satisfactory Dedicated Server – standalone ban management system")
  .version("1.0.0");

// ------------------------------------------------------------------
// Helper – build service stack from config
// ------------------------------------------------------------------
function buildStack(): { db: BanDatabase; svc: BanService; cfg: ReturnType<typeof getConfig> } {
  const cfg = getConfig();
  const db = new BanDatabase(cfg.dbPath);
  const svc = new BanService(db);
  return { db, svc, cfg };
}

function defaultBackupDir(dbPath: string): string {
  return path.join(path.dirname(dbPath), "backups");
}

function normalisePlatform(raw: string): Platform {
  const up = raw.toUpperCase() as Platform;
  if (up === "STEAM" || up === "EOS" || up === "UNKNOWN") return up;
  throw new Error(`Unknown platform "${raw}". Expected STEAM, EOS or UNKNOWN.`);
}

// ------------------------------------------------------------------
// Command: start – daemon mode (enforcer + REST API)
// ------------------------------------------------------------------
program
  .command("start")
  .description("Start the ban enforcer daemon and the REST management API")
  .action(async () => {
    const { db, svc, cfg } = buildStack();

    // Initial backup on startup
    const backupDir = defaultBackupDir(cfg.dbPath);
    svc.backup(backupDir, cfg.maxBackups).catch((e: Error) =>
      console.warn("[main] Startup backup failed:", e.message)
    );

    // REST API
    const app = createApiServer(svc, backupDir, cfg.maxBackups);
    const server = app.listen(cfg.apiPort, "127.0.0.1", () => {
      console.log(`[api] REST management API listening on http://127.0.0.1:${cfg.apiPort}`);
    });

    // Enforcer
    const dsClient = new DSApiClient(cfg);
    const enforcer = new Enforcer(cfg, dsClient, svc);
    enforcer.start();

    // Graceful shutdown
    const shutdown = (): void => {
      console.log("\n[main] Shutting down…");
      enforcer.stop();
      server.close(() => {
        db.close();
        process.exit(0);
      });
    };
    process.on("SIGINT", shutdown);
    process.on("SIGTERM", shutdown);
  });

// ------------------------------------------------------------------
// Command: ban – permanent ban by raw ID + platform
// ------------------------------------------------------------------
program
  .command("ban <playerUID> <platform> [reason]")
  .description("Permanently ban a player by raw ID and platform")
  .option("--name <name>", "Player display name (informational)")
  .option("--by <admin>", "Banned by (admin identifier)", "admin")
  .action((playerUID: string, platform: string, reason: string | undefined, opts) => {
    const { svc } = buildStack();
    const record = svc.ban(playerUID, normalisePlatform(platform), {
      playerName: opts.name,
      reason,
      bannedBy: opts.by,
    });
    console.log("Banned:", formatRecord(record));
  });

// ------------------------------------------------------------------
// Command: banid – permanent ban by compound UID ("PLATFORM:rawId")
// ------------------------------------------------------------------
program
  .command("banid <uid> [reason]")
  .description('Permanently ban a player by compound UID e.g. "STEAM:76561198012345678"')
  .option("--name <name>", "Player display name (informational)")
  .option("--by <admin>", "Banned by (admin identifier)", "admin")
  .action((uid: string, reason: string | undefined, opts) => {
    const { svc } = buildStack();
    const { platform, playerUID } = BanDatabase.parseUid(uid);
    const record = svc.ban(playerUID, platform, {
      playerName: opts.name,
      reason,
      bannedBy: opts.by,
    });
    console.log("Banned:", formatRecord(record));
  });

// ------------------------------------------------------------------
// Command: tempban – temporary ban by raw ID + platform
// ------------------------------------------------------------------
program
  .command("tempban <playerUID> <platform> <minutes> [reason]")
  .description("Temporarily ban a player for the given number of minutes")
  .option("--name <name>", "Player display name (informational)")
  .option("--by <admin>", "Banned by (admin identifier)", "admin")
  .action((playerUID: string, platform: string, minutes: string, reason: string | undefined, opts) => {
    const { svc } = buildStack();
    const mins = parseInt(minutes, 10);
    if (isNaN(mins) || mins <= 0) {
      console.error("minutes must be a positive integer");
      process.exit(1);
    }
    const record = svc.tempBan(playerUID, normalisePlatform(platform), mins, {
      playerName: opts.name,
      reason,
      bannedBy: opts.by,
    });
    console.log("Temp-banned:", formatRecord(record));
  });

// ------------------------------------------------------------------
// Command: unban
// ------------------------------------------------------------------
program
  .command("unban <uid>")
  .description('Remove a ban by compound UID e.g. "STEAM:76561198012345678"')
  .action((uid: string) => {
    const { svc } = buildStack();
    const removed = svc.unban(uid);
    process.exit(removed ? 0 : 1);
  });

// ------------------------------------------------------------------
// Command: check
// ------------------------------------------------------------------
program
  .command("check <uid>")
  .description("Check whether a player is currently banned")
  .action((uid: string) => {
    const { svc } = buildStack();
    const result = svc.checkBan(uid);
    console.log(JSON.stringify(result, null, 2));
    process.exit(result.isBanned ? 1 : 0);
  });

// ------------------------------------------------------------------
// Command: list
// ------------------------------------------------------------------
program
  .command("list")
  .description("List bans (active only by default)")
  .option("--all", "Include expired bans")
  .option("--json", "Output as JSON")
  .action((opts) => {
    const { svc } = buildStack();
    const bans = opts.all ? svc.listAllBans() : svc.listActiveBans();
    if (opts.json) {
      console.log(JSON.stringify(bans, null, 2));
    } else {
      if (bans.length === 0) {
        console.log("No bans found.");
        return;
      }
      console.log(`${"ID".padEnd(5)} ${"UID".padEnd(50)} ${"Reason".padEnd(30)} ${"BanDate".padEnd(24)} ${"Expires"}`);
      console.log("-".repeat(140));
      for (const b of bans) {
        const expires = b.isPermanent ? "PERMANENT" : (b.expireDate ?? "?");
        console.log(
          `${String(b.id).padEnd(5)} ${b.uid.padEnd(50)} ${b.reason.slice(0, 28).padEnd(30)} ${b.banDate.padEnd(24)} ${expires}`
        );
      }
    }
  });

// ------------------------------------------------------------------
// Command: prune
// ------------------------------------------------------------------
program
  .command("prune")
  .description("Delete all expired temporary bans from the database")
  .action(() => {
    const { svc } = buildStack();
    const count = svc.pruneExpired();
    console.log(`Pruned ${count} expired ban(s).`);
  });

// ------------------------------------------------------------------
// Command: backup
// ------------------------------------------------------------------
program
  .command("backup")
  .description("Create a timestamped backup of the ban database")
  .option("--dir <directory>", "Backup directory")
  .option("--keep <n>", "Number of backups to keep", "5")
  .action(async (opts) => {
    const { svc, cfg } = buildStack();
    const backupDir = opts.dir ?? defaultBackupDir(cfg.dbPath);
    const keep = parseInt(opts.keep, 10);
    const dest = await svc.backup(backupDir, keep);
    console.log(`Backup written to: ${dest}`);
  });

// ------------------------------------------------------------------
// Parse
// ------------------------------------------------------------------
program.parseAsync(process.argv).catch((err: unknown) => {
  console.error("Fatal error:", err);
  process.exit(1);
});

// ------------------------------------------------------------------
// Formatting helper
// ------------------------------------------------------------------
function formatRecord(r: BanRecord): string {
  const expires = r.isPermanent ? "PERMANENT" : `until ${r.expireDate}`;
  return `[id=${r.id}] ${r.uid} | reason="${r.reason}" | ${expires}`;
}
