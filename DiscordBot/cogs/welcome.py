"""
Welcome cog — sends configurable DMs to new members.

Staff can configure the welcome message and a "latest info" message, and
preview them via /set_welcome commands.
"""

from __future__ import annotations

import os

import discord
from discord import app_commands
from discord.ext import commands

from db import get_config, set_config


def _is_staff(member: discord.Member, staff_role_name: str) -> bool:
    role_name_lower = staff_role_name.lower()
    for role in member.roles:
        if role.name.lower() == role_name_lower or str(role.id) == staff_role_name:
            return True
    return False


class Welcome(commands.Cog):
    """Send configurable DMs to new server members."""

    def __init__(self, bot: commands.Bot) -> None:
        self.bot = bot
        self.staff_role: str = os.getenv("STAFF_ROLE", "Staff")

    # ── Listener ──────────────────────────────────────────────────────────────

    @commands.Cog.listener()
    async def on_member_join(self, member: discord.Member) -> None:
        welcome_msg = get_config("welcome_message", "")
        latest_info = get_config("latest_info", "")

        if welcome_msg:
            try:
                await member.send(welcome_msg)
            except discord.HTTPException:
                pass

        if latest_info:
            try:
                await member.send(latest_info)
            except discord.HTTPException:
                pass

    # ── /set_welcome group ────────────────────────────────────────────────────

    set_welcome_group = app_commands.Group(
        name="set_welcome",
        description="Configure welcome DMs for new members (staff only).",
    )

    @set_welcome_group.command(
        name="message",
        description="Set the welcome DM sent to new members (staff only).",
    )
    @app_commands.describe(
        text="Welcome message text. Leave blank to disable welcome DMs."
    )
    async def set_welcome_message(
        self, interaction: discord.Interaction, text: str = ""
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to configure welcome messages.",
                ephemeral=True,
            )
            return

        await interaction.response.defer(ephemeral=True)
        set_config("welcome_message", text)

        if text:
            await interaction.followup.send(
                "✅ Welcome message updated.", ephemeral=True
            )
        else:
            await interaction.followup.send(
                "✅ Welcome message cleared (disabled).", ephemeral=True
            )

    @set_welcome_group.command(
        name="latest_info",
        description="Set the 'latest info' DM sent to new members (staff only).",
    )
    @app_commands.describe(
        text="Latest-info message text. Leave blank to disable."
    )
    async def set_latest_info(
        self, interaction: discord.Interaction, text: str = ""
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to configure welcome messages.",
                ephemeral=True,
            )
            return

        await interaction.response.defer(ephemeral=True)
        set_config("latest_info", text)

        if text:
            await interaction.followup.send(
                "✅ Latest-info message updated.", ephemeral=True
            )
        else:
            await interaction.followup.send(
                "✅ Latest-info message cleared (disabled).", ephemeral=True
            )

    @set_welcome_group.command(
        name="preview",
        description="Preview the current welcome DM by sending it to yourself (staff only).",
    )
    async def preview(self, interaction: discord.Interaction) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to preview welcome messages.",
                ephemeral=True,
            )
            return

        await interaction.response.defer(ephemeral=True)
        welcome_msg = get_config("welcome_message", "")
        latest_info = get_config("latest_info", "")

        if not welcome_msg and not latest_info:
            await interaction.followup.send(
                "No welcome messages are configured yet.", ephemeral=True
            )
            return

        sent = False
        if welcome_msg:
            try:
                await interaction.user.send(f"**[Preview — Welcome Message]**\n\n{welcome_msg}")
                sent = True
            except discord.HTTPException:
                await interaction.followup.send(
                    "❌ Could not send you a DM — please enable DMs from server members.",
                    ephemeral=True,
                )
                return

        if latest_info:
            try:
                await interaction.user.send(f"**[Preview — Latest Info]**\n\n{latest_info}")
                sent = True
            except discord.HTTPException:
                pass

        if sent:
            await interaction.followup.send(
                "✅ Preview sent to your DMs.", ephemeral=True
            )


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(Welcome(bot))
