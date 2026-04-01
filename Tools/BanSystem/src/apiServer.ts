/**
 * REST API server.
 *
 * Provides a local HTTP management API for the ban system.
 * All endpoints are JSON. No external dependencies beyond Express.
 *
 * Routes
 * ──────
 *  GET    /health                       – liveness probe
 *  GET    /bans                         – list active bans
 *  GET    /bans/all                     – list all bans (incl. expired)
 *  GET    /bans/check/:uid              – check if a UID is currently banned
 *  POST   /bans                         – create a ban
 *  DELETE /bans/:uid                    – remove a ban by compound UID
 *  DELETE /bans/id/:id                  – remove a ban by integer id
 *  POST   /bans/prune                   – prune expired bans
 *  POST   /bans/backup                  – create a database backup
 *
 * POST /bans body (all fields optional except playerUID and platform):
 *   {
 *     "playerUID":       "76561198012345678",
 *     "platform":        "STEAM",
 *     "playerName":      "PlayerXYZ",
 *     "reason":          "Griefing",
 *     "bannedBy":        "admin",
 *     "durationMinutes": 60        // omit or 0 for permanent
 *   }
 */

import express, { Request, Response, NextFunction } from "express";
import type { BanService } from "./banService.js";
import type { CreateBanInput, Platform } from "./models.js";
import * as path from "node:path";

export function createApiServer(
  banService: BanService,
  backupDir: string,
  maxBackups: number
): express.Express {
  const app = express();
  app.use(express.json());

  // ------------------------------------------------------------------
  // Health
  // ------------------------------------------------------------------
  app.get("/health", (_req: Request, res: Response) => {
    res.json({ status: "ok", timestamp: new Date().toISOString() });
  });

  // ------------------------------------------------------------------
  // List bans
  // ------------------------------------------------------------------
  app.get("/bans", (_req: Request, res: Response) => {
    const bans = banService.listActiveBans();
    res.json({ count: bans.length, bans });
  });

  app.get("/bans/all", (_req: Request, res: Response) => {
    const bans = banService.listAllBans();
    res.json({ count: bans.length, bans });
  });

  // ------------------------------------------------------------------
  // Check a ban  (must be before /bans/:uid to avoid route collision)
  // ------------------------------------------------------------------
  app.get("/bans/check/:uid", (req: Request, res: Response) => {
    const uid = decodeURIComponent(req.params["uid"] ?? "");
    const result = banService.checkBan(uid);
    res.json(result);
  });

  // ------------------------------------------------------------------
  // Create a ban
  // ------------------------------------------------------------------
  app.post("/bans", (req: Request, res: Response) => {
    const body = req.body as Partial<CreateBanInput>;

    if (!body.playerUID) {
      res.status(400).json({ error: "playerUID is required" });
      return;
    }
    if (!body.platform) {
      res.status(400).json({ error: "platform is required (STEAM | EOS | UNKNOWN)" });
      return;
    }

    const platform = (body.platform as string).toUpperCase() as Platform;
    if (!["STEAM", "EOS", "UNKNOWN"].includes(platform)) {
      res.status(400).json({ error: "platform must be STEAM, EOS or UNKNOWN" });
      return;
    }

    const durationMinutes = Number(body.durationMinutes ?? 0);

    let record;
    if (durationMinutes > 0) {
      record = banService.tempBan(body.playerUID, platform, durationMinutes, {
        playerName: body.playerName,
        reason: body.reason,
        bannedBy: body.bannedBy,
      });
    } else {
      record = banService.ban(body.playerUID, platform, {
        playerName: body.playerName,
        reason: body.reason,
        bannedBy: body.bannedBy,
      });
    }

    res.status(201).json(record);
  });

  // ------------------------------------------------------------------
  // Remove a ban by compound UID
  // ------------------------------------------------------------------
  app.delete("/bans/:uid", (req: Request, res: Response) => {
    const uid = decodeURIComponent(req.params["uid"] ?? "");
    const removed = banService.unban(uid);
    if (removed) {
      res.json({ removed: true, uid });
    } else {
      res.status(404).json({ removed: false, error: "Ban not found" });
    }
  });

  // ------------------------------------------------------------------
  // Maintenance
  // ------------------------------------------------------------------
  app.post("/bans/prune", (_req: Request, res: Response) => {
    const count = banService.pruneExpired();
    res.json({ pruned: count });
  });

  app.post("/bans/backup", (_req: Request, res: Response) => {
    banService
      .backup(backupDir, maxBackups)
      .then((dest) => res.json({ backup: dest }))
      .catch((err: Error) => res.status(500).json({ error: err.message }));
  });

  // ------------------------------------------------------------------
  // Generic error handler
  // ------------------------------------------------------------------
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  app.use((err: Error, _req: Request, res: Response, _next: NextFunction) => {
    console.error("[api] Unhandled error:", err.message);
    res.status(500).json({ error: err.message });
  });

  return app;
}
