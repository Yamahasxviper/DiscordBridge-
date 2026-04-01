/**
 * Satisfactory Dedicated Server HTTP API client.
 *
 * The DS exposes a single POST endpoint:  POST /api/v1
 * Every request body is:
 *   { "function": "<FunctionName>", "data": { ... } }
 *
 * Authentication uses a Bearer token obtained via PasswordLogin.
 * The token is cached in memory and refreshed automatically on 401.
 *
 * Reference: https://satisfactory.wiki.gg/wiki/Dedicated_servers/HTTPS_API
 */

import * as https from "node:https";
import * as http from "node:http";
import * as url from "node:url";
import type { BanSystemConfig, ConnectedPlayer, Platform, ServerState } from "./models.js";

// ------------------------------------------------------------------
// Raw DS API response shapes
// ------------------------------------------------------------------

interface DSApiResponse<T = unknown> {
  data?: T;
  errorCode?: string;
  errorMessage?: string;
}

interface PasswordLoginResponse {
  authenticationToken: string;
}

interface QueryServerStateData {
  serverGameState: {
    activeSessionName: string;
    numConnectedPlayers: number;
    playerLimit: number;
    techTier: number;
    isGameRunning: boolean;
    serverName?: string;
    connectedPlayers?: RawPlayerEntry[];
  };
}

interface RawPlayerEntry {
  playerName: string;
  playerId?: string;
  platform?: string;
}

// ------------------------------------------------------------------
// Client
// ------------------------------------------------------------------

export class DSApiClient {
  private readonly config: BanSystemConfig;
  private authToken: string | null = null;
  private readonly agent: https.Agent;

  constructor(config: BanSystemConfig) {
    this.config = config;
    this.agent = new https.Agent({
      rejectUnauthorized: !config.allowSelfSignedTls,
    });
  }

  // ------------------------------------------------------------------
  // Authentication
  // ------------------------------------------------------------------

  /** Obtain (or reuse) an admin Bearer token from the DS. */
  async authenticate(): Promise<string> {
    if (this.authToken) return this.authToken;

    const resp = await this.call<PasswordLoginResponse>("PasswordLogin", {
      minimumPrivilegeLevel: "Administrator",
      password: this.config.dsAdminPassword,
    });

    if (!resp.data?.authenticationToken) {
      throw new Error(
        `DS authentication failed: ${resp.errorCode ?? "unknown"} – ${resp.errorMessage ?? ""}`
      );
    }

    this.authToken = resp.data.authenticationToken;
    return this.authToken;
  }

  /** Invalidate the cached token (called on 401). */
  private invalidateToken(): void {
    this.authToken = null;
  }

  // ------------------------------------------------------------------
  // High-level operations
  // ------------------------------------------------------------------

  /** Query the server state and return a typed snapshot. */
  async getServerState(): Promise<ServerState> {
    const resp = await this.callAuthenticated<QueryServerStateData>(
      "QueryServerState",
      {}
    );
    const gs = resp.data?.serverGameState;
    if (!gs) {
      throw new Error("QueryServerState returned no serverGameState");
    }

    const connectedPlayers: ConnectedPlayer[] = (gs.connectedPlayers ?? []).map(
      (p) => rawPlayerToConnected(p)
    );

    return {
      serverName: gs.serverName ?? "Unknown",
      activeSessionName: gs.activeSessionName,
      numConnectedPlayers: gs.numConnectedPlayers,
      playerLimit: gs.playerLimit,
      techTier: gs.techTier,
      isGameRunning: gs.isGameRunning,
      connectedPlayers,
    };
  }

  /**
   * Kick a player from the server.
   * `playerId` is the platform-native ID (Steam64 or EOS PUID).
   */
  async kickPlayer(playerId: string, kickReason: string): Promise<void> {
    await this.callAuthenticated("KickPlayer", { playerId, kickReason });
  }

  /**
   * Run an arbitrary server console command.
   * Used as a fallback kick mechanism: `DisconnectPlayer <playerId> <reason>`
   */
  async runCommand(command: string): Promise<string> {
    const resp = await this.callAuthenticated<{ commandResult: string }>(
      "RunCommand",
      { command }
    );
    return resp.data?.commandResult ?? "";
  }

  // ------------------------------------------------------------------
  // Transport
  // ------------------------------------------------------------------

  /** Call an endpoint that does NOT require authentication. */
  async call<T>(functionName: string, data: Record<string, unknown> = {}): Promise<DSApiResponse<T>> {
    return this.rawPost<T>(functionName, data, null);
  }

  /** Call an endpoint that requires admin authentication. Retries once on 401. */
  async callAuthenticated<T>(
    functionName: string,
    data: Record<string, unknown> = {}
  ): Promise<DSApiResponse<T>> {
    const token = await this.authenticate();
    try {
      return await this.rawPost<T>(functionName, data, token);
    } catch (err) {
      if (err instanceof DSApiError && err.statusCode === 401) {
        this.invalidateToken();
        const freshToken = await this.authenticate();
        return this.rawPost<T>(functionName, data, freshToken);
      }
      throw err;
    }
  }

  /** Low-level POST to /api/v1 */
  private rawPost<T>(
    functionName: string,
    data: Record<string, unknown>,
    token: string | null
  ): Promise<DSApiResponse<T>> {
    return new Promise((resolve, reject) => {
      const parsed = new url.URL(this.config.dsApiUrl);
      const isHttps = parsed.protocol === "https:";
      const port = parsed.port
        ? parseInt(parsed.port, 10)
        : isHttps
        ? 443
        : 80;

      const body = JSON.stringify({ function: functionName, data });

      const headers: Record<string, string> = {
        "Content-Type": "application/json",
        "Content-Length": Buffer.byteLength(body).toString(),
      };
      if (token) {
        headers["Authorization"] = `Bearer ${token}`;
      }

      const reqOptions: https.RequestOptions = {
        hostname: parsed.hostname,
        port,
        path: "/api/v1",
        method: "POST",
        headers,
        agent: isHttps ? this.agent : undefined,
      };

      const transport = isHttps ? https : http;
      const req = transport.request(reqOptions, (res) => {
        let raw = "";
        res.setEncoding("utf-8");
        res.on("data", (chunk) => (raw += chunk));
        res.on("end", () => {
          if ((res.statusCode ?? 0) >= 400) {
            reject(new DSApiError(res.statusCode ?? 0, raw, functionName));
            return;
          }
          try {
            resolve(raw ? (JSON.parse(raw) as DSApiResponse<T>) : {});
          } catch {
            reject(new Error(`Failed to parse DS API response: ${raw}`));
          }
        });
      });

      req.on("error", reject);
      req.setTimeout(10_000, () => {
        req.destroy(new Error(`DS API request timed out: ${functionName}`));
      });
      req.write(body);
      req.end();
    });
  }
}

// ------------------------------------------------------------------
// Error type
// ------------------------------------------------------------------

export class DSApiError extends Error {
  constructor(
    public readonly statusCode: number,
    public readonly body: string,
    public readonly functionName: string
  ) {
    super(`DS API error ${statusCode} on ${functionName}: ${body}`);
    this.name = "DSApiError";
  }
}

// ------------------------------------------------------------------
// Helper
// ------------------------------------------------------------------

function rawPlayerToConnected(p: RawPlayerEntry): ConnectedPlayer {
  const rawPlatform = (p.platform ?? "UNKNOWN").toUpperCase();
  const platform: Platform =
    rawPlatform === "STEAM" || rawPlatform === "EOS" ? rawPlatform : "UNKNOWN";
  const playerId = p.playerId ?? "";
  const uid = `${platform}:${playerId}`;
  return { uid, playerName: p.playerName, platform, playerId };
}
