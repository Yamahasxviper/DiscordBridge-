"""
Levelling cog — XP, ranks, and leaderboards.

XP is awarded for messages with a per-user cooldown.  Ranks are computed from
XP using a logarithmic scale and mapped to Discord roles via the rank_roles
table.  Staff can manage XP and role mappings via slash commands.
"""

from __future__ import annotations

import math
import os
from datetime import datetime, timezone
from typing import Optional

import discord
from discord import app_commands
from discord.ext import commands

from db import db, get_config, set_config


def _is_staff(member: discord.Member, staff_role_name: str) -> bool:
    role_name_lower = staff_role_name.lower()
    for role in member.roles:
        if role.name.lower() == role_name_lower or str(role.id) == staff_role_name:
            return True
    return False


def _compute_rank(xp: float, base: float, multiplier: float) -> int:
    """Return rank (>= 0) for the given XP value."""
    if xp < base or multiplier <= 1:
        return 0
    try:
        rank = math.floor(math.log(xp / base) / math.log(multiplier))
        return max(0, rank)
    except (ValueError, ZeroDivisionError):
        return 0


class Levelling(commands.Cog):
    """XP and rank tracking."""

    def __init__(self, bot: commands.Bot) -> None:
        self.bot = bot
        self.staff_role: str = os.getenv("STAFF_ROLE", "Staff")
        self._last_xp_time: dict[int, datetime] = {}

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _ensure_user(self, conn, user_id: int) -> None:
        conn.execute(
            "INSERT OR IGNORE INTO users (user_id) VALUES (?)", (user_id,)
        )

    def _get_user(self, conn, user_id: int) -> Optional[dict]:
        return conn.execute(
            "SELECT * FROM users WHERE user_id = ?", (user_id,)
        ).fetchone()

    async def _apply_rank_role(
        self, member: discord.Member, new_rank: int, old_rank: int
    ) -> None:
        """Assign the role for new_rank and remove the role for old_rank if different."""
        if new_rank == old_rank:
            return

        with db() as conn:
            new_role_row = conn.execute(
                "SELECT role_id FROM rank_roles WHERE rank = ?", (new_rank,)
            ).fetchone()
            old_role_row = conn.execute(
                "SELECT role_id FROM rank_roles WHERE rank = ?", (old_rank,)
            ).fetchone()

        if old_role_row:
            old_role = member.guild.get_role(old_role_row["role_id"])
            if old_role and old_role in member.roles:
                try:
                    await member.remove_roles(old_role, reason="Rank up")
                except discord.HTTPException:
                    pass

        if new_role_row:
            new_role = member.guild.get_role(new_role_row["role_id"])
            if new_role and new_role not in member.roles:
                try:
                    await member.add_roles(new_role, reason=f"Reached rank {new_rank}")
                except discord.HTTPException:
                    pass

    # ── Listener ──────────────────────────────────────────────────────────────

    @commands.Cog.listener()
    async def on_message(self, message: discord.Message) -> None:
        if message.author.bot:
            return
        if not isinstance(message.channel, discord.TextChannel):
            return
        if get_config("levelling_state", "1") != "1":
            return

        try:
            base = float(get_config("base_level_value") or "100")
            multiplier = float(get_config("level_value_multiplier") or "1.5")
            xp_gain = float(get_config("xp_gain_value") or "10")
            delay_secs = float(get_config("xp_gain_delay") or "60")
        except ValueError:
            return

        user_id = message.author.id
        now = datetime.now(timezone.utc)

        with db() as conn:
            self._ensure_user(conn, user_id)
            row = self._get_user(conn, user_id)
            if row is None:
                return

            conn.execute(
                "UPDATE users SET message_count = message_count + 1 WHERE user_id = ?",
                (user_id,),
            )

            last = self._last_xp_time.get(user_id)
            if last and (now - last).total_seconds() < delay_secs:
                return  # Still on cooldown

            self._last_xp_time[user_id] = now
            effective_gain = xp_gain * float(row["xp_multiplier"])
            new_xp = float(row["xp_count"]) + effective_gain
            old_rank = int(row["rank"])
            new_rank = _compute_rank(new_xp, base, multiplier)

            conn.execute(
                "UPDATE users SET xp_count = ?, rank = ? WHERE user_id = ?",
                (new_xp, new_rank, user_id),
            )

        if new_rank != old_rank and isinstance(message.author, discord.Member):
            await self._apply_rank_role(message.author, new_rank, old_rank)

    # ── Prefix commands ───────────────────────────────────────────────────────

    @commands.command(name="level")
    async def level_cmd(
        self, ctx: commands.Context, member: Optional[discord.Member] = None
    ) -> None:
        """Show a user's level and XP."""
        target = member or ctx.author
        with db() as conn:
            row = conn.execute(
                "SELECT xp_count, rank, message_count FROM users WHERE user_id = ?",
                (target.id,),
            ).fetchone()

        if row is None:
            embed = discord.Embed(
                description=f"{target.mention} hasn't earned any XP yet.",
                color=discord.Color.blurple(),
            )
        else:
            embed = discord.Embed(
                title=f"⭐ {target.display_name}'s Level",
                color=discord.Color.gold(),
            )
            embed.add_field(name="Rank", value=str(row["rank"]), inline=True)
            embed.add_field(name="XP", value=f"{row['xp_count']:.1f}", inline=True)
            embed.add_field(name="Messages", value=str(row["message_count"]), inline=True)
            embed.set_thumbnail(url=target.display_avatar.url)

        await ctx.reply(embed=embed)

    @commands.command(name="leaderboard")
    async def leaderboard_cmd(self, ctx: commands.Context) -> None:
        """Show the top 10 users by XP."""
        with db() as conn:
            rows = conn.execute(
                "SELECT user_id, xp_count, rank FROM users ORDER BY xp_count DESC LIMIT 10"
            ).fetchall()

        embed = discord.Embed(
            title="🏆 XP Leaderboard",
            color=discord.Color.gold(),
        )
        lines = []
        for i, row in enumerate(rows, start=1):
            user = ctx.guild.get_member(row["user_id"]) if ctx.guild else None
            name = user.display_name if user else f"User {row['user_id']}"
            lines.append(
                f"**{i}.** {name} — {row['xp_count']:.1f} XP (Rank {row['rank']})"
            )

        embed.description = "\n".join(lines) if lines else "No data yet."
        await ctx.reply(embed=embed)

    # ── /xp slash commands ────────────────────────────────────────────────────

    xp_group = app_commands.Group(
        name="xp",
        description="Manage user XP (staff only).",
    )

    @xp_group.command(name="give", description="Give XP to a user (staff only).")
    @app_commands.describe(user="Target user.", amount="Amount of XP to give.")
    async def xp_give(
        self, interaction: discord.Interaction, user: discord.Member, amount: float
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role.", ephemeral=True
            )
            return
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            self._ensure_user(conn, user.id)
            conn.execute(
                "UPDATE users SET xp_count = xp_count + ? WHERE user_id = ?",
                (amount, user.id),
            )
        await interaction.followup.send(
            f"✅ Gave **{amount:.1f}** XP to {user.mention}.", ephemeral=True
        )

    @xp_group.command(name="take", description="Remove XP from a user (staff only).")
    @app_commands.describe(user="Target user.", amount="Amount of XP to remove.")
    async def xp_take(
        self, interaction: discord.Interaction, user: discord.Member, amount: float
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role.", ephemeral=True
            )
            return
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            self._ensure_user(conn, user.id)
            conn.execute(
                "UPDATE users SET xp_count = MAX(0, xp_count - ?) WHERE user_id = ?",
                (amount, user.id),
            )
        await interaction.followup.send(
            f"✅ Removed **{amount:.1f}** XP from {user.mention}.", ephemeral=True
        )

    @xp_group.command(name="set", description="Set a user's XP to an exact value (staff only).")
    @app_commands.describe(user="Target user.", amount="New XP value.")
    async def xp_set(
        self, interaction: discord.Interaction, user: discord.Member, amount: float
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role.", ephemeral=True
            )
            return
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            self._ensure_user(conn, user.id)
            conn.execute(
                "UPDATE users SET xp_count = ? WHERE user_id = ?",
                (max(0.0, amount), user.id),
            )
        await interaction.followup.send(
            f"✅ Set {user.mention}'s XP to **{amount:.1f}**.", ephemeral=True
        )

    @xp_group.command(
        name="multiplier", description="Set a user's XP multiplier (staff only)."
    )
    @app_commands.describe(user="Target user.", multiplier="XP gain multiplier (e.g. 2.0).")
    async def xp_multiplier(
        self,
        interaction: discord.Interaction,
        user: discord.Member,
        multiplier: float,
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role.", ephemeral=True
            )
            return
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            self._ensure_user(conn, user.id)
            conn.execute(
                "UPDATE users SET xp_multiplier = ? WHERE user_id = ?",
                (max(0.0, multiplier), user.id),
            )
        await interaction.followup.send(
            f"✅ Set {user.mention}'s XP multiplier to **{multiplier}×**.", ephemeral=True
        )

    # ── /levelling slash commands ─────────────────────────────────────────────

    levelling_group = app_commands.Group(
        name="levelling",
        description="Configure the levelling system (staff only).",
    )

    @levelling_group.command(
        name="state", description="Enable or disable the levelling system (staff only)."
    )
    @app_commands.describe(enabled="True to enable levelling, False to disable.")
    async def levelling_state(
        self, interaction: discord.Interaction, enabled: bool
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role.", ephemeral=True
            )
            return
        await interaction.response.defer(ephemeral=True)
        set_config("levelling_state", "1" if enabled else "0")
        state_str = "enabled" if enabled else "disabled"
        await interaction.followup.send(
            f"✅ Levelling system is now **{state_str}**.", ephemeral=True
        )

    @levelling_group.command(
        name="add_role",
        description="Map a rank number to a Discord role (staff only).",
    )
    @app_commands.describe(role="The role to assign.", rank="The rank number that earns this role.")
    async def levelling_add_role(
        self,
        interaction: discord.Interaction,
        role: discord.Role,
        rank: int,
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role.", ephemeral=True
            )
            return
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            conn.execute(
                "INSERT OR REPLACE INTO rank_roles (role_id, rank) VALUES (?, ?)",
                (role.id, rank),
            )
        await interaction.followup.send(
            f"✅ {role.mention} will be awarded at **rank {rank}**.", ephemeral=True
        )

    @levelling_group.command(
        name="remove_role",
        description="Remove a role from rank-role mappings (staff only).",
    )
    @app_commands.describe(role="The role to remove from rank mappings.")
    async def levelling_remove_role(
        self, interaction: discord.Interaction, role: discord.Role
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role.", ephemeral=True
            )
            return
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            cur = conn.execute(
                "DELETE FROM rank_roles WHERE role_id = ?", (role.id,)
            )
        if cur.rowcount == 0:
            await interaction.followup.send(
                f"❌ {role.mention} was not in the rank-role mappings.", ephemeral=True
            )
            return
        await interaction.followup.send(
            f"✅ {role.mention} removed from rank-role mappings.", ephemeral=True
        )


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(Levelling(bot))
