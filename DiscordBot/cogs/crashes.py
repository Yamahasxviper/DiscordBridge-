"""
Crashes cog — automatic log/crash analysis and pattern management.

On every message the cog checks:
  • .log attachments  → downloaded and analysed
  • .zip attachments  → unzipped in-memory, FactoryGame.log extracted and analysed
  • pastebin.com URLs → raw content fetched and analysed
  • image attachments → OCR attempted via pytesseract (skipped if not installed)

Staff can manage crash patterns via the /crash slash-command group.
"""

from __future__ import annotations

import io
import os
import re
import zipfile
from typing import TYPE_CHECKING, Optional

import discord
from discord import app_commands
from discord.ext import commands

from db import db

if TYPE_CHECKING:
    from bot import FredBot


def _is_staff(member: discord.Member, staff_role_name: str) -> bool:
    role_name_lower = staff_role_name.lower()
    for role in member.roles:
        if role.name.lower() == role_name_lower or str(role.id) == staff_role_name:
            return True
    return False


class Crashes(commands.Cog):
    """Automatic crash-log analysis and crash-pattern management."""

    def __init__(self, bot: "FredBot") -> None:
        self.bot = bot
        self.staff_role: str = os.getenv("STAFF_ROLE", "Staff")

    # ── Core analysis ─────────────────────────────────────────────────────────

    def _analyze(self, text: str, source: str = "") -> list[tuple[str, str]]:
        """Return a list of (name, response) for every matching crash pattern."""
        findings: list[tuple[str, str]] = []
        with db() as conn:
            rows = conn.execute("SELECT name, pattern, response FROM crashes").fetchall()
        for row in rows:
            try:
                if re.search(row["pattern"], text, re.IGNORECASE | re.S):
                    findings.append((row["name"], row["response"]))
            except re.error:
                pass
        return findings

    async def _fetch_text(self, url: str) -> Optional[str]:
        """Download URL and return text content, or None on failure."""
        try:
            async with self.bot.http_session.get(url) as resp:
                if resp.status == 200:
                    return await resp.text(errors="replace")
        except Exception:
            pass
        return None

    async def _fetch_bytes(self, url: str) -> Optional[bytes]:
        """Download URL and return raw bytes, or None on failure."""
        try:
            async with self.bot.http_session.get(url) as resp:
                if resp.status == 200:
                    return await resp.read()
        except Exception:
            pass
        return None

    async def _send_findings(
        self,
        message: discord.Message,
        findings: list[tuple[str, str]],
        source: str,
    ) -> None:
        """Send an embed for *findings* as a reply to *message*."""
        embed = discord.Embed(
            title="🔍 Crash Analysis",
            color=discord.Color.red(),
        )
        for name, response in findings:
            embed.add_field(name=name, value=response[:1024], inline=False)
        if source:
            embed.set_footer(text=f"Source: {source}")
        await message.reply(embed=embed)

    # ── Message listener ──────────────────────────────────────────────────────

    @commands.Cog.listener()
    async def on_message(self, message: discord.Message) -> None:
        if message.author.bot:
            return

        all_findings: list[tuple[str, str]] = []
        source_label = ""

        # 1. Attachments
        for attachment in message.attachments:
            fname = attachment.filename.lower()

            if fname.endswith(".log"):
                text = await self._fetch_text(attachment.url)
                if text:
                    findings = self._analyze(text, attachment.filename)
                    if findings:
                        await self._send_findings(message, findings, attachment.filename)
                continue

            if fname.endswith(".zip"):
                raw = await self._fetch_bytes(attachment.url)
                if raw:
                    try:
                        with zipfile.ZipFile(io.BytesIO(raw)) as zf:
                            for member in zf.namelist():
                                if member.lower().endswith("factorygame.log"):
                                    log_text = zf.read(member).decode("utf-8", errors="replace")
                                    findings = self._analyze(log_text, member)
                                    if findings:
                                        await self._send_findings(message, findings, member)
                                    break
                    except zipfile.BadZipFile:
                        pass
                continue

            if fname.endswith((".png", ".jpg", ".jpeg")):
                try:
                    import pytesseract
                    from PIL import Image

                    raw_img = await self._fetch_bytes(attachment.url)
                    if raw_img:
                        img = Image.open(io.BytesIO(raw_img))
                        ocr_text = pytesseract.image_to_string(img)
                        findings = self._analyze(ocr_text, attachment.filename)
                        if findings:
                            await self._send_findings(message, findings, attachment.filename)
                except ImportError:
                    pass
                except Exception:
                    pass

        # 2. Pastebin links in message text
        pastebin_pattern = re.compile(r"https?://pastebin\.com/(?!raw/)([A-Za-z0-9]+)")
        for match in pastebin_pattern.finditer(message.content):
            paste_id = match.group(1)
            raw_url = f"https://pastebin.com/raw/{paste_id}"
            text = await self._fetch_text(raw_url)
            if text:
                findings = self._analyze(text, raw_url)
                if findings:
                    await self._send_findings(message, findings, raw_url)

    # ── /crash command group ──────────────────────────────────────────────────

    crash_group = app_commands.Group(
        name="crash",
        description="Manage crash analysis patterns (staff only).",
    )

    @crash_group.command(name="add", description="Add a crash pattern (staff only).")
    @app_commands.describe(
        name="Short name for this crash type.",
        pattern="Python regex pattern to search for in logs.",
        response="Response text shown when this pattern matches.",
    )
    async def crash_add(
        self,
        interaction: discord.Interaction,
        name: str,
        pattern: str,
        response: str,
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage crash patterns.", ephemeral=True
            )
            return

        await interaction.response.defer(ephemeral=True)

        try:
            re.compile(pattern)
        except re.error as exc:
            await interaction.followup.send(
                f"❌ Invalid regex pattern: {exc}", ephemeral=True
            )
            return

        with db() as conn:
            conn.execute(
                "INSERT OR REPLACE INTO crashes (name, pattern, response) VALUES (?, ?, ?)",
                (name, pattern, response),
            )

        await interaction.followup.send(
            f"✅ Crash pattern **{name}** added.", ephemeral=True
        )

    @crash_group.command(name="remove", description="Remove a crash pattern (staff only).")
    @app_commands.describe(name="Name of the pattern to remove.")
    async def crash_remove(self, interaction: discord.Interaction, name: str) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage crash patterns.", ephemeral=True
            )
            return

        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            cur = conn.execute("DELETE FROM crashes WHERE name = ?", (name,))

        if cur.rowcount == 0:
            await interaction.followup.send(
                f"❌ No crash pattern named **{name}** found.", ephemeral=True
            )
            return
        await interaction.followup.send(
            f"✅ Crash pattern **{name}** removed.", ephemeral=True
        )

    @crash_group.command(name="list", description="List all crash pattern names.")
    async def crash_list(self, interaction: discord.Interaction) -> None:
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            rows = conn.execute("SELECT name FROM crashes ORDER BY name").fetchall()

        if not rows:
            await interaction.followup.send(
                "No crash patterns defined yet.", ephemeral=True
            )
            return

        names = [row["name"] for row in rows]
        embed = discord.Embed(
            title=f"🪲 Crash Patterns ({len(names)})",
            description="\n".join(f"• `{n}`" for n in names),
            color=discord.Color.blurple(),
        )
        await interaction.followup.send(embed=embed, ephemeral=True)

    @crash_group.command(
        name="test", description="Test a regex pattern against sample text (staff only)."
    )
    @app_commands.describe(
        pattern="Python regex pattern to test.",
        text="Sample text to match against.",
    )
    async def crash_test(
        self,
        interaction: discord.Interaction,
        pattern: str,
        text: str,
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to test crash patterns.", ephemeral=True
            )
            return

        await interaction.response.defer(ephemeral=True)
        try:
            match = re.search(pattern, text, re.IGNORECASE | re.S)
        except re.error as exc:
            await interaction.followup.send(
                f"❌ Invalid regex pattern: {exc}", ephemeral=True
            )
            return

        if match:
            embed = discord.Embed(
                title="✅ Pattern matched",
                color=discord.Color.green(),
            )
            embed.add_field(name="Match", value=f"`{match.group(0)[:500]}`", inline=False)
        else:
            embed = discord.Embed(
                title="❌ No match",
                color=discord.Color.red(),
            )
        embed.add_field(name="Pattern", value=f"`{pattern}`", inline=False)
        embed.add_field(name="Text", value=f"```{text[:500]}```", inline=False)
        await interaction.followup.send(embed=embed, ephemeral=True)


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(Crashes(bot))  # type: ignore[arg-type]
