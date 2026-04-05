# BanSystem – UID Linking

← [Back to index](README.md)

On CSS Dedicated Server, all players are identified exclusively by their **EOS Product User ID** regardless of which launcher they used (Steam, Epic Games Store, etc.). BanSystem stores each compound UID (`EOS:xxx`) in its ban database.

In rare cases a player may end up with more than one EOS PUID — for example after an account migration or when deliberately creating a secondary account to evade a ban. **UID linking** solves this by associating two compound UIDs so that a ban on either one also blocks the other.

---

## How it works

When `IsCurrentlyBannedByAnyId()` is called at login time (the enforcement path), it checks:

1. The player's compound UID against every primary `Uid` field in the database.
2. The player's compound UID against every `LinkedUids` array in the database.

If any match is found, the player is kicked — regardless of which identity triggered the original ban.

---

## Linking two UIDs

Use `/linkbans` in-game (requires admin) or call the REST API to link UIDs:

```
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:00020aed06f0a6958c3c067fb4b73d52
```

Both UIDs must already have their own ban records before linking. The link is stored on **both** records so enforcement works in both directions.

### Typical workflow

1. Ban the player by name while they are online:
   ```
   /ban SomePlayer Cheating
   ```
2. The player reconnects under a new EOS PUID. Use `/playerhistory` to find the new UID:
   ```
   /playerhistory SomePlayer
   ```
3. Ban the new UID:
   ```
   /ban EOS:00020aed06f0a6958c3c067fb4b73d52 Cheating (ban evasion)
   ```
4. Link the two bans so either identity is blocked:
   ```
   /linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:00020aed06f0a6958c3c067fb4b73d52
   ```

---

## Removing a link

```
/unlinkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:00020aed06f0a6958c3c067fb4b73d52
```

This removes the link from both records. The underlying bans themselves are not removed — use `/unban` separately if you want to lift them.

---

## Viewing linked UIDs

`/bancheck <UID>` shows the full ban record including the `LinkedUids` list:

```
/bancheck EOS:00020aed06f0a6958c3c067fb4b73d51
```

The REST endpoint `GET /bans` also includes `LinkedUids` in every ban record.

---

## Player session registry

The `player_sessions.json` file records every unique compound UID and display name seen at login. Use `/playerhistory` to search it — it helps you correlate a player's past identities without needing them to be online:

```
/playerhistory SomePlayer
```

Returns up to 20 matches sorted by most recently seen, showing the compound UID and the last join timestamp for each.

---

## UID format reference

| Platform | Compound UID format | Raw ID format |
|----------|--------------------|--------------------|
| EOS | `EOS:<32-char hex>` | `00020aed06f0a6958c3c067fb4b73d51` |

The `/whoami` command shows a player's compound UID in the correct format.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
