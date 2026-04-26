"""
Webhook listener cog — aiohttp HTTP server for GitHub webhook events.

Starts an aiohttp web server alongside the Discord bot.  Incoming GitHub
webhook ``POST /githook`` payloads are translated into Discord embeds and
forwarded to the configured channel.

Supported event types: push, pull_request, issues, release, member.
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
from typing import TYPE_CHECKING, Optional

import aiohttp.web
import discord
from discord import app_commands
from discord.ext import commands

from db import get_config, set_config

if TYPE_CHECKING:
    from bot import FredBot

log = logging.getLogger("fredbot.webhooklistener")


def _is_staff(member: discord.Member, staff_role_name: str) -> bool:
    role_name_lower = staff_role_name.lower()
    for role in member.roles:
        if role.name.lower() == role_name_lower or str(role.id) == staff_role_name:
            return True
    return False


def _build_embed(event: str, payload: dict) -> Optional[discord.Embed]:
    """Build a Discord embed for a GitHub webhook event, or None to skip."""
    repo_name: str = (payload.get("repository") or {}).get("full_name", "unknown/repo")

    if event == "push":
        ref: str = payload.get("ref", "")
        branch = ref.replace("refs/heads/", "") if ref.startswith("refs/heads/") else ref
        commits: list[dict] = payload.get("commits", [])
        forced: bool = payload.get("forced", False)
        color = discord.Color.red() if forced else discord.Color.green()
        title = f"{'⚠️ Force-pushed' if forced else 'Pushed'} {len(commits)} commit(s) to {repo_name}/{branch}"
        lines = [
            f"[`{c['id'][:7]}`]({c['url']}) {c['message'].splitlines()[0][:80]}"
            for c in commits[:5]
        ]
        embed = discord.Embed(title=title, description="\n".join(lines), color=color)
        pusher = (payload.get("pusher") or {}).get("name", "")
        if pusher:
            embed.set_footer(text=f"Pushed by {pusher}")
        return embed

    if event == "pull_request":
        action: str = payload.get("action", "")
        pr: dict = payload.get("pull_request", {})
        title_text = f"PR {action} in {repo_name}: {pr.get('title', '')}"
        additions = pr.get("additions", "?")
        deletions = pr.get("deletions", "?")
        action_colors = {
            "opened": discord.Color.green(),
            "closed": discord.Color.red(),
            "merged": discord.Color.purple(),
            "reopened": discord.Color.yellow(),
        }
        color = action_colors.get(action, discord.Color.blurple())
        embed = discord.Embed(
            title=title_text,
            url=pr.get("html_url", ""),
            color=color,
        )
        embed.add_field(name="Changes", value=f"+{additions} / -{deletions}", inline=True)
        embed.add_field(name="Author", value=pr.get("user", {}).get("login", "?"), inline=True)
        return embed

    if event == "issues":
        action = payload.get("action", "")
        issue: dict = payload.get("issue", {})
        n = issue.get("number", "?")
        title_text = f"Issue #{n} {action} in {repo_name}: {issue.get('title', '')}"
        action_colors = {
            "opened": discord.Color.green(),
            "deleted": discord.Color.red(),
        }
        color = action_colors.get(action, discord.Color.orange())
        embed = discord.Embed(
            title=title_text,
            url=issue.get("html_url", ""),
            color=color,
        )
        return embed

    if event == "release":
        release: dict = payload.get("release", {})
        tag = release.get("tag_name", "?")
        prerelease = release.get("prerelease", False)
        title_text = f"New {'pre-release' if prerelease else 'release'} for {repo_name}: {tag}"
        embed = discord.Embed(
            title=title_text,
            url=release.get("html_url", ""),
            color=discord.Color.gold(),
        )
        body = (release.get("body") or "")[:500]
        if body:
            embed.description = body
        return embed

    if event == "member":
        action = payload.get("action", "")
        if action == "added":
            member_login = (payload.get("member") or {}).get("login", "?")
            embed = discord.Embed(
                title=f"{member_login} added to {repo_name}",
                color=discord.Color.green(),
            )
            return embed

    return None


class WebhookListener(commands.Cog):
    """Receives GitHub webhook events and posts them as Discord embeds."""

    def __init__(self, bot: "FredBot") -> None:
        self.bot = bot
        self.staff_role: str = os.getenv("STAFF_ROLE", "Staff")
        self._runner: Optional[aiohttp.web.AppRunner] = None

    async def cog_load(self) -> None:
        asyncio.create_task(self._start_server())

    async def cog_unload(self) -> None:
        if self._runner:
            await self._runner.cleanup()

    # ── HTTP server ───────────────────────────────────────────────────────────

    async def _start_server(self) -> None:
        port = int(os.getenv("WEBHOOK_PORT", "8080"))
        app = aiohttp.web.Application()
        app.router.add_post("/githook", self._handle_githook)
        app.router.add_get("/healthy", self._handle_healthy)

        self._runner = aiohttp.web.AppRunner(app)
        await self._runner.setup()
        site = aiohttp.web.TCPSite(self._runner, "0.0.0.0", port)
        await site.start()
        log.info("Webhook HTTP server listening on port %d", port)

    async def _handle_healthy(self, request: aiohttp.web.Request) -> aiohttp.web.Response:
        return aiohttp.web.Response(text="OK")

    async def _handle_githook(self, request: aiohttp.web.Request) -> aiohttp.web.Response:
        event = request.headers.get("x-github-event", "")
        try:
            payload = await request.json()
        except Exception:
            return aiohttp.web.Response(status=400, text="Invalid JSON")

        embed = _build_embed(event, payload)
        if embed is not None:
            channel_id_str = get_config("githook_channel", "")
            if channel_id_str:
                try:
                    channel = self.bot.get_channel(int(channel_id_str))
                    if isinstance(channel, discord.TextChannel):
                        asyncio.create_task(channel.send(embed=embed))
                except Exception as exc:
                    log.warning("Could not send webhook embed: %s", exc)

        return aiohttp.web.Response(text="OK")

    # ── Slash command ─────────────────────────────────────────────────────────

    @app_commands.command(
        name="set_webhook_channel",
        description="Set the channel where GitHub webhook events are posted (staff only).",
    )
    @app_commands.describe(channel="Channel to receive GitHub event notifications.")
    async def set_webhook_channel(
        self, interaction: discord.Interaction, channel: discord.TextChannel
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to configure the webhook channel.",
                ephemeral=True,
            )
            return

        await interaction.response.defer(ephemeral=True)
        set_config("githook_channel", str(channel.id))
        await interaction.followup.send(
            f"✅ GitHub webhook events will be posted to {channel.mention}.",
            ephemeral=True,
        )


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(WebhookListener(bot))  # type: ignore[arg-type]
