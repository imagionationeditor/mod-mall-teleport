#include "AccountMgr.h"
#include "Chat.h"
#include "Configuration/Config.h"
#include "Creature.h"
#include "DataMap.h"
#include "Define.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "DatabaseEnv.h"

// Configuration values
struct MallTeleportConfig
{
    bool moduleEnabled;
    uint32 requiredLevel;
    uint32 cooldownTime;
    bool costEnabled;
    uint32 regularCost;
    uint32 vipCost;
    bool blockInCombat;
    std::string regularLocation;
    std::string vipLocation;
    bool messagesEnabled;
    bool loginNotice;
    bool gmOnly;
    bool vipRequireTable;
} sConfig;

void LoadMallTeleportConfig()
{
    sConfig.moduleEnabled = sConfigMgr->GetOption<bool>("MallTeleport.Enable", true);
    sConfig.requiredLevel = sConfigMgr->GetOption<uint32>("MallTeleport.RequireLevel", 1);
    sConfig.cooldownTime = sConfigMgr->GetOption<uint32>("MallTeleport.CooldownTime", 0);
    sConfig.costEnabled = sConfigMgr->GetOption<bool>("MallTeleport.Cost.Enable", false);
    sConfig.regularCost = sConfigMgr->GetOption<uint32>("MallTeleport.Cost.Gold", 0);
    sConfig.vipCost = sConfigMgr->GetOption<uint32>("MallTeleport.Cost.VIPGold", 0);
    sConfig.blockInCombat = sConfigMgr->GetOption<bool>("MallTeleport.Combat.Block", true);
    sConfig.regularLocation = sConfigMgr->GetOption<std::string>("MallTeleport.Locations.Regular", "PlayerMall");
    sConfig.vipLocation = sConfigMgr->GetOption<std::string>("MallTeleport.Locations.VIP", "VIPMall");
    sConfig.messagesEnabled = sConfigMgr->GetOption<bool>("MallTeleport.Messages.Enable", true);
    sConfig.loginNotice = sConfigMgr->GetOption<bool>("MallTeleport.Messages.LoginNotice", true);
    sConfig.gmOnly = sConfigMgr->GetOption<bool>("MallTeleport.Security.GMOnly", false);
    sConfig.vipRequireTable = sConfigMgr->GetOption<bool>("MallTeleport.VIP.RequireTable", true);
}

class MallTeleportPlayer : public PlayerScript
{
public:
    MallTeleportPlayer() : PlayerScript("MallTeleportPlayer", {
        PLAYERHOOK_ON_LOGIN
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!sConfig.moduleEnabled)
            return;

        if (!sConfig.loginNotice || !sConfig.messagesEnabled)
            return;

        ChatHandler(player->GetSession()).SendSysMessage("Mall teleport is available! Use .M for regular mall or .vM for VIP mall.");
    }
};

using namespace Acore::ChatCommands;

class MallTeleport : public CommandScript
{
public:
    MallTeleport() : CommandScript("MallTeleport") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable MallTeleportTable =
        {
            { "M",  HandleMallTeleportCommand, SEC_PLAYER, Console::No },
            { "vM", HandleVIPMallTeleportCommand, SEC_PLAYER, Console::No }
        };

        return MallTeleportTable;
    }

private:
    // Helper function to check basic requirements
    static bool CheckBasicRequirements(ChatHandler* handler, Player* player, const std::string& teleportType)
    {
        if (!sConfig.moduleEnabled)
        {
            handler->SendSysMessage("Mall teleport module is disabled.");
            return false;
        }

        if (!player)
        {
            handler->SendSysMessage("Player not found.");
            return false;
        }

        // Check level requirement
        if (player->GetLevel() < sConfig.requiredLevel)
        {
            handler->PSendSysMessage("You must be at least level %u to use mall teleport.", sConfig.requiredLevel);
            return false;
        }

        // Check combat status
        if (sConfig.blockInCombat && player->IsInCombat())
        {
            handler->SendSysMessage("You cannot teleport while in combat.");
            return false;
        }

        // Check GM restriction
        if (sConfig.gmOnly && !player->IsGameMaster())
        {
            handler->SendSysMessage("This command is restricted to Game Masters only.");
            return false;
        }

        // Check cooldown (basic implementation - could be enhanced with database storage)
        // For now, we'll skip cooldown implementation

        return true;
    }

    // Helper function to check and deduct cost
    static bool CheckAndDeductCost(ChatHandler* handler, Player* player, uint32 cost)
    {
        if (!sConfig.costEnabled || cost == 0)
            return true;

        if (!player->HasEnoughMoney(cost))
        {
            handler->PSendSysMessage("You need %u gold to use this teleport.", cost / 10000);
            return false;
        }

        player->ModifyMoney(-int32(cost));
        handler->PSendSysMessage("You paid %u gold for the teleport.", cost / 10000);
        return true;
    }

    // Helper function to perform teleportation
    static bool PerformTeleport(ChatHandler* handler, Player* player, const std::string& locationName, const std::string& teleportType)
    {
        QueryResult result = WorldDatabase.Query("SELECT `map`, `position_x`, `position_y`, `position_z`, `orientation` FROM `game_tele` WHERE `name`='{}'", locationName);

        if (!result)
        {
            handler->PSendSysMessage("Teleport location '%s' not found. Please contact an administrator.", locationName.c_str());
            return false;
        }

        Field* fields = result->Fetch();
        uint32 map = fields[0].Get<uint32>();
        float position_x = fields[1].Get<float>();
        float position_y = fields[2].Get<float>();
        float position_z = fields[3].Get<float>();
        float orientation = fields[4].Get<float>();

        if (!player->TeleportTo(map, position_x, position_y, position_z, orientation))
        {
            handler->SendSysMessage("Teleportation failed. Please try again.");
            return false;
        }

        return true;
    }

    static bool HandleMallTeleportCommand(ChatHandler* handler, std::string /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (!CheckBasicRequirements(handler, player, "regular"))
            return false;

        if (!CheckAndDeductCost(handler, player, sConfig.regularCost))
            return false;

        return PerformTeleport(handler, player, sConfig.regularLocation, "regular");
    }

    static bool HandleVIPMallTeleportCommand(ChatHandler* handler, std::string /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (!CheckBasicRequirements(handler, player, "VIP"))
            return false;

        // Check VIP access
        if (sConfig.vipRequireTable)
        {
            QueryResult result = CharacterDatabase.Query("SELECT `AccountId` FROM `premium` WHERE `active`=1 AND `AccountId`={}", player->GetSession()->GetAccountId());
            
            if (!result)
            {
                handler->SendSysMessage("You do not have VIP access to use this command.");
                return false;
            }
        }

        if (!CheckAndDeductCost(handler, player, sConfig.vipCost))
            return false;

        return PerformTeleport(handler, player, sConfig.vipLocation, "VIP");
    }
};

void AddMallTeleportScripts()
{
    // Load configuration
    LoadMallTeleportConfig();
    
    new MallTeleportPlayer();
    new MallTeleport();
}
