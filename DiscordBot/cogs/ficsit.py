"""
Ficsit cog — ficsit.app GraphQL and Algolia documentation search.

Commands:
  /mod <name>        — search for mods on ficsit.app
  /docsearch <query> — search the modding documentation via Algolia
  /version           — show latest SML version info
"""

from __future__ import annotations

import json
import os
from typing import TYPE_CHECKING, Any

import discord
from discord import app_commands
from discord.ext import commands

if TYPE_CHECKING:
    from bot import ViperBot

FICSIT_API = "https://api.ficsit.app/v2/query"

SEARCH_MODS = (
    '{ getMods(filter: {search: "%s", limit: 5}) '
    "{ mods { id name short_description downloads "
    "authors { user { username } role } "
    "latestVersions { release { version link } } } count } }"
)

SML_QUERY = (
    "{ getSMLVersions(filter: {limit: 1, order: desc, order_by: created_at}) "
    "{ sml_versions { version date satisfactory_version changelog } } }"
)

ALGOLIA_APP_ID_DEFAULT = "2FDCZBLZ1A"
ALGOLIA_API_KEY_DEFAULT = "28531804beda52a04275ecd964db429d"


async def _graphql(session: Any, query: str) -> dict:
    """Execute a GraphQL query against the ficsit.app API."""
    async with session.post(
        FICSIT_API,
        json={"query": query},
        headers={"Content-Type": "application/json"},
    ) as resp:
        resp.raise_for_status()
        return await resp.json()


class Ficsit(commands.Cog):
    """Commands for querying ficsit.app and the modding documentation."""

    def __init__(self, bot: "ViperBot") -> None:
        self.bot = bot

    # ── /mod ──────────────────────────────────────────────────────────────────

    @app_commands.command(name="mod", description="Look up a mod on ficsit.app.")
    @app_commands.describe(name="Mod name or search term.")
    async def mod(self, interaction: discord.Interaction, name: str) -> None:
        await interaction.response.defer()

        escaped = name.replace('"', '\\"')
        try:
            data = await _graphql(self.bot.http_session, SEARCH_MODS % escaped)
        except Exception as exc:
            await interaction.followup.send(
                f"❌ Failed to query ficsit.app: {exc}", ephemeral=True
            )
            return

        result = data.get("data", {}).get("getMods", {})
        mods: list[dict] = result.get("mods", [])

        if not mods:
            await interaction.followup.send(
                f"❌ No mods found for **{name}**.", ephemeral=True
            )
            return

        best = mods[0]
        authors = ", ".join(
            a["user"]["username"] for a in best.get("authors", []) if a.get("user")
        ) or "Unknown"
        release = (best.get("latestVersions") or {}).get("release") or {}
        version = release.get("version", "—")
        link = release.get("link", "")
        mod_url = f"https://ficsit.app/mod/{best['id']}"

        embed = discord.Embed(
            title=best["name"],
            url=mod_url,
            description=best.get("short_description", ""),
            color=discord.Color.orange(),
        )
        embed.add_field(name="Latest Version", value=version, inline=True)
        embed.add_field(name="Downloads", value=f"{best.get('downloads', 0):,}", inline=True)
        embed.add_field(name="Authors", value=authors, inline=True)
        if link:
            embed.add_field(name="Download", value=link, inline=False)

        if len(mods) > 1:
            alternatives = "\n".join(
                f"• [{m['name']}](https://ficsit.app/mod/{m['id']})"
                for m in mods[1:]
            )
            embed.add_field(name="Other results", value=alternatives, inline=False)

        await interaction.followup.send(embed=embed)

    # ── /docsearch ────────────────────────────────────────────────────────────

    @app_commands.command(
        name="docsearch", description="Search the Satisfactory modding documentation."
    )
    @app_commands.describe(query="Search term for the documentation.")
    async def docsearch(self, interaction: discord.Interaction, query: str) -> None:
        await interaction.response.defer()

        app_id = os.getenv("ALGOLIA_APP_ID", ALGOLIA_APP_ID_DEFAULT)
        api_key = os.getenv("ALGOLIA_API_KEY", ALGOLIA_API_KEY_DEFAULT)

        try:
            from algoliasearch.search.client import SearchClientSync

            client = SearchClientSync(app_id, api_key)
            results = client.search_single_index(
                index_name="ficsit",
                search_params={
                    "query": query,
                    "facetFilters": [
                        "component_name:satisfactory-modding",
                        "component_version:latest",
                    ],
                    "hitsPerPage": 1,
                },
            )
            hits = results.hits if hasattr(results, "hits") else []
        except Exception as exc:
            await interaction.followup.send(
                f"❌ Documentation search failed: {exc}", ephemeral=True
            )
            return

        if not hits:
            await interaction.followup.send(
                "No results found in the modding documentation.", ephemeral=True
            )
            return

        hit = hits[0]
        # Algolia hit objects vary; try attribute access then dict access.
        try:
            url = hit.url  # type: ignore[attr-defined]
        except AttributeError:
            url = hit.get("url", "") if isinstance(hit, dict) else ""

        if not url:
            await interaction.followup.send(
                "No results found in the modding documentation.", ephemeral=True
            )
            return

        try:
            title = hit.hierarchy.lvl0  # type: ignore[attr-defined]
        except AttributeError:
            try:
                title = (hit.get("hierarchy", {}) or {}).get("lvl0", query)  # type: ignore[union-attr]
            except Exception:
                title = query

        embed = discord.Embed(
            title=f"📖 {title}",
            url=url,
            description=f"Best documentation match for **{query}**",
            color=discord.Color.blurple(),
        )
        await interaction.followup.send(embed=embed)

    # ── /version ──────────────────────────────────────────────────────────────

    @app_commands.command(
        name="version", description="Show the latest Satisfactory Mod Loader (SML) version."
    )
    async def version(self, interaction: discord.Interaction) -> None:
        await interaction.response.defer()

        try:
            data = await _graphql(self.bot.http_session, SML_QUERY)
        except Exception as exc:
            await interaction.followup.send(
                f"❌ Failed to query ficsit.app: {exc}", ephemeral=True
            )
            return

        versions: list[dict] = (
            data.get("data", {}).get("getSMLVersions", {}).get("sml_versions", [])
        )
        if not versions:
            await interaction.followup.send(
                "Could not retrieve SML version information.", ephemeral=True
            )
            return

        v = versions[0]
        embed = discord.Embed(
            title="🏭 Satisfactory Mod Loader — Latest Version",
            color=discord.Color.green(),
        )
        embed.add_field(name="SML Version", value=v.get("version", "—"), inline=True)
        embed.add_field(
            name="Target Satisfactory CL",
            value=str(v.get("satisfactory_version", "—")),
            inline=True,
        )
        embed.add_field(name="Released", value=v.get("date", "—")[:10], inline=True)

        changelog = (v.get("changelog") or "").strip()
        if changelog:
            embed.add_field(
                name="Changelog", value=changelog[:1000], inline=False
            )

        await interaction.followup.send(embed=embed)


async def setup(bot: commands.Bot) -> None:
    await bot.add_cog(Ficsit(bot))  # type: ignore[arg-type]
