"""
FAQ cog — /ask and /faq management commands.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Optional

import discord
from discord import app_commands
from discord.ext import commands
from rapidfuzz import fuzz, process

DATA_PATH = Path(__file__).parent.parent / "data" / "knowledge_base.json"

# Minimum match score (0-100) for fuzzy search to return a result.
MATCH_THRESHOLD = 45

# ──────────────────────────────────────────────────────────────────────────────
# Helpers
# ──────────────────────────────────────────────────────────────────────────────

def _load() -> dict:
    with DATA_PATH.open(encoding="utf-8") as fh:
        return json.load(fh)


def _save(data: dict) -> None:
    with DATA_PATH.open("w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=2, ensure_ascii=False)


def _build_search_corpus(data: dict) -> list[tuple[int, str]]:
    """Return a list of (entry_id, searchable_text) pairs."""
    corpus = []
    for entry in data["entries"]:
        combined = " ".join(
            [entry["question"]]
            + entry.get("keywords", [])
            + [entry["answer"]]
        )
        corpus.append((entry["id"], combined))
    return corpus


def _find_best(query: str, data: dict) -> Optional[dict]:
    corpus = _build_search_corpus(data)
    if not corpus:
        return None

    texts = [text for _, text in corpus]
    result = process.extractOne(
        query,
        texts,
        scorer=fuzz.WRatio,
        score_cutoff=MATCH_THRESHOLD,
    )
    if result is None:
        return None

    _, score, idx = result
    matched_id = corpus[idx][0]
    for entry in data["entries"]:
        if entry["id"] == matched_id:
            return entry
    return None


def _is_staff(member: discord.Member, staff_role_name: str) -> bool:
    role_name_lower = staff_role_name.lower()
    for role in member.roles:
        if role.name.lower() == role_name_lower or str(role.id) == staff_role_name:
            return True
    return False


# ──────────────────────────────────────────────────────────────────────────────
# Cog
# ──────────────────────────────────────────────────────────────────────────────

class FAQ(commands.Cog):
    """Satisfactory / modding Q&A commands."""

    def __init__(self, bot: commands.Bot) -> None:
        self.bot = bot
        self.staff_role: str = os.getenv("STAFF_ROLE", "Staff")

    # ── /ask ──────────────────────────────────────────────────────────────────

    @app_commands.command(
        name="ask",
        description="Ask a question about Satisfactory or its mods.",
    )
    @app_commands.describe(question="Your question about Satisfactory or modding.")
    async def ask(self, interaction: discord.Interaction, question: str) -> None:
        data = _load()
        entry = _find_best(question, data)

        if entry is None:
            embed = discord.Embed(
                title="❓ No answer found",
                description=(
                    f"I couldn't find a good match for **\"{question}\"**.\n\n"
                    "Try rephrasing, or check the resources below:\n"
                    "• Modding docs: https://docs.ficsit.app/satisfactory-modding/latest/\n"
                    "• Mod repository: https://ficsit.app\n"
                    "• Modding Discord: https://discord.ficsit.app"
                ),
                color=discord.Color.orange(),
            )
            await interaction.response.send_message(embed=embed, ephemeral=True)
            return

        embed = discord.Embed(
            title=entry["question"],
            description=entry["answer"],
            color=discord.Color.green(),
        )
        embed.set_footer(text=f"Entry #{entry['id']} • Use /faq list to see all topics")
        await interaction.response.send_message(embed=embed)

    # ── /faq group ────────────────────────────────────────────────────────────

    faq_group = app_commands.Group(
        name="faq",
        description="Manage the Satisfactory Q&A knowledge base (staff only).",
    )

    @faq_group.command(name="add", description="Add a new FAQ entry.")
    @app_commands.describe(
        question="The question to add.",
        answer="The answer text (supports Discord markdown).",
        keywords="Comma-separated keywords to improve search (optional).",
    )
    async def faq_add(
        self,
        interaction: discord.Interaction,
        question: str,
        answer: str,
        keywords: str = "",
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage FAQ entries.",
                ephemeral=True,
            )
            return

        data = _load()
        kw_list = [k.strip().lower() for k in keywords.split(",") if k.strip()]
        entry = {
            "id": data["next_id"],
            "keywords": kw_list,
            "question": question,
            "answer": answer,
        }
        data["entries"].append(entry)
        data["next_id"] += 1
        _save(data)

        embed = discord.Embed(
            title="✅ FAQ entry added",
            color=discord.Color.green(),
        )
        embed.add_field(name="ID", value=str(entry["id"]), inline=True)
        embed.add_field(name="Question", value=question, inline=False)
        embed.add_field(name="Answer", value=answer[:500], inline=False)
        await interaction.response.send_message(embed=embed, ephemeral=True)

    @faq_group.command(name="edit", description="Edit an existing FAQ entry.")
    @app_commands.describe(
        entry_id="The numeric ID of the entry to edit (use /faq list to find it).",
        question="New question text (leave blank to keep existing).",
        answer="New answer text (leave blank to keep existing).",
        keywords="New comma-separated keywords (leave blank to keep existing).",
    )
    async def faq_edit(
        self,
        interaction: discord.Interaction,
        entry_id: int,
        question: str = "",
        answer: str = "",
        keywords: str = "",
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage FAQ entries.",
                ephemeral=True,
            )
            return

        data = _load()
        target = next((e for e in data["entries"] if e["id"] == entry_id), None)
        if target is None:
            await interaction.response.send_message(
                f"❌ No entry found with ID **{entry_id}**.", ephemeral=True
            )
            return

        if question:
            target["question"] = question
        if answer:
            target["answer"] = answer
        if keywords:
            target["keywords"] = [k.strip().lower() for k in keywords.split(",") if k.strip()]

        _save(data)
        embed = discord.Embed(
            title=f"✅ Entry #{entry_id} updated",
            color=discord.Color.green(),
        )
        embed.add_field(name="Question", value=target["question"], inline=False)
        embed.add_field(name="Answer", value=target["answer"][:500], inline=False)
        await interaction.response.send_message(embed=embed, ephemeral=True)

    @faq_group.command(name="remove", description="Delete an FAQ entry.")
    @app_commands.describe(entry_id="The numeric ID of the entry to delete.")
    async def faq_remove(
        self, interaction: discord.Interaction, entry_id: int
    ) -> None:
        if not isinstance(interaction.user, discord.Member) or not _is_staff(
            interaction.user, self.staff_role
        ):
            await interaction.response.send_message(
                "❌ You need the **Staff** role to manage FAQ entries.",
                ephemeral=True,
            )
            return

        data = _load()
        before = len(data["entries"])
        data["entries"] = [e for e in data["entries"] if e["id"] != entry_id]

        if len(data["entries"]) == before:
            await interaction.response.send_message(
                f"❌ No entry found with ID **{entry_id}**.", ephemeral=True
            )
            return

        _save(data)
        await interaction.response.send_message(
            f"✅ Entry **#{entry_id}** has been removed.", ephemeral=True
        )

    @faq_group.command(name="list", description="List all FAQ topics with their IDs.")
    async def faq_list(self, interaction: discord.Interaction) -> None:
        data = _load()
        entries = data["entries"]

        if not entries:
            await interaction.response.send_message(
                "The knowledge base is empty. Use `/faq add` to add entries.",
                ephemeral=True,
            )
            return

        # Paginate: show up to 25 entries per embed field block.
        lines = [f"**#{e['id']}** — {e['question']}" for e in entries]
        chunks: list[str] = []
        chunk: list[str] = []
        length = 0
        for line in lines:
            if length + len(line) > 900:
                chunks.append("\n".join(chunk))
                chunk = []
                length = 0
            chunk.append(line)
            length += len(line) + 1
        if chunk:
            chunks.append("\n".join(chunk))

        embed = discord.Embed(
            title=f"📚 FAQ — {len(entries)} topics",
            description=f"Use `/ask <question>` to get an answer.\n\n{chunks[0]}",
            color=discord.Color.blurple(),
        )
        for extra in chunks[1:]:
            embed.add_field(name="\u200b", value=extra, inline=False)

        await interaction.response.send_message(embed=embed, ephemeral=True)


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(FAQ(bot))
