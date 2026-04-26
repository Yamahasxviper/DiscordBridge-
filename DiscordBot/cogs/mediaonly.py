"""
Media-only cog — enforces media-only in designated channels.

Messages without attachments or embeds in a configured channel are deleted and
the author is sent their message content as a DM explaining the rule.
"""

from __future__ import annotations

import os

import discord
from discord import app_commands
from discord.ext import commands

from db import db


def _is_staff(member: discord.Member, staff_role_name: str) -> bool:
    role_name_lower = staff_role_name.lower()
    for role in member.roles:
        if role.name.lower() == role_name_lower or str(role.id) == staff_role_name:
            return True
    return False


class MediaOnly(commands.Cog):
    """Enforce media-only channels."""

    def __init__(self, bot: commands.Bot) -> None:
        self.bot = bot
        self.staff_role: str = os.getenv("STAFF_ROLE", "Staff")

    # ── Listener ──────────────────────────────────────────────────────────────

    @commands.Cog.listener()
    async def on_message(self, message: discord.Message) -> None:
        if message.author.bot:
            return
        if not isinstance(message.channel, discord.TextChannel):
            return

        with db() as conn:
            row = conn.execute(
                "SELECT 1 FROM mediaonly_channels WHERE channel_id = ?",
                (message.channel.id,),
            ).fetchone()

        if row is None:
            return

        if message.attachments or message.embeds:
            return

        original_content = message.content

        try:
            await message.delete()
        except discord.HTTPException:
            pass

        try:
            await message.author.send(
                f"👋 Your message in **#{message.channel.name}** was removed because "
                "that channel is **media-only** (images/files only).\n\n"
                "Your message content:\n"
                f"```\n{original_content[:1800]}\n```"
            )
        except discord.HTTPException:
            pass

    # ── /mediaonly group ──────────────────────────────────────────────────────

    mediaonly_group = app_commands.Group(
        name="mediaonly",
        description="Configure media-only channels (staff only).",
    )

    @mediaonly_group.command(
        name="add", description="Mark a channel as media-only (staff only)."
    )
    @app_commands.describe(channel="The channel to restrict to media posts only.")
    async def mediaonly_add(
        self, interaction: discord.Interaction, channel: discord.TextChannel
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to configure media-only channels.",
                ephemeral=True,
            )
            return

        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            conn.execute(
                "INSERT OR IGNORE INTO mediaonly_channels (channel_id) VALUES (?)",
                (channel.id,),
            )

        await interaction.followup.send(
            f"✅ {channel.mention} is now a **media-only** channel.", ephemeral=True
        )

    @mediaonly_group.command(
        name="remove", description="Remove media-only restriction from a channel (staff only)."
    )
    @app_commands.describe(channel="The channel to remove the media-only restriction from.")
    async def mediaonly_remove(
        self, interaction: discord.Interaction, channel: discord.TextChannel
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to configure media-only channels.",
                ephemeral=True,
            )
            return

        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            cur = conn.execute(
                "DELETE FROM mediaonly_channels WHERE channel_id = ?", (channel.id,)
            )

        if cur.rowcount == 0:
            await interaction.followup.send(
                f"❌ {channel.mention} was not in the media-only list.", ephemeral=True
            )
            return
        await interaction.followup.send(
            f"✅ {channel.mention} is no longer media-only.", ephemeral=True
        )

    @mediaonly_group.command(name="list", description="List all media-only channels.")
    async def mediaonly_list(self, interaction: discord.Interaction) -> None:
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            rows = conn.execute(
                "SELECT channel_id FROM mediaonly_channels"
            ).fetchall()

        if not rows:
            await interaction.followup.send(
                "No media-only channels configured.", ephemeral=True
            )
            return

        mentions = []
        for row in rows:
            ch = interaction.guild.get_channel(row["channel_id"]) if interaction.guild else None
            mentions.append(ch.mention if ch else f"<#{row['channel_id']}>")

        embed = discord.Embed(
            title="🖼️ Media-Only Channels",
            description="\n".join(mentions),
            color=discord.Color.blurple(),
        )
        await interaction.followup.send(embed=embed, ephemeral=True)


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(MediaOnly(bot))
