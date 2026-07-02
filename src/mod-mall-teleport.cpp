#include "Chat.h"
#include "Configuration/Config.h"
#include "Define.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "DatabaseEnv.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <ctime>

// ============================================================
//  Config
// ============================================================
struct MallTeleportConfig
{
    bool     moduleEnabled;
    uint32   requiredLevel;
    uint32   cooldownTime;      // seconds; 0 = no cooldown
    bool     costEnabled;       // when enabled, bypassing an active cooldown costs gold
    uint32   regularBypassCost; // copper cost to bypass cooldown for regular mall
    uint32   vipBypassCost;     // copper cost to bypass cooldown for VIP mall
    bool     blockInCombat;
    std::string regularLocation;  // "name" OR "map:x:y:z:o"
    std::string vipLocation;      // "name" OR "map:x:y:z:o"
    bool     messagesEnabled;
    bool     loginNotice;
    bool     gmOnly;
    bool     vipRequireTable;
} sConfig;

void LoadMallTeleportConfig()
{
    sConfig.moduleEnabled      = sConfigMgr->GetOption<bool>       ("MallTeleport.Enable",               true);
    sConfig.requiredLevel      = sConfigMgr->GetOption<uint32>     ("MallTeleport.RequireLevel",          1);
    sConfig.cooldownTime       = sConfigMgr->GetOption<uint32>     ("MallTeleport.CooldownTime",          0);
    sConfig.costEnabled        = sConfigMgr->GetOption<bool>       ("MallTeleport.Cost.Enable",           false);
    sConfig.regularBypassCost  = sConfigMgr->GetOption<uint32>     ("MallTeleport.Cost.Gold",             0);
    sConfig.vipBypassCost      = sConfigMgr->GetOption<uint32>     ("MallTeleport.Cost.VIPGold",          0);
    sConfig.blockInCombat      = sConfigMgr->GetOption<bool>       ("MallTeleport.Combat.Block",          true);
    sConfig.regularLocation    = sConfigMgr->GetOption<std::string>("MallTeleport.Locations.Regular",     "PlayerMall");
    sConfig.vipLocation        = sConfigMgr->GetOption<std::string>("MallTeleport.Locations.VIP",         "VIPMall");
    sConfig.messagesEnabled    = sConfigMgr->GetOption<bool>       ("MallTeleport.Messages.Enable",       true);
    sConfig.loginNotice        = sConfigMgr->GetOption<bool>       ("MallTeleport.Messages.LoginNotice",  true);
    sConfig.gmOnly             = sConfigMgr->GetOption<bool>       ("MallTeleport.Security.GMOnly",       false);
    sConfig.vipRequireTable    = sConfigMgr->GetOption<bool>       ("MallTeleport.VIP.RequireTable",      false);
}

// ============================================================
//  Cooldown tracking  (accountId -> timestamp of last teleport)
// ============================================================
static std::unordered_map<uint32, time_t> sRegularCooldown;
static std::unordered_map<uint32, time_t> sVipCooldown;

// Returns remaining cooldown in seconds (0 = ready).
static uint32 GetRemainingCooldown(std::unordered_map<uint32, time_t>& map, uint32 accountId)
{
    auto it = map.find(accountId);
    if (it == map.end())
        return 0;

    time_t elapsed = std::time(nullptr) - it->second;
    if (elapsed >= static_cast<time_t>(sConfig.cooldownTime))
        return 0;

    return static_cast<uint32>(sConfig.cooldownTime - elapsed);
}

static void SetCooldown(std::unordered_map<uint32, time_t>& map, uint32 accountId)
{
    map[accountId] = std::time(nullptr);
}

// ============================================================
//  Location resolution
//  Accepts either:
//    "PlayerMall"           → looks up game_tele by name
//    "571:5765.4:637.5:647.4:3.58"  → uses raw coordinates
// ============================================================
struct TeleportLocation
{
    bool    valid = false;
    uint32  map   = 0;
    float   x     = 0.f;
    float   y     = 0.f;
    float   z     = 0.f;
    float   o     = 0.f;
};

static TeleportLocation ResolveLocation(ChatHandler* handler, std::string const& locationStr)
{
    TeleportLocation loc;

    // Direct coordinate format?  Expect exactly 4 colons: map:x:y:z:o
    if (std::count(locationStr.begin(), locationStr.end(), ':') == 4)
    {
        std::istringstream ss(locationStr);
        std::string token;
        try
        {
            std::getline(ss, token, ':'); loc.map = static_cast<uint32>(std::stoul(token));
            std::getline(ss, token, ':'); loc.x   = std::stof(token);
            std::getline(ss, token, ':'); loc.y   = std::stof(token);
            std::getline(ss, token, ':'); loc.z   = std::stof(token);
            std::getline(ss, token, ':'); loc.o   = std::stof(token);
            loc.valid = true;
        }
        catch (...)
        {
            handler->PSendSysMessage("Invalid coordinate format: '{}'.", locationStr);
        }
        return loc;
    }

    // Otherwise look up in game_tele table by name
    QueryResult result = WorldDatabase.Query(
        "SELECT `map`, `position_x`, `position_y`, `position_z`, `orientation` "
        "FROM `game_tele` WHERE `name`='{}'",
        locationStr);

    if (!result)
    {
        handler->PSendSysMessage("Teleport location '{}' not found. Please contact an administrator.", locationStr);
        return loc;
    }

    Field* fields = result->Fetch();
    loc.map   = fields[0].Get<uint32>();
    loc.x     = fields[1].Get<float>();
    loc.y     = fields[2].Get<float>();
    loc.z     = fields[3].Get<float>();
    loc.o     = fields[4].Get<float>();
    loc.valid = true;
    return loc;
}

// ============================================================
//  Player script  (login notice)
// ============================================================
class MallTeleportPlayer : public PlayerScript
{
public:
    MallTeleportPlayer() : PlayerScript("MallTeleportPlayer", { PLAYERHOOK_ON_LOGIN }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!sConfig.moduleEnabled || !sConfig.loginNotice || !sConfig.messagesEnabled)
            return;

        ChatHandler(player->GetSession()).SendSysMessage(
            "Mall teleport is available! Use .M for regular mall or .vM for VIP mall.");
    }
};

// ============================================================
//  Command script
// ============================================================
using namespace Acore::ChatCommands;

class MallTeleport : public CommandScript
{
public:
    MallTeleport() : CommandScript("MallTeleport") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable table =
        {
            { "M",  HandleMallTeleportCommand,    SEC_PLAYER, Console::No },
            { "vM", HandleVIPMallTeleportCommand, SEC_PLAYER, Console::No }
        };
        return table;
    }

private:
    // ----------------------------------------------------------
    //  Shared pre-flight checks (level, combat, GM restriction)
    // ----------------------------------------------------------
    static bool CheckBasicRequirements(ChatHandler* handler, Player* player)
    {
        if (!sConfig.moduleEnabled)
        {
            handler->SendSysMessage("Mall teleport module is disabled.");
            return false;
        }

        if (player->GetLevel() < sConfig.requiredLevel)
        {
            handler->PSendSysMessage("You must be at least level {} to use mall teleport.", sConfig.requiredLevel);
            return false;
        }

        if (sConfig.blockInCombat && player->IsInCombat())
        {
            handler->SendSysMessage("You cannot teleport while in combat.");
            return false;
        }

        if (sConfig.gmOnly && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
        {
            handler->SendSysMessage("This command is restricted to Game Masters only.");
            return false;
        }

        return true;
    }

    // ----------------------------------------------------------
    //  Cooldown + cost logic
    //
    //  Rules:
    //    - Cooldown expired (or disabled) → teleport FREE, reset cooldown.
    //    - Cooldown active + cost enabled + player has enough gold
    //        → allow bypass, deduct gold AFTER successful teleport.
    //    - Cooldown active + cost disabled → deny (must wait).
    //    - Cooldown active + cost enabled + not enough gold → deny.
    //
    //  Returns: whether the teleport should proceed.
    //  Out param `costToPay`: copper amount to deduct after teleport (0 = free).
    // ----------------------------------------------------------
    static bool CheckCooldownAndCost(
        ChatHandler* handler,
        Player* player,
        std::unordered_map<uint32, time_t>& cooldownMap,
        uint32 bypassCost,
        uint32& costToPay)
    {
        costToPay = 0;

        if (sConfig.cooldownTime == 0)
            return true;  // cooldown disabled entirely

        uint32 accountId  = player->GetSession()->GetAccountId();
        uint32 remaining  = GetRemainingCooldown(cooldownMap, accountId);

        if (remaining == 0)
            return true;  // cooldown expired → free teleport

        // Cooldown still active
        if (!sConfig.costEnabled || bypassCost == 0)
        {
            handler->PSendSysMessage("You must wait {} more second(s) before teleporting again.", remaining);
            return false;
        }

        // Player wants to bypass with gold
        if (!player->HasEnoughMoney(static_cast<int64>(bypassCost)))
        {
            handler->PSendSysMessage(
                "You are on cooldown ({} second(s) remaining). Pay {} gold to bypass, or wait.",
                remaining,
                bypassCost / 10000);
            return false;
        }

        costToPay = bypassCost;
        return true;
    }

    // ----------------------------------------------------------
    //  Core teleport: resolve location, teleport, then apply cost
    // ----------------------------------------------------------
    static bool DoTeleport(
        ChatHandler* handler,
        Player* player,
        std::string const& locationStr,
        std::unordered_map<uint32, time_t>& cooldownMap,
        uint32 costToPay)
    {
        TeleportLocation loc = ResolveLocation(handler, locationStr);
        if (!loc.valid)
            return false;

        if (!player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o))
        {
            handler->SendSysMessage("Teleportation failed. Please try again.");
            return false;
        }

        // Teleport succeeded: now deduct gold (if any) and update cooldown
        if (costToPay > 0)
        {
            player->ModifyMoney(-static_cast<int32>(costToPay));

            if (sConfig.messagesEnabled)
                handler->PSendSysMessage("Cooldown bypassed. {} gold deducted.", costToPay / 10000);
        }

        SetCooldown(cooldownMap, player->GetSession()->GetAccountId());
        return true;
    }

    // ----------------------------------------------------------
    //  .M — Regular mall
    // ----------------------------------------------------------
    static bool HandleMallTeleportCommand(ChatHandler* handler, std::string /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (!CheckBasicRequirements(handler, player))
            return false;

        uint32 costToPay = 0;
        if (!CheckCooldownAndCost(handler, player, sRegularCooldown, sConfig.regularBypassCost, costToPay))
            return false;

        return DoTeleport(handler, player, sConfig.regularLocation, sRegularCooldown, costToPay);
    }

    // ----------------------------------------------------------
    //  .vM — VIP mall
    // ----------------------------------------------------------
    static bool HandleVIPMallTeleportCommand(ChatHandler* handler, std::string /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (!CheckBasicRequirements(handler, player))
            return false;

        if (sConfig.vipRequireTable)
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT `AccountId` FROM `premium` WHERE `active`=1 AND `AccountId`={}",
                player->GetSession()->GetAccountId());

            if (!result)
            {
                handler->SendSysMessage("You do not have VIP access to use this command.");
                return false;
            }
        }

        uint32 costToPay = 0;
        if (!CheckCooldownAndCost(handler, player, sVipCooldown, sConfig.vipBypassCost, costToPay))
            return false;

        return DoTeleport(handler, player, sConfig.vipLocation, sVipCooldown, costToPay);
    }
};

// ============================================================
//  Registration
// ============================================================
void AddMallTeleportScripts()
{
    LoadMallTeleportConfig();
    new MallTeleportPlayer();
    new MallTeleport();
}
