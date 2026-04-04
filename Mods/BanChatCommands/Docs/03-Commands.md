# BanChatCommands – Commands Reference

← [Back to index](README.md)

---

## Player targeting

Commands that accept `<player|UID>` resolve the argument in this order:

1. **Compound UID** — a string starting with `STEAM:` or `EOS:` is used as-is.
2. **17-digit decimal** — treated as a raw Steam 64-bit ID (`STEAM:` prefix added automatically).
3. **32-character hex string** — treated as a raw EOS Product User ID (`EOS:` prefix added automatically).
4. **Display name** — case-insensitive substring match against currently connected players. If more than one player matches, the command lists all ambiguous matches and asks you to be more specific. Offline players must be targeted by UID.

---

## /ban

```
/ban <player|UID> [reason...]
```

Permanently bans a player. The ban is stored in BanSystem's database and enforced at every future login attempt.

**Requires admin.**

```
/ban SomePlayer Griefing
/ban 76561198000000000 Cheating
/ban STEAM:76561198000000000 Duplicate account
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Cheating
```

---

## /tempban

```
/tempban <player|UID> <minutes> [reason...]
```

Temporarily bans a player for the specified number of minutes. The ban is lifted automatically when the time expires; BanSystem prunes expired bans on startup and periodically at runtime.

**Requires admin.**

```
/tempban SomePlayer 60 Spamming
/tempban 76561198000000000 1440 Toxic behaviour
```

---

## /unban

```
/unban <UID>
```

Removes an existing ban. You must supply the compound UID (`STEAM:xxx` or `EOS:xxx`); display names are not accepted for safety.

**Requires admin.**

```
/unban STEAM:76561198000000000
/unban EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

## /bancheck

```
/bancheck <player|UID>
```

Reports the current ban status for a player. Shows the ban reason, expiry time (for temp bans), and linked UIDs if any.

**Requires admin.**

```
/bancheck SomePlayer
/bancheck STEAM:76561198000000000
```

---

## /banlist

```
/banlist [page]
```

Lists all currently active bans (permanent and unexpired temporary). Results are paginated at 10 entries per page. Pass an optional page number to view further pages.

**Requires admin.**

```
/banlist
/banlist 2
```

---

## /linkbans

```
/linkbans <UID1> <UID2>
```

Links two compound UIDs so that a ban on one also blocks the player when they connect under the other identity. Both UIDs must already have ban records. The link is bidirectional.

**Requires admin.**

```
/linkbans STEAM:76561198000000000 EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

## /unlinkbans

```
/unlinkbans <UID1> <UID2>
```

Removes the bidirectional link between two UIDs that was created by `/linkbans`.

**Requires admin.**

```
/unlinkbans STEAM:76561198000000000 EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

## /playerhistory

```
/playerhistory <name_substring|UID>
```

Searches the BanSystem player session registry. Pass a display name substring to find all UIDs that have connected under that name, or pass a UID to find the display name recorded for it. Returns up to 20 results.

Useful when a player reconnects under a new EOS PUID: look up their old display name, find their previous UID, then use `/ban` or `/linkbans` accordingly.

**Requires admin.**

```
/playerhistory SomePlayer
/playerhistory EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

## /whoami

```
/whoami
```

Shows the calling player's own compound UID. No admin required — any connected player can use this command to find the exact UID they need for a ban lookup or to give to a server admin.

**No admin required.**

Example output:
```
[BanChatCommands] Your Steam64: STEAM:76561198000000000
[BanChatCommands] Your EOS PUID: EOS:00020aed06f0a6958c3c067fb4b73d51
```
