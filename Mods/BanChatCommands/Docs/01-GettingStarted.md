# BanChatCommands – Getting Started

← [Back to index](README.md)

---

## Prerequisites

| Requirement | Version |
|-------------|---------|
| Satisfactory (dedicated server) | `>=416835` |
| SML | `^3.11.3` |
| BanSystem | `^1.0.0` |

BanChatCommands **requires BanSystem**. Install BanSystem first and confirm it is working before adding BanChatCommands.

---

## Installation

1. Copy the `BanChatCommands` mod folder into your server's `Mods/` directory:
   ```
   <ServerRoot>/FactoryGame/Mods/BanChatCommands/
   ```
2. Ensure BanSystem is also present in the same `Mods/` directory.
3. Start (or restart) the dedicated server. Both mods are loaded automatically by SML.

---

## Verify it loaded

When the server starts you should see these log lines:

```
LogBanChatCommands: BanChatCommands module starting up.
LogBanChatCommands: BanChatCommands: Registered 9 commands (ban, tempban, unban, bancheck, banlist, linkbans, unlinkbans, playerhistory, whoami).
LogBanChatCommands: BanChatCommands module started.
```

If those lines are absent, check that both `BanChatCommands` and `BanSystem` are in the `Mods/` folder and that the SML version requirement is satisfied.

---

## Quick test

Once the server is running, connect as a player and type:

```
/whoami
```

You should receive a reply such as:

```
[BanChatCommands] Your EOS PUID: 00020aed06f0a6958c3c067fb4b73d51
```

`/whoami` requires no admin privileges and confirms that the mod is loaded and responding.

---

## Next steps

- [Configure admin access →](02-Configuration.md)
- [Learn all commands →](03-Commands.md)
