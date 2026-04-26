"""
Tags cog — runtime tag/command system mirroring FICSIT-Fred's !tagname commands.

Tags can be invoked via the bot prefix (e.g. ``!tagname arg1 arg2``) or via
the ``/tag`` slash-command group.  Aliases are stored with content ``!target``.
"""

from __future__ import annotations

import io
import os
from typing import TYPE_CHECKING, Optional

import discord
from discord import app_commands
from discord.ext import commands

from db import db, get_config

if TYPE_CHECKING:
    from bot import FredBot


def _is_staff(member: discord.Member, staff_role_name: str) -> bool:
    role_name_lower = staff_role_name.lower()
    for role in member.roles:
        if role.name.lower() == role_name_lower or str(role.id) == staff_role_name:
            return True
    return False


def _resolve_tag(name: str, args: list[str], depth: int = 0) -> Optional[tuple[str, Optional[str]]]:
    """
    Follow alias chains and substitute positional placeholders.

    Returns ``(content, attachment_url)`` or ``None`` if not found.
    Alias chains are capped at depth 10 to prevent infinite loops.
    """
    if depth > 10:
        return None
    with db() as conn:
        row = conn.execute(
            "SELECT content, attachment FROM tags WHERE name = ?",
            (name.lower(),),
        ).fetchone()
    if row is None:
        return None

    content: str = row["content"] or ""
    attachment: Optional[str] = row["attachment"]

    # Follow alias — stored as "!target"
    if content.startswith("!") and not content.startswith("! "):
        target = content[1:].strip()
        return _resolve_tag(target, args, depth + 1)

    # Substitute positional args: {0}, {1}, {2}, ...
    for i, arg in enumerate(args):
        content = content.replace(f"{{{i}}}", arg)

    return content, attachment


class Tags(commands.Cog):
    """Runtime tag system — !<tag> prefix and /tag slash commands."""

    def __init__(self, bot: "FredBot") -> None:
        self.bot = bot
        self.staff_role: str = os.getenv("STAFF_ROLE", "Staff")

    # ── Prefix listener ───────────────────────────────────────────────────────

    @commands.Cog.listener()
    async def on_message(self, message: discord.Message) -> None:
        if message.author.bot:
            return

        prefix = get_config("prefix") or "!"
        if not message.content.startswith(prefix):
            return

        # Strip prefix and split into word + args
        body = message.content[len(prefix):].strip()
        if not body:
            return
        parts = body.split()
        tag_name = parts[0].lower()
        args = parts[1:]

        result = _resolve_tag(tag_name, args)
        if result is None:
            return  # Not a known tag — let other listeners/commands handle it

        content, attachment_url = result

        file: Optional[discord.File] = None
        if attachment_url:
            try:
                async with self.bot.http_session.get(attachment_url) as resp:
                    if resp.status == 200:
                        data = await resp.read()
                        filename = attachment_url.split("/")[-1].split("?")[0] or "attachment"
                        file = discord.File(io.BytesIO(data), filename=filename)
            except Exception:
                pass  # Attachment download failed; send text only

        await message.channel.send(content=content or None, file=file)

    # ── /tag command group ────────────────────────────────────────────────────

    tag_group = app_commands.Group(
        name="tag",
        description="Manage and display tags.",
    )

    @tag_group.command(name="show", description="Show the content of a tag.")
    @app_commands.describe(name="Name of the tag to show.")
    async def tag_show(self, interaction: discord.Interaction, name: str) -> None:
        await interaction.response.defer()
        result = _resolve_tag(name.lower(), [])
        if result is None:
            await interaction.followup.send(
                f"❌ No tag named **{name}** was found.", ephemeral=True
            )
            return

        content, attachment_url = result
        file: Optional[discord.File] = None
        if attachment_url:
            try:
                async with self.bot.http_session.get(attachment_url) as resp:
                    if resp.status == 200:
                        data = await resp.read()
                        filename = attachment_url.split("/")[-1].split("?")[0] or "attachment"
                        file = discord.File(io.BytesIO(data), filename=filename)
            except Exception:
                pass

        await interaction.followup.send(content=content or "\u200b", file=file)

    @tag_group.command(name="list", description="List all available tags.")
    async def tag_list(self, interaction: discord.Interaction) -> None:
        await interaction.response.defer(ephemeral=True)
        with db() as conn:
            rows = conn.execute("SELECT name FROM tags ORDER BY name").fetchall()

        if not rows:
            await interaction.followup.send("No tags have been created yet.", ephemeral=True)
            return

        names = [row["name"] for row in rows]
        embed = discord.Embed(
            title=f"🏷️ Tags ({len(names)})",
            description=", ".join(f"`{n}`" for n in names),
            color=discord.Color.blurple(),
        )
        await interaction.followup.send(embed=embed, ephemeral=True)

    @tag_group.command(name="add", description="Add a new tag (staff only).")
    @app_commands.describe(
        name="Tag name (used as !name or /tag show name).",
        content="Tag content — supports Discord markdown.",
        attachment_url="Optional URL to an image/file to attach.",
    )
    async def tag_add(
        self,
        interaction: discord.Interaction,
        name: str,
        content: str,
        attachment_url: str = "",
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage tags.", ephemeral=True
            )
            return

        await interaction.response.defer(ephemeral=True)
        name = name.lower().strip()
        with db() as conn:
            conn.execute(
                "INSERT OR REPLACE INTO tags (name, content, attachment) VALUES (?, ?, ?)",
                (name, content, attachment_url or None),
            )

        await interaction.followup.send(f"✅ Tag **{name}** has been added.", ephemeral=True)

    @tag_group.command(name="edit", description="Edit an existing tag (staff only).")
    @app_commands.describe(
        name="Name of the tag to edit.",
        content="New content (leave blank to keep existing).",
        attachment_url="New attachment URL (leave blank to keep existing).",
    )
    async def tag_edit(
        self,
        interaction: discord.Interaction,
        name: str,
        content: str = "",
        attachment_url: str = "",
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage tags.", ephemeral=True
            )
            return

        await interaction.response.defer(ephemeral=True)
        name = name.lower().strip()
        with db() as conn:
            row = conn.execute(
                "SELECT content, attachment FROM tags WHERE name = ?", (name,)
            ).fetchone()
            if row is None:
                await interaction.followup.send(
                    f"❌ No tag named **{name}** was found.", ephemeral=True
                )
                return
            new_content = content or row["content"]
            new_attachment = attachment_url or row["attachment"]
            conn.execute(
                "UPDATE tags SET content = ?, attachment = ? WHERE name = ?",
                (new_content, new_attachment, name),
            )

        await interaction.followup.send(f"✅ Tag **{name}** has been updated.", ephemeral=True)

    @tag_group.command(name="remove", description="Delete a tag (staff only).")
    @app_commands.describe(name="Name of the tag to delete.")
    async def tag_remove(self, interaction: discord.Interaction, name: str) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage tags.", ephemeral=True
            )
            return

        await interaction.response.defer(ephemeral=True)
        name = name.lower().strip()
        with db() as conn:
            cur = conn.execute("DELETE FROM tags WHERE name = ?", (name,))

        if cur.rowcount == 0:
            await interaction.followup.send(
                f"❌ No tag named **{name}** was found.", ephemeral=True
            )
            return
        await interaction.followup.send(f"✅ Tag **{name}** has been removed.", ephemeral=True)

    @tag_group.command(name="alias", description="Create an alias for an existing tag (staff only).")
    @app_commands.describe(
        target="The tag name to alias to.",
        alias="The new alias name.",
    )
    async def tag_alias(
        self, interaction: discord.Interaction, target: str, alias: str
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage tags.", ephemeral=True
            )
            return

        await interaction.response.defer(ephemeral=True)
        target = target.lower().strip()
        alias = alias.lower().strip()

        with db() as conn:
            existing = conn.execute(
                "SELECT 1 FROM tags WHERE name = ?", (target,)
            ).fetchone()
            if existing is None:
                await interaction.followup.send(
                    f"❌ Target tag **{target}** does not exist.", ephemeral=True
                )
                return
            conn.execute(
                "INSERT OR REPLACE INTO tags (name, content, attachment) VALUES (?, ?, NULL)",
                (alias, f"!{target}"),
            )

        await interaction.followup.send(
            f"✅ Alias **{alias}** → **{target}** created.", ephemeral=True
        )


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(Tags(bot))  # type: ignore[arg-type]
