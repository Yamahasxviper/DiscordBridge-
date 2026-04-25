"""
Help cog — /help embed listing available commands.
"""

from __future__ import annotations

import discord
from discord import app_commands
from discord.ext import commands


class Help(commands.Cog):
    """Provides a /help command with an overview of all bot features."""

    def __init__(self, bot: commands.Bot) -> None:
        self.bot = bot

    @app_commands.command(
        name="help",
        description="Show all available commands and what they do.",
    )
    async def help_cmd(self, interaction: discord.Interaction) -> None:
        embed = discord.Embed(
            title="🤖 Satisfactory Q&A Bot — Help",
            description=(
                "I answer questions about **Satisfactory** and its **mods**. "
                "Here are all my commands:"
            ),
            color=discord.Color.blurple(),
        )

        embed.add_field(
            name="❓ `/ask <question>`",
            value=(
                "Ask any question about Satisfactory or modding. "
                "I'll search the knowledge base and return the best matching answer."
            ),
            inline=False,
        )
        embed.add_field(
            name="📋 `/faq list`",
            value="List all FAQ topics and their IDs.",
            inline=False,
        )
        embed.add_field(
            name="➕ `/faq add` *(staff)*",
            value="Add a new question + answer to the knowledge base.",
            inline=False,
        )
        embed.add_field(
            name="✏️ `/faq edit` *(staff)*",
            value="Edit an existing FAQ entry by its ID.",
            inline=False,
        )
        embed.add_field(
            name="🗑️ `/faq remove` *(staff)*",
            value="Delete an FAQ entry by its ID.",
            inline=False,
        )

        embed.add_field(
            name="📖 Useful links",
            value=(
                "• Modding docs: https://docs.ficsit.app/satisfactory-modding/latest/\n"
                "• Mod repository: https://ficsit.app\n"
                "• Modding Discord: https://discord.ficsit.app"
            ),
            inline=False,
        )

        embed.set_footer(text="Staff commands require the configured Staff role.")
        await interaction.response.send_message(embed=embed, ephemeral=True)


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(Help(bot))
