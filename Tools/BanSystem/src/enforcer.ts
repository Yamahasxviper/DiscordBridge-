/**
 * Enforcer – the polling loop that checks every connected player against
 * the ban database and kicks any that are currently banned.
 *
 * Flow:
 *  1. Every `pollIntervalSeconds`, call DS API `QueryServerState`.
 *  2. For each connected player, build their compound UID.
 *  3. Query the ban database.
 *  4. If banned, call `KickPlayer` via the DS API and log the action.
 *  5. Also prune expired bans on every tick.
 */

import type { DSApiClient } from "./dsApiClient.js";
import type { BanService } from "./banService.js";
import type { BanSystemConfig, ConnectedPlayer } from "./models.js";

export class Enforcer {
  private readonly config: BanSystemConfig;
  private readonly dsClient: DSApiClient;
  private readonly banService: BanService;

  private timer: NodeJS.Timeout | null = null;
  private running = false;

  constructor(
    config: BanSystemConfig,
    dsClient: DSApiClient,
    banService: BanService
  ) {
    this.config = config;
    this.dsClient = dsClient;
    this.banService = banService;
  }

  /** Start the polling loop. Resolves immediately (non-blocking). */
  start(): void {
    if (this.running) return;
    this.running = true;
    console.log(
      `[enforcer] Started – polling every ${this.config.pollIntervalSeconds}s`
    );
    // Run once immediately, then on interval
    void this.tick();
    this.timer = setInterval(
      () => void this.tick(),
      this.config.pollIntervalSeconds * 1000
    );
  }

  /** Stop the polling loop. */
  stop(): void {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
    this.running = false;
    console.log("[enforcer] Stopped");
  }

  // ------------------------------------------------------------------
  // Internal
  // ------------------------------------------------------------------

  private async tick(): Promise<void> {
    // Prune expired bans on every cycle
    this.banService.pruneExpired();

    let state;
    try {
      state = await this.dsClient.getServerState();
    } catch (err) {
      console.warn(`[enforcer] Failed to query server state: ${String(err)}`);
      return;
    }

    if (state.connectedPlayers.length === 0) return;

    for (const player of state.connectedPlayers) {
      await this.checkAndEnforce(player);
    }
  }

  private async checkAndEnforce(player: ConnectedPlayer): Promise<void> {
    const result = this.banService.checkBan(player.uid);
    if (!result.isBanned) return;

    const kickMsg = this.banService.buildKickMessage(result.record!);
    console.log(
      `[enforcer] Kicking banned player ${player.playerName} (${player.uid}): ${kickMsg}`
    );

    try {
      await this.dsClient.kickPlayer(player.playerId, kickMsg);
    } catch (kickErr) {
      // Fallback: try console command
      console.warn(
        `[enforcer] kickPlayer API call failed, trying RunCommand fallback: ${String(kickErr)}`
      );
      try {
        await this.dsClient.runCommand(
          `KickPlayer ${player.playerId} "${kickMsg}"`
        );
      } catch (cmdErr) {
        console.error(
          `[enforcer] Fallback kick also failed for ${player.uid}: ${String(cmdErr)}`
        );
      }
    }
  }
}
