# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore

## Mall Teleport Module

[![Build Status](https://github.com/azerothcore/mod-mall-teleport/workflows/core-build/badge.svg?branch=master&event=push)](https://github.com/azerothcore/mod-mall-teleport)

**Rewritten by Mojispectre**

Adds two chat commands, `.M` and `.vM`, that teleport a player to a "mall"
location — a shop hub for vendors, banks, trainers, etc. A regular mall and
a VIP mall are supported separately, each with its own destination,
cooldown, and optional gold bypass cost.

## How it works

- Teleporting after the cooldown has expired (or with cooldown disabled)
  is **always free**.
- If the cooldown is still active, the player is blocked from teleporting
  — unless the cost system is turned on and they can afford to bypass it,
  in which case they teleport immediately and gold is deducted.
- Gold is only ever deducted **after** a successful teleport. Nothing is
  taken up front, and nothing is taken for a free trip.
- Destinations can be set either by `game_tele` name or by raw
  `map:x:y:z:o` coordinates directly in the config file.

## Features

### Core
- `.M` — teleport to the regular mall
- `.vM` — teleport to the VIP mall
- Per-command cooldown, tracked independently for regular vs. VIP
- Minimum level requirement (shared by both commands)
- Combat protection — blocked while the player is in combat
- GM-only mode — optionally restrict both commands to GM accounts

### Cooldown & Cost
- Configurable cooldown duration (or none at all)
- Optional gold-bypass system: pay to skip an active cooldown instead of
  waiting
- Separate bypass prices for the regular mall and the VIP mall
- Cost is charged only on bypass, and only after teleport succeeds — never
  charged for a free (post-cooldown) teleport, never charged up front

### Locations
- Destinations set via `game_tele` entry name, **or**
- Destinations set via raw coordinates (`map:x:y:z:o`) directly in the
  config, no database entry required

### VIP Access (optional, off by default)
- `.vM` can optionally require an active row in the `premium` table
- Currently disabled by default since VIP isn't set up on this server —
  `.vM` behaves the same as `.M` (different destination/cost) until you
  enable `MallTeleport.VIP.RequireTable`

### Messages
- Toggleable login reminder pointing players to `.M` / `.vM`
- Toggleable informational messages (e.g. bypass cost confirmation)
- Clear denial messages for every failure case (level, combat, cooldown,
  gold, GM-only, missing location)

## Installation

### 1. Database setup (only needed if you plan to use VIP access)
Apply the SQL file to your **characters** database:
```sql
-- File: data/sql/db-characters/base/PremiumTable.sql
CREATE TABLE IF NOT EXISTS `premium` (
  `AccountId` int(11) unsigned NOT NULL,
  `active` tinyint(1) unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`AccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
```

### 2. Teleport locations
If you're using named locations (the default), create them in-game while
standing at the target spot:
```
.tele add PlayerMall
.tele add VIPMall
```
Alternatively, skip this step and put raw coordinates directly in the
config (see `MallTeleport.Locations.*` below).

### 3. VIP access (optional)
Only relevant once `MallTeleport.VIP.RequireTable = 1`. Add accounts to
the premium table:
```sql
INSERT INTO premium (AccountId, active) VALUES (ACCOUNT_ID, 1);
```

### 4. Configuration
Copy and customize the configuration file:
```bash
cp conf/mod_mall_teleport.conf.dist conf/mod_mall_teleport.conf
```

## Configuration reference

See `conf/mod_mall_teleport.conf.dist` for the full file with inline
explanations and examples for every option. Summary:

| Option | Default | Description |
|---|---|---|
| `MallTeleport.Enable` | `1` | Master on/off switch |
| `MallTeleport.RequireLevel` | `1` | Minimum level to use `.M` / `.vM` |
| `MallTeleport.CooldownTime` | `0` | Cooldown in seconds; `0` = no cooldown |
| `MallTeleport.Cost.Enable` | `0` | Allow paying gold to bypass an active cooldown |
| `MallTeleport.Cost.Gold` | `0` | Copper cost to bypass regular mall cooldown |
| `MallTeleport.Cost.VIPGold` | `0` | Copper cost to bypass VIP mall cooldown |
| `MallTeleport.Locations.Regular` | `"PlayerMall"` | `game_tele` name or `map:x:y:z:o` |
| `MallTeleport.Locations.VIP` | `"VIPMall"` | `game_tele` name or `map:x:y:z:o` |
| `MallTeleport.Messages.Enable` | `1` | Enable informational chat messages |
| `MallTeleport.Messages.LoginNotice` | `1` | Show reminder on login |
| `MallTeleport.Combat.Block` | `1` | Block teleport while in combat |
| `MallTeleport.Security.GMOnly` | `0` | Restrict commands to GM accounts |
| `MallTeleport.VIP.RequireTable` | `0` | Require `premium` table check for `.vM` |

## Commands

| Command | Description | Access |
|---|---|---|
| `.M` | Teleport to the regular mall | All players (configurable) |
| `.vM` | Teleport to the VIP mall | All players by default; restricted to `premium` accounts if `VIP.RequireTable = 1` |

## Denial messages

- **Module disabled**: "Mall teleport module is disabled."
- **Level too low**: "You must be at least level X to use mall teleport."
- **In combat**: "You cannot teleport while in combat."
- **GM-only**: "This command is restricted to Game Masters only."
- **No VIP access**: "You do not have VIP access to use this command."
- **On cooldown, no bypass available**: "You must wait X more second(s) before teleporting again."
- **On cooldown, not enough gold**: "You are on cooldown (X second(s) remaining). Pay X gold to bypass, or wait."
- **Location missing**: "Teleport location 'X' not found. Please contact an administrator."

## License

This project is licensed under the GNU AGPL v3 License - see the [LICENSE](LICENSE) file for details.

## Support

If you run into issues:
1. Double-check the config file settings
2. If using VIP access, confirm the `premium` table exists and is populated
3. If using named locations, confirm they exist in the `game_tele` table
4. Check the worldserver console/log for startup errors
