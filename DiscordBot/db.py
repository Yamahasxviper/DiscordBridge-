"""
Database module — SQLite via stdlib sqlite3.

Provides a context-manager connection factory, schema initialisation,
and thin helpers for the config table.
"""

from __future__ import annotations

import sqlite3
from contextlib import contextmanager
from pathlib import Path
from typing import Generator, Optional

DB_PATH = Path(__file__).parent / "data" / "bot.db"

_CONFIG_DEFAULTS: dict[str, str] = {
    "prefix": "!",
    "welcome_message": "",
    "latest_info": "",
    "githook_channel": "",
    "levelling_state": "1",
    "base_level_value": "100",
    "level_value_multiplier": "1.5",
    "xp_gain_value": "10",
    "xp_gain_delay": "60",
}


@contextmanager
def db() -> Generator[sqlite3.Connection, None, None]:
    """Yield a sqlite3 connection; commit on success, rollback on error."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()


def init_db() -> None:
    """Create all tables and seed config defaults."""
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)

    with db() as conn:
        conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS config (
                key   TEXT PRIMARY KEY,
                value TEXT
            );

            CREATE TABLE IF NOT EXISTS tags (
                name       TEXT PRIMARY KEY,
                content    TEXT,
                attachment TEXT
            );

            CREATE TABLE IF NOT EXISTS crashes (
                name     TEXT PRIMARY KEY,
                pattern  TEXT NOT NULL,
                response TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS users (
                user_id        INTEGER PRIMARY KEY,
                xp_count       REAL    DEFAULT 0,
                rank           INTEGER DEFAULT 0,
                xp_multiplier  REAL    DEFAULT 1.0,
                accepts_dms    INTEGER DEFAULT 1,
                message_count  INTEGER DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS mediaonly_channels (
                channel_id INTEGER PRIMARY KEY
            );

            CREATE TABLE IF NOT EXISTS rank_roles (
                role_id INTEGER PRIMARY KEY,
                rank    INTEGER NOT NULL
            );
            """
        )

        for key, value in _CONFIG_DEFAULTS.items():
            conn.execute(
                "INSERT OR IGNORE INTO config (key, value) VALUES (?, ?)",
                (key, value),
            )


def get_config(key: str, default: Optional[str] = None) -> Optional[str]:
    """Return the value for *key* from the config table, or *default*."""
    with db() as conn:
        row = conn.execute(
            "SELECT value FROM config WHERE key = ?", (key,)
        ).fetchone()
    return row["value"] if row else default


def set_config(key: str, value: str) -> None:
    """Insert or replace a config entry."""
    with db() as conn:
        conn.execute(
            "INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)",
            (key, value),
        )
