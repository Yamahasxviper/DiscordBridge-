"""
ViperBot — main entry point.

Loads all cogs, syncs slash commands, and manages the shared aiohttp session.
"""

from __future__ import annotations

import asyncio
import logging
import os
from typing import Optional

import aiohttp
import discord
from discord.ext import commands
from dotenv import load_dotenv

from db import get_config, init_db

load_dotenv()

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
log = logging.getLogger("viperbot")

COGS = [
    "cogs.faq",
    "cogs.help",
    "cogs.tags",
    "cogs.ficsit",
    "cogs.crashes",
    "cogs.mediaonly",
    "cogs.welcome",
    "cogs.levelling",
    "cogs.webhooklistener",
]


class ViperBot(commands.Bot):
    """Main bot class with shared HTTP session and SQLite database."""

    http_session: aiohttp.ClientSession

    def __init__(self) -> None:
        prefix = self._read_prefix()
        intents = discord.Intents.default()
        intents.message_content = True
        intents.members = True
        super().__init__(command_prefix=prefix, intents=intents)

    # ── Helpers ───────────────────────────────────────────────────────────────

    @staticmethod
    def _read_prefix() -> str:
        """Read command prefix from DB if available, else fall back to '!'."""
        try:
            return get_config("prefix") or "!"
        except Exception:
            return "!"

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    async def setup_hook(self) -> None:
        init_db()
        # Refresh prefix from DB now that the DB is ready.
        self.command_prefix = get_config("prefix") or "!"

        self.http_session = aiohttp.ClientSession()

        for cog in COGS:
            try:
                await self.load_extension(cog)
                log.info("Loaded cog: %s", cog)
            except Exception as exc:
                log.error("Failed to load cog %s: %s", cog, exc)

        guild_id_str: Optional[str] = os.getenv("DISCORD_GUILD_ID")
        if guild_id_str:
            guild = discord.Object(id=int(guild_id_str))
            self.tree.copy_global_to(guild=guild)
            await self.tree.sync(guild=guild)
            log.info("Slash commands synced to guild %s", guild_id_str)
        else:
            await self.tree.sync()
            log.info("Slash commands synced globally")

    async def close(self) -> None:
        await self.http_session.close()
        await super().close()

    async def on_ready(self) -> None:
        log.info("Logged in as %s (ID: %s)", self.user, self.user.id if self.user else "?")


bot = ViperBot()

if __name__ == "__main__":
    token = os.environ["DISCORD_TOKEN"]
    bot.run(token)
