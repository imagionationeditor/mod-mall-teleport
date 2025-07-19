# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore

## Enhanced Mall Teleport Module

- Latest build status with azerothcore:

[![Build Status](https://github.com/azerothcore/mod-mall-teleport/workflows/core-build/badge.svg?branch=master&event=push)](https://github.com/azerothcore/mod-mall-teleport)

**Enhanced and developed by mojispectre**

An advanced module for AzerothCore that provides teleportation to mall locations with VIP support, cost system, level requirements, and comprehensive configuration options.

## Features

### ✨ Core Features
- **Dual Mall System**: Regular mall (`.M`) and VIP mall (`.vM`) commands
- **VIP Access Control**: Premium account verification through database
- **Cost System**: Configurable gold cost for teleportations
- **Level Requirements**: Minimum level restriction for usage
- **Combat Protection**: Prevents teleportation during combat
- **GM Restrictions**: Optional GM-only access mode

### ⚙️ Configuration System
- **Comprehensive Config File**: All settings configurable via `mod_mall_teleport.conf`
- **Real-time Settings**: No code recompilation needed for changes
- **Flexible Locations**: Customizable teleport destination names
- **Message Control**: Toggle welcome messages and notifications

### 🛡️ Security & Validation
- **Error Handling**: Comprehensive error checking and user feedback
- **Database Validation**: Verifies teleport locations exist
- **Access Control**: Multiple layers of permission checking
- **Safe Teleportation**: Validates teleport success before completion

## Installation

### 1. Database Setup
Apply the SQL file to your **characters** database:
```sql
-- File: data/sql/db-characters/base/PremiumTable.sql
CREATE TABLE IF NOT EXISTS `premium` (
  `AccountId` int(11) NOT NULL,
  `active` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`AccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
```

### 2. Teleport Locations
Create two teleport locations using GM commands:
```
.tele add PlayerMall
.tele add VIPMall
```

### 3. VIP Access
Add VIP accounts to the premium table:
```sql
INSERT INTO premium (AccountId, active) VALUES (ACCOUNT_ID, 1);
```

### 4. Configuration
Copy and customize the configuration file:
```bash
cp conf/mod_mall_teleport.conf.dist conf/mod_mall_teleport.conf
```

## Configuration Options

### Basic Settings
```properties
MallTeleport.Enable = 1                    # Enable/disable module
MallTeleport.RequireLevel = 1              # Minimum level required
MallTeleport.CooldownTime = 0              # Cooldown between teleports (seconds)
```

### Cost System
```properties
MallTeleport.Cost.Enable = 0               # Enable gold cost
MallTeleport.Cost.Gold = 0                 # Regular mall cost (copper)
MallTeleport.Cost.VIPGold = 0              # VIP mall cost (copper)
```

### Security
```properties
MallTeleport.Combat.Block = 1              # Block during combat
MallTeleport.Security.GMOnly = 0           # Restrict to GMs only
MallTeleport.VIP.RequireTable = 1          # Require premium table check
```

### Locations
```properties
MallTeleport.Locations.Regular = "PlayerMall"  # Regular mall location name
MallTeleport.Locations.VIP = "VIPMall"         # VIP mall location name
```

### Messages
```properties
MallTeleport.Messages.Enable = 1          # Enable messages
MallTeleport.Messages.LoginNotice = 1     # Show login notification
```

## Commands

| Command | Description | Access |
|---------|-------------|--------|
| `.M` | Teleport to regular mall | All players (configurable) |
| `.vM` | Teleport to VIP mall | VIP accounts only |

## Error Messages

The module provides clear feedback for various scenarios:
- **Insufficient Level**: "You must be at least level X to use mall teleport."
- **Insufficient Gold**: "You need X gold to use this teleport."
- **No VIP Access**: "You do not have VIP access to use this command."
- **In Combat**: "You cannot teleport while in combat."
- **Location Not Found**: "Teleport location 'X' not found. Please contact an administrator."

## Development

### Enhanced by mojispectre

This module has been significantly enhanced from the original version with the following improvements:

#### 🔧 Technical Improvements
- **Modern C++ Structure**: Clean, maintainable code architecture
- **Configuration System**: Comprehensive config file support
- **Error Handling**: Robust error checking and user feedback
- **Modular Design**: Separated concerns with helper functions
- **Database Safety**: Proper query handling and validation

#### 🚀 Feature Additions
- **Cost System**: Configurable gold costs for teleportations
- **Level Requirements**: Minimum level restrictions
- **Enhanced Security**: Multiple permission layers
- **Flexible Messaging**: Configurable notification system
- **Location Flexibility**: Customizable teleport destination names

#### 🛠️ Code Quality
- **Clean Architecture**: Well-organized class structure
- **Documentation**: Comprehensive inline comments
- **Standards Compliance**: Following AzerothCore coding standards
- **Performance**: Optimized database queries and logic flow

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## License

This project is licensed under the GNU AGPL v3 License - see the [LICENSE](LICENSE) file for details.

## Support

If you encounter any issues or have questions:
1. Check the configuration file settings
2. Verify database setup is correct
3. Ensure teleport locations exist in `game_tele` table
4. Check server logs for error messages

