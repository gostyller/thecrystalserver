//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
#include "otpch.h"

#include <string>
#include <sstream>
#include <utility>
#include <iostream>
#include <fstream>

#include <boost/config.hpp>
#include <boost/version.hpp>

#include "commands.h"
#include "player.h"
#include "npc.h"
#include "monsters.h"
#include "game.h"
#include "actions.h"
#include "house.h"
#include "iomanager.h"
#include "tools.h"
#include "ioban.h"
#include "configmanager.h"
#include "town.h"
#include "spells.h"
#include "talkaction.h"
#include "movement.h"
#include "spells.h"
#include "weapons.h"
#include "raids.h"
#include "globalevent.h"
#include "chat.h"
#include "teleport.h"
#include "quests.h"
#include "game.h"
#include "mounts.h"

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
#include "outputmessage.h"
#include "connection.h"
#include "status.h"
#include "protocollogin.h"
#include "manager.h"
#include "protocolold.h"
#endif

#ifdef __LOGIN_SERVER__
#include "gameservers.h"
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

extern ConfigManager g_config;
extern Actions* g_actions;
extern Monsters g_monsters;
extern Npcs g_npcs;
extern TalkActions* g_talkActions;
extern MoveEvents* g_moveEvents;
extern Spells* g_spells;
extern Weapons* g_weapons;
extern Game g_game;
extern Chat g_chat;
extern CreatureEvents* g_creatureEvents;
extern GlobalEvents* g_globalEvents;

extern bool readXMLInteger(xmlNodePtr p, const char* tag, int32_t &value);

#define ipText(a) (uint32_t)a[0] << "." << (uint32_t)a[1] << "." << (uint32_t)a[2] << "." << (uint32_t)a[3]

s_defcommands Commands::defined_commands[] =
{
 	#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	{"/serverdiag",&Commands::serverDiag},
 	#endif
 	#ifdef __TCS_VIP__
 	{"/vip", &Commands::addVipDays},
 	{"!vip", &Commands::showVipDays},
 	#endif
 	{"/premium", &Commands::addPremiumDays},
	{"/reload", &Commands::reloadInfo},
	{"/ghost", &Commands::ghost},
	{"/attr", &Commands::changeThingProporties},
 	{"/baninfo", &Commands::banishmentInfo},
    {"/pvp",&Commands::setWorldType},
	{"/bless",&Commands::addBlessing},
	{"/addon",&Commands::addAddon},
	{"/check",&Commands::mcCheck},
	{"/save",&Commands::saveServer},
	{"/skill",&Commands::skillsPlayers},
	{"!buyhouse", &Commands::buyHouse},
 	{"!sellhouse", &Commands::sellHouse},
 	{"!createguild", &Commands::createGuild},
 	{"!joinguild", &Commands::joinGuild},
 	{"!info", &Commands::shoftware},
    {"!pvp",&Commands::checkWorldType},
    {"!r",&Commands::sendBugReport},
	{"!exp",&Commands::showExpForLvl},
	{"!mana",&Commands::showManaForLvl}
};

Commands::Commands()
{
	loaded = false;

	//setup command map
	for(uint32_t i = 0; i < sizeof(defined_commands) / sizeof(defined_commands[0]); i++)
	{
		Command* cmd = new Command;
		cmd->loadedAccess = false;
		cmd->accessLevel = 0;
		cmd->loadedLogging = false;
		cmd->logged = true;
		cmd->f = defined_commands[i].f;
		std::string key = defined_commands[i].name;
		commandMap[key] = cmd;
	}
}

bool Commands::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FILE_TYPE_XML, "commands.xml").c_str());
	if(!doc)
	{
		std::cout << "[Warning - Commands::loadFromXml] Cannot load commands file." << std::endl;
		std::cout << getLastXMLError() << std::endl;
		return false;
	}

		loaded = true;
		xmlNodePtr root, p;
		std::string strCmd;

		root = xmlDocGetRootElement(doc);
		if(xmlStrcmp(root->name,(const xmlChar*)"commands") != 0)
		{
			xmlFreeDoc(doc);
			return false;
		}

		p = root->children;
		while(p)
		{
			if(xmlStrcmp(p->name, (const xmlChar*)"command") == 0)
			{
				if(readXMLString(p, "cmd", strCmd))
				{
					int32_t intValue;
					std::string strValue;

					CommandMap::iterator it = commandMap.find(strCmd);
					if(it != commandMap.end())
					{
						if(readXMLInteger(p, "access", intValue))
						{
							if(!it->second->loadedAccess)
							{
								it->second->accessLevel = intValue;
								it->second->loadedAccess = true;
							}
							else
								std::cout << "Duplicated access tag for " << strCmd << std::endl;
						}
						else
							std::cout << "Missing access tag for " << strCmd << std::endl;

						if(readXMLString(p, "log", strValue))
						{
							if(!it->second->loadedLogging)
							{
								it->second->logged = booleanString(strValue);
								it->second->loadedLogging = true;
							}
							else
								std::cout << "Duplicated log tag for " << strCmd << std::endl;
						}
						else
							std::cout << "Missing log tag for " << strCmd << std::endl;
					}
					else
						std::cout << "Unknown command " << strCmd << std::endl;
				}
				else
					std::cout << "Missing cmd." << std::endl;
			}
			p = p->next;
		}
		xmlFreeDoc(doc);

	for(CommandMap::iterator it = commandMap.begin(); it != commandMap.end(); ++it)
	{
		if(!it->second->loadedAccess)
			std::cout << "Warning: Missing access for command " << it->first << std::endl;

		if(!it->second->loadedLogging)
			std::cout << "Warning: Missing log command " << it->first << std::endl;

		g_game.addCommandTag(it->first.substr(0, 1));
	}
	return loaded;
}

bool Commands::reload()
{
	loaded = false;
	for(CommandMap::iterator it = commandMap.begin(); it != commandMap.end(); ++it)
	{
		it->second->accessLevel = 0;
		it->second->loadedAccess = false;
		it->second->logged = true;
		it->second->loadedLogging = false;
	}
	g_game.resetCommandTag();
	return loadFromXml();
}

bool Commands::exeCommand(Creature* creature, const std::string& cmd)
{
	std::string str_command;
	std::string str_param;

	std::string::size_type loc = cmd.find(' ', 0 );
	if(loc != std::string::npos && loc >= 0)
	{
		str_command = std::string(cmd, 0, loc);
		str_param = std::string(cmd, (loc + 1), cmd.size() - loc - 1);
	}
	else
	{
		str_command = cmd;
		str_param = std::string("");
	}

	//find command
	CommandMap::iterator it = commandMap.find(str_command);
	if(it == commandMap.end())
		return false;

	Player* player = creature->getPlayer();
	if(player && (it->second->accessLevel > player->getAccess() || player->isAccountManager()))
	{
		if(player->getAccess() > 0)
			player->sendTextMessage(MSG_STATUS_SMALL, "You can not execute this command.");

		return false;
	}

	//execute command
	CommandFunc cfunc = it->second->f;
	(this->*cfunc)(creature, str_command, str_param);
	if(player && it->second->logged)
	{
		player->sendTextMessage(MSG_STATUS_CONSOLE_RED, cmd.c_str());

		char buf[21], buffer[100];
		formatDate2(time(NULL), buf);
		sprintf(buffer, "data/logs/commands/%s.log", player->name.c_str());

		FILE* file = fopen(buffer, "a");
		if(file)
		{
			fprintf(file, "[%s] %s\n", buf, cmd.c_str());
			fclose(file);
		}
	}
	return true;
}

bool Commands::reloadInfo(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(player)
	{
		std::string tmpParam = asLowerCaseString(param);
		if(tmpParam == "all" || tmpParam == "todo")
		{
			g_actions->reload();
			g_chat.reload();
			g_config.reload();
			reload();
			g_creatureEvents->reload();
			#ifdef __LOGIN_SERVER__
			GameServers::getInstance()->reload();
			#endif
			g_globalEvents->reload();
			g_game.reloadHighscores();
			g_monsters.reload();
			Mounts::getInstance()->reload();
			g_moveEvents->reload();
			g_npcs.reload();
			Quests::getInstance()->reload();
			Raids::getInstance()->reload();
			Raids::getInstance()->startup();
			g_spells->reload();
			g_monsters.reload();
			g_game.loadExperienceStages();
			g_talkActions->reload();
			Vocations::getInstance()->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded all.");
		}
		else if(tmpParam == "action" || tmpParam == "actions")
		{
			g_actions->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded actions.");
		}
		else if(tmpParam == "chat" || tmpParam == "chats")
		{
			g_chat.reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded chat.");
		}
		else if(tmpParam == "config" || tmpParam == "configuration")
		{
			g_config.reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded config.");
		}
		else if(tmpParam == "command" || tmpParam == "commands")
		{
			reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded commands.");
		}
		else if(tmpParam == "creaturescript" || tmpParam == "creaturescripts" || 
			 tmpParam == "creatureevent" || tmpParam == "creatureevents")
		{
			g_creatureEvents->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded creature scripts.");
		}
		#ifdef __LOGIN_SERVER__
		else if(tmpParam == "gameserver" || tmpParam == "gameservers" || 
			 tmpParam == "gserver" || tmpParam == "gservers")
		{
			GameServers::getInstance()->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded game servers");
		}
		#endif
		else if(tmpParam == "globalevent" || tmpParam == "globalevents")
		{
			g_globalEvents->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded global events.");
		}
		else if(tmpParam == "group" || tmpParam == "groups")
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reload type does not work.");
		else if(tmpParam == "highscore" || tmpParam == "highscores")
		{
			g_game.reloadHighscores();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded highscores.");
		}
		else if(tmpParam == "item" || tmpParam == "items")
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reload type does not work.");
		else if(tmpParam == "monster" || tmpParam == "monsters")
		{
			g_monsters.reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded monsters.");
		}
		else if(tmpParam == "mount" || tmpParam == "mounts")
		{
			Mounts::getInstance()->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded mounts.");
		}
		else if(tmpParam == "move" || tmpParam == "movement" || tmpParam == "movements"
			|| tmpParam == "moveevents" || tmpParam == "moveevent")
		{
			g_moveEvents->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded movements.");
		}
		else if(tmpParam == "npc" || tmpParam == "npcs")
		{
			g_npcs.reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded npcs.");
		}
		else if(tmpParam == "outfit" || tmpParam == "outfits")
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reload type does not work.");
		else if(tmpParam == "quests" || tmpParam == "quest")
		{
			Quests::getInstance()->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded quests.");
		}
		else if(tmpParam == "raid" || tmpParam == "raids")
		{
			Raids::getInstance()->reload();
			Raids::getInstance()->startup();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded raids.");
		}
		else if(tmpParam == "spell" || tmpParam == "spells")
		{
			g_spells->reload();
			g_monsters.reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded spells.");
		}
		else if(tmpParam == "stage" || tmpParam == "stages")
		{
			g_game.loadExperienceStages();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded stages.");
		}
		else if(tmpParam == "talk" || tmpParam == "talkaction" || tmpParam == "talkactions")
		{
			g_talkActions->reload();
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded talk actions.");
		}
		else if(tmpParam == "vocation" || tmpParam == "vocations" || tmpParam == "voc")
		{
			 Vocations::getInstance()->reload();
			 player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded vocations.");
		}
		else if(tmpParam == "weapons" || tmpParam == "weapon" || tmpParam == "weap")
			 player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reload type does not work.");
		else
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reload type not found.");
	}

	return true;
}

bool Commands::sellHouse(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !g_config.getBool(ConfigManager::HOUSE_BUY_AND_SELL))
		return false;

	House* house = Houses::getInstance()->getHouseByPlayerId(player->getGUID());
	if(!house && (!player->getGuildId() || !(house = Houses::getInstance()->getHouseByGuildId(player->getGuildId()))))
	{
		player->sendCancel("You do not rent any flat.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(house->isGuild() && player->getGuildLevel() != GUILDLEVEL_LEADER)
    {
        player->sendCancel("You have to be at least a guild leader to sell this hall.");
        g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
        return false;
    }

    Tile* tile = g_game.getTile(player->getPosition());
    if(!tile || !tile->getHouseTile() || tile->getHouseTile()->getHouse() != house)
    {
        player->sendCancel("You have to be inside a house that you would like to sell.");
        g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
        return false;
    }

	Player* tradePartner = NULL;
	ReturnValue ret = g_game.getPlayerByNameWildcard(param, tradePartner);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(tradePartner == player)
	{
		player->sendCancel("You cannot trade with yourself.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!house->isGuild())
	{
		if(Houses::getInstance()->getHouseByPlayerId(tradePartner->getGUID()))
		{
			player->sendCancel("Trade player already rents another house.");
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		uint16_t housesPerAccount = g_config.getNumber(ConfigManager::HOUSES_PER_ACCOUNT);
		if(housesPerAccount > 0 && Houses::getInstance()->getHousesCount(tradePartner->getAccount()) >= housesPerAccount)
		{
			char buffer[100];
			sprintf(buffer, "Trade player has reached limit of %d house%s per account.", housesPerAccount, (housesPerAccount != 1 ? "s" : ""));

			player->sendCancel(buffer);
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if(!tradePartner->isPremium() && !g_config.getBool(ConfigManager::HOUSE_NEED_PREMIUM))
		{
			player->sendCancel("Trade player does not have a premium account.");
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		uint32_t levelToBuyHouse = g_config.getNumber(ConfigManager::LEVEL_TO_BUY_HOUSE);
		if(tradePartner->getLevel() < levelToBuyHouse)
		{
			char buffer[100];
			sprintf(buffer, "Trade player has to be at least Level %d to buy house.", levelToBuyHouse);

			player->sendCancel(buffer);
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}
	else
	{
		if(!tradePartner->getGuildId() || tradePartner->getGuildLevel() != GUILDLEVEL_LEADER)
		{
			player->sendCancel("Trade player has to be at least a guild leader to buy a hall.");
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if(Houses::getInstance()->getHouseByGuildId(tradePartner->getGuildId()))
		{
			player->sendCancel("Trade player's guild already rents another hall.");
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}

	if(!Position::areInRange<3,3,0>(tradePartner->getPosition(), player->getPosition()))
	{
		player->sendCancel("Trade player is too far away.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!Houses::getInstance()->payRent(player, house, 0))
	{
		player->sendCancel("You have to pay a pre-rent first.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Item* transferItem = TransferItem::createTransferItem(house);
	player->transferContainer.__addThing(NULL, transferItem);

	player->transferContainer.setParent(player);
	if(!g_game.internalStartTrade(player, tradePartner, transferItem))
		transferItem->onTradeEvent(ON_TRADE_CANCEL, player, NULL);

	g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	return false;
}

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
bool Commands::serverDiag(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	std::stringstream s;
	s << "Server diagonostic:" << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, s.str());

	s.str("");
	s << "World:" << std::endl
		<< "--------------------" << std::endl
		<< "Player: " << g_game.getPlayersOnline() << " (" << Player::playerCount << ")" << std::endl
		<< "Npc: " << g_game.getNpcsOnline() << " (" << Npc::npcCount << ")" << std::endl
		<< "Monster: " << g_game.getMonstersOnline() << " (" << Monster::monsterCount << ")" << std::endl << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, s.str());

	s.str("");
	s << "Protocols:" << std::endl
		<< "--------------------" << std::endl
		<< "ProtocolGame: " << ProtocolGame::protocolGameCount << std::endl
		<< "ProtocolLogin: " << ProtocolLogin::protocolLoginCount << std::endl
#ifdef __OTADMIN__
		<< "ProtocolAdmin: " << ProtocolAdmin::protocolAdminCount << std::endl
#endif
		<< "ProtocolManager: " << ProtocolManager::protocolManagerCount << std::endl
		<< "ProtocolStatus: " << ProtocolStatus::protocolStatusCount << std::endl
		<< "ProtocolOld: " << ProtocolOld::protocolOldCount << std::endl << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, s.str());

	s.str("");
	s << "Connections:" << std::endl
		<< "--------------------" << std::endl
		<< "Active connections: " << Connection::connectionCount << std::endl
		<< "Total message pool: " << OutputMessagePool::getInstance()->getTotalMessageCount() << std::endl
		<< "Auto message pool: " << OutputMessagePool::getInstance()->getAutoMessageCount() << std::endl
		<< "Queued message pool: " << OutputMessagePool::getInstance()->getQueuedMessageCount() << std::endl
		<< "Free message pool: " << OutputMessagePool::getInstance()->getAvailableMessageCount() << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, s.str());

#else
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Command not available, please rebuild your software with -D__ENABLE_SERVER_DIAG__");
#endif
	return true;
}

bool Commands::buyHouse(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !g_config.getBool(ConfigManager::HOUSE_BUY_AND_SELL))
		return false;

	const Position& pos = getNextPosition(player->getDirection(), player->getPosition());
	Tile* tile = g_game.getTile(pos);
	if(!tile)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	HouseTile* houseTile = tile->getHouseTile();
	if(!houseTile)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	House* house = houseTile->getHouse();
	if(!house)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!house->getDoorByPosition(pos))
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!house->isGuild())
	{
		if(Houses::getInstance()->getHouseByPlayerId(player->getGUID()))
		{
			player->sendCancel("You already rent another house.");
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		uint16_t accountHouses = g_config.getNumber(ConfigManager::HOUSES_PER_ACCOUNT);
		if(accountHouses > 0 && Houses::getInstance()->getHousesCount(player->getAccount()) >= accountHouses)
		{
			char buffer[80];
			sprintf(buffer, "You may own only %d house%s per account.", accountHouses, (accountHouses != 1 ? "s" : ""));

			player->sendCancel(buffer);
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if(g_config.getBool(ConfigManager::HOUSE_NEED_PREMIUM) && !player->isPremium())
		{
			player->sendCancelMessage(RET_YOUNEEDPREMIUMACCOUNT);
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		uint32_t levelToBuyHouse = g_config.getNumber(ConfigManager::LEVEL_TO_BUY_HOUSE);
		if(player->getLevel() < levelToBuyHouse)
		{
			char buffer[90];
			sprintf(buffer, "You have to be at least Level %d to purchase a house.", levelToBuyHouse);
			player->sendCancel(buffer);
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}
	else
	{
		if(!player->getGuildId() || player->getGuildLevel() != GUILDLEVEL_LEADER)
		{
			player->sendCancel("You have to be at least a guild leader to purchase a hall.");
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if(Houses::getInstance()->getHouseByGuildId(player->getGuildId()))
		{
			player->sendCancel("Your guild rents already another hall.");
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}

	if(house->getOwner())
	{
		player->sendCancel("This flat is already owned by someone else.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if((uint32_t)g_game.getMoney(player) < house->getPrice() || !g_game.removeMoney(player, house->getPrice()))
	{
		player->sendCancel("You do not have enough money.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	house->setOwnerEx(player->getGUID(), true);
	std::string ret = "You have successfully bought this ";
	if(house->isGuild())
		ret += "hall";
	else
		ret += "house";

	ret += ", remember to leave money at ";
	if(house->isGuild())
		ret += "guild owner ";

	if(g_config.getBool(ConfigManager::BANK_SYSTEM))
		ret += "bank or ";

	ret += "depot of this town for rent.";
	player->sendTextMessage(MSG_INFO_DESCR, ret.c_str());

	g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	return false;
}

bool Commands::joinGuild(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !g_config.getBool(ConfigManager::INGAME_GUILD_MANAGEMENT))
		return false;

	std::string param_ = param;
	trimString(param_);
	if(!player->getGuildId())
	{
		uint32_t guildId;
		if(IOGuild::getInstance()->getGuildId(guildId, param_))
		{
			if(player->isGuildInvited(guildId))
			{
				IOGuild::getInstance()->joinGuild(player, guildId);
				player->sendTextMessage(MSG_INFO_DESCR, "You have joined the guild.");

				char buffer[80];
				sprintf(buffer, "%s has joined the guild.", player->getName().c_str());
				if(ChatChannel* guildChannel = g_chat.getChannel(player, 0x00))
					guildChannel->talk(player, SPEAK_CHANNEL_W, buffer);
			}
			else
				player->sendCancel("You are not invited to that guild.");
		}
		else
			player->sendCancel("There's no guild with that name.");
	}
	else
		player->sendCancel("You are already in a guild.");

	return true;
}

bool Commands::createGuild(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !g_config.getBool(ConfigManager::INGAME_GUILD_MANAGEMENT))
		return false;

	if(player->getGuildId())
	{
		player->sendCancel("You are already in a guild.");
		return true;
	}

	std::string param_ = param;
	trimString(param_);
	if(!isValidName(param_))
	{
		player->sendCancel("That guild name contains illegal characters, please choose another name.");
		return true;
	}

	const uint32_t minLength = g_config.getNumber(ConfigManager::MIN_GUILDNAME);
	const uint32_t maxLength = g_config.getNumber(ConfigManager::MAX_GUILDNAME);
	if(param_.length() < minLength)
	{
		player->sendCancel("That guild name is too short, please select a longer name.");
		return true;
	}

	if(param_.length() > maxLength)
	{
		player->sendCancel("That guild name is too long, please select a shorter name.");
		return true;
	}

	uint32_t guildId;
	if(IOGuild::getInstance()->getGuildId(guildId, param_))
	{
		player->sendCancel("There is already a guild with that name.");
		return true;
	}

	const uint32_t levelToFormGuild = g_config.getNumber(ConfigManager::LEVEL_TO_FORM_GUILD);
	if(player->getLevel() < levelToFormGuild)
	{
		char buffer[70 + levelToFormGuild];
		sprintf(buffer, "You have to be at least Level %d to form a guild.", levelToFormGuild);
		player->sendCancel(buffer);
		return true;
	}

	const int32_t premiumDays = g_config.getNumber(ConfigManager::GUILD_PREMIUM_DAYS);
	if(player->getPremiumDays() < premiumDays && !g_config.getBool(ConfigManager::FREE_PREMIUM))
	{
		char buffer[70 + premiumDays];
		sprintf(buffer, "You need to have at least %d premium days to form a guild.", premiumDays);
		player->sendCancel(buffer);
		return true;
	}

	player->setGuildName(param_);
	IOGuild::getInstance()->createGuild(player);

	char buffer[50 + maxLength];
	sprintf(buffer, "You have formed guild \"%s\"!", param_.c_str());
	player->sendTextMessage(MSG_INFO_DESCR, buffer);
	return true;
}

bool Commands::ghost(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	if(player->hasFlag(PlayerFlag_CannotBeSeen))
	{
		player->sendTextMessage(MSG_INFO_DESCR, "Command disabled for players with special, invisibility flag.");
		return true;
	}

	SpectatorVec::iterator it;
	SpectatorVec list = g_game.getSpectators(player->getPosition());
	Player* tmpPlayer = NULL;

	Condition* condition = NULL;
	if((condition = player->getCondition(CONDITION_GAMEMASTER, CONDITIONID_DEFAULT, GAMEMASTER_INVISIBLE)))
	{
		player->sendTextMessage(MSG_INFO_DESCR, "You are visible again.");
		IOManager::getInstance()->updateOnlineStatus(player->getGUID(), true);
		for(AutoList<Player>::iterator pit = Player::autoList.begin(); pit != Player::autoList.end(); ++pit)
		{
			if(!pit->second->canSeeCreature(player))
				pit->second->notifyLogIn(player);
		}

		for(it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()) && !tmpPlayer->canSeeCreature(player))
				tmpPlayer->sendMagicEffect(player->getPosition(), MAGIC_EFFECT_TELEPORT);
		}

		player->removeCondition(condition);
		g_game.internalCreatureChangeVisible(creature, VISIBLE_GHOST_APPEAR);
	}
	else if((condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_GAMEMASTER, -1, 0, false, GAMEMASTER_INVISIBLE)))
	{
		player->addCondition(condition);
		g_game.internalCreatureChangeVisible(creature, VISIBLE_GHOST_DISAPPEAR);
		for(it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()) && !tmpPlayer->canSeeCreature(player))
				tmpPlayer->sendMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		}

		for(AutoList<Player>::iterator pit = Player::autoList.begin(); pit != Player::autoList.end(); ++pit)
		{
			if(!pit->second->canSeeCreature(player))
				pit->second->notifyLogOut(player);
		}

		IOManager::getInstance()->updateOnlineStatus(player->getGUID(), false);
		if(player->isTrading())
			g_game.internalCloseTrade(player);

		player->clearPartyInvitations();
		if(player->getParty())
			player->getParty()->leave(player);

		player->sendTextMessage(MSG_INFO_DESCR, "You are now invisible.");
	}

	return true;
}

bool Commands::changeThingProporties(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	const Position& pos = getNextPosition(player->getDirection(), player->getPosition());
	Tile* tile = g_game.getTile(pos);
	if(!tile)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "No tile found.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return true;
	}

	Thing* thing = tile->getTopVisibleThing(creature);
	if(!thing)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "No object found.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return true;
	}

	boost::char_separator<char> sep(" ");
	tokenizer tokens(param, sep);

	std::string invalid;
	for(tokenizer::iterator it = tokens.begin(); it != tokens.end();)
	{
		std::string action = parseParams(it, tokens.end());
		toLowerCaseString(action);
		if(Item* item = thing->getItem())
		{
			if(action == "set" || action == "add" || action == "new")
			{
				std::string type = parseParams(it, tokens.end()), key = parseParams(it,
					tokens.end()), value = parseParams(it, tokens.end());
				if(type == "integer" || type == "number" || type == "int" || type == "num")
					item->setAttribute(key, atoi(value.c_str()));
				else if(type == "float" || type == "double")
					item->setAttribute(key, (float)atof(value.c_str()));
				else if(type == "bool" || type == "boolean")
					item->setAttribute(key, booleanString(value));
				else
					item->setAttribute(key, value);
			}
			else if(action == "erase" || action == "remove" || action == "delete")
				item->eraseAttribute(parseParams(it, tokens.end()));
			else if(action == "action" || action == "actionid" || action == "aid")
			{
				int32_t tmp = atoi(parseParams(it, tokens.end()).c_str());
				if(tmp > 0)
					item->setActionId(tmp);
				else
					item->resetActionId();
			}
			else if(action == "unique" || action == "uniqueid" || action == "uid")
			{
				int32_t tmp = atoi(parseParams(it, tokens.end()).c_str());
				if(tmp >= 1000 || tmp <= 0xFFFF)
					item->setUniqueId(tmp);
			}
			else if(action == "destination" || action == "position" || action == "pos"
				|| action == "dest" || action == "loc" || action == "location") //TODO: doesn't work
			{
				if(Teleport* teleport = item->getTeleport())
					teleport->setDestination(Position(atoi(parseParams(it, tokens.end()).c_str()), atoi(
						parseParams(it, tokens.end()).c_str()), atoi(parseParams(it, tokens.end()).c_str())));
			}
			else
			{
				std::stringstream s;
				s << action << " (" << parseParams(it, tokens.end()) << ")";
				invalid += s.str();
				break;
			}
		}
		else if(Creature* _creature = thing->getCreature())
		{
			if(action == "health")
				_creature->changeHealth(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "maxhealth")
				_creature->changeMaxHealth(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "mana")
				_creature->changeMana(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "maxmana")
				_creature->changeMaxMana(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "basespeed")
				_creature->setBaseSpeed(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "droploot")
				_creature->setDropLoot((lootDrop_t)atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "lossskill")
				_creature->setLossSkill(booleanString(parseParams(it, tokens.end())));
			else if(action == "cannotmove")
				_creature->setNoMove(booleanString(parseParams(it, tokens.end())));
			else if(action == "skull")
			{
				_creature->setSkull(getSkulls(parseParams(it, tokens.end())));
				g_game.updateCreatureSkull(_creature);
			}
			else if(action == "shield")
			{
				_creature->setShield(getShields(parseParams(it, tokens.end())));
				g_game.updateCreatureShield(_creature);
			}
			else if(action == "emblem")
			{
				_creature->setEmblem(getEmblems(parseParams(it, tokens.end())));
				g_game.updateCreatureEmblem(_creature);
			}
			else if(action == "speaktype")
				_creature->setSpeakType((SpeakClasses)atoi(parseParams(it, tokens.end()).c_str()));
			else if(Player* _player = _creature->getPlayer())
			{
				if(action == "fyi")
					_player->sendFYIBox(parseParams(it, tokens.end()).c_str());
				else if(action == "tutorial")
					_player->sendTutorial(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "guildlevel")
					_player->setGuildLevel((GuildLevel_t)atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "guildrank")
					_player->setRankId(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "guildnick")
					_player->setGuildNick(parseParams(it, tokens.end()).c_str());
				else if(action == "group")
					_player->setGroupId(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "vocation")
					_player->setVocation(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "sex" || action == "gender")
					_player->setSex(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "town" || action == "temple")
				{
					if(Town* town = Towns::getInstance()->getTown(parseParams(it, tokens.end())))
					{
						_player->setMasterPosition(town->getPosition());
						_player->setTown(town->getID());
					}
				}
				else if(action == "marriage" || action == "partner")
					_player->marriage = atoi(parseParams(it, tokens.end()).c_str());
				else if(action == "balance")
					_player->balance = atoi(parseParams(it, tokens.end()).c_str());
				else if(action == "rates")
					_player->rates[atoi(parseParams(it, tokens.end()).c_str())] = atof(
						parseParams(it, tokens.end()).c_str());
				else if(action == "idle")
					_player->setIdleTime(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "stamina")
					_player->setStaminaMinutes(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "capacity" || action == "cap")
					_player->setCapacity(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "execute")
					g_talkActions->onPlayerSay(_player, atoi(parseParams(it, tokens.end()).c_str()),
						parseParams(it, tokens.end()), booleanString(parseParams(it, tokens.end())));
				else if(action == "saving" || action == "save")
					_player->switchSaving();
				else
				{
					std::stringstream s;
					s << action << " (" << parseParams(it, tokens.end()) << ")";
					invalid += s.str();
					break;
				}
			}
			/*else if(Npc* _npc = _creature->getNpc())
			{
			}
			else if(Monster* _monster = _creature->getMonster())
			{
			}*/
			else
			{
				std::stringstream s;
				s << action << " (" << parseParams(it, tokens.end()) << ")";
				invalid += s.str();
				break;
			}
		}
	}

	const SpectatorVec& list = g_game.getSpectators(pos);
	SpectatorVec::const_iterator it;

	Player* tmpPlayer = NULL;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendUpdateTile(tile, pos);
	}

	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onUpdateTile(tile, pos);

	g_game.addMagicEffect(pos, MAGIC_EFFECT_WRAPS_GREEN);
	if(invalid.empty())
		return true;

	std::string tmp = "Following action was invalid: " + invalid;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, tmp.c_str());
	return true;
}

bool Commands::banishmentInfo(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	StringVec params = explodeString(param, ",");
	std::string what = "Account";
	trimString(params[0]);

	Ban ban;
	ban.type = BAN_ACCOUNT;
	if(params.size() > 1)
	{
		trimString(params[1]);
		if(params[0].substr(0, 1) == "p")
		{
			what = "Character";
			ban.type = BAN_PLAYER;
			ban.param = PLAYERBAN_BANISHMENT;

			ban.value = atoi(params[1].c_str());
			if(!ban.value)
			{
				IOManager::getInstance()->getGuidByName(ban.value, params[1], true);
				if(!ban.value)
					ban.value = IOManager::getInstance()->getAccountIdByName(params[1]);
			}
		}
		else
		{
			ban.value = atoi(params[1].c_str());
			if(!ban.value)
			{
				IOManager::getInstance()->getAccountId(params[1], ban.value);
				if(!ban.value)
					ban.value = IOManager::getInstance()->getAccountIdByName(params[1]);
			}
		}
	}
	else
	{
		ban.value = atoi(params[0].c_str());
		if(!ban.value)
		{
			IOManager::getInstance()->getAccountId(params[0], ban.value);
			if(!ban.value)
				ban.value = IOManager::getInstance()->getAccountIdByName(params[0]);
		}
	}

	if(!ban.value)
	{
		toLowerCaseString(what);
		player->sendCancel("Invalid " + what + (std::string)" name or id.");
		return true;
	}

	if(!IOBan::getInstance()->getData(ban))
	{
		player->sendCancel("That player or account is not banished or deleted.");
		return true;
	}

	bool deletion = ban.expires < 0;
	std::string admin = "Automatic ";
	if(!ban.adminId)
		admin += (deletion ? "deletion" : "banishment");
	else
		IOManager::getInstance()->getNameByGuid(ban.adminId, admin, true);

	std::string end = "Banishment will be lifted at:\n";
	if(deletion)
		end = what + (std::string)" won't be undeleted";

	char buffer[500 + ban.comment.length()];
	sprintf(buffer, "%s has been %s at:\n%s by: %s,\nfor the following reason:\n%s.\nThe action taken was:\n%s.\nThe comment given was:\n%s.\n%s%s.",
		what.c_str(), (deletion ? "deleted" : "banished"), formatDateEx(ban.added, "%d %b %Y").c_str(), admin.c_str(), getReason(ban.reason).c_str(),
		getAction(ban.action, false).c_str(), ban.comment.c_str(), end.c_str(), (deletion ? "." : formatDateEx(ban.expires).c_str()));

	player->sendFYIBox(buffer);
	return true;
}

bool Commands::shoftware(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Crystal Server - Version 0.3 (Ice Fenix).");
	return true;
}

bool Commands::addPremiumDays(Creature* creature, const std::string& cmd, const std::string& param)
{
	uint32_t premiumTime = 0;
	std::string name;
	std::istringstream in(param.c_str());

	std::getline(in, name, ',');
	in >> premiumTime;	
	
	Player* player = g_game.getPlayerByName(name);
	if(player)
	{
		if(premiumTime < 0 || premiumTime > 999)
			premiumTime = 1;
		
	uint32_t days = premiumTime;   
     Account account = IOManager::getInstance()->loadAccount(player->getAccount());
		if(player->premiumDays < 65535)
		{
			if(player->premiumDays <= 50000 && days <= 10000)
			{
				account.premiumDays += days;
				player->premiumDays += days;
			}
			IOManager::getInstance()->saveAccount(account);
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
			return true;
		}
	}
    
	return false;
}

#ifdef __TCS_VIP__
bool Commands::addVipDays(Creature* creature, const std::string& cmd, const std::string& param)
{
	uint32_t vipTime = 0;
	std::string name;
	std::istringstream in(param.c_str());

	std::getline(in, name, ',');
	in >> vipTime;	
	
	Player* player = g_game.getPlayerByName(name);
	if(player)
	{
		if(vipTime < 0 || vipTime > 999)
			vipTime = 1;
		
	uint32_t days = vipTime;
	    
     Account account = IOManager::getInstance()->loadAccount(player->getAccount());
		if(player->premiumDays < 65535)
		{
			if(player->vipDays <= 50000 && days <= 10000)
			{
				account.vipDays += days;
				player->vipDays += days;
			}
			
			IOManager::getInstance()->saveAccount(account);
			g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
			return true;
		}
	}
    
	return false;
}

bool Commands::showVipDays(Creature* creature, const std::string& cmd, const std::string& param)
{
	if(Player* player = creature->getPlayer())
	{
         Account account = IOManager::getInstance()->loadAccount(player->getAccount());
         uint32_t vipDays;
         vipDays = account.vipDays;
         
         if(vipDays == 0)
         player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "You do not have a vip account.");
         
         if(vipDays == 1)
         player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "You have only one more day of vip account .");
         
         if(vipDays > 1)
		 {
          	std::stringstream vipDaysMessage;
         	vipDaysMessage << "You have more " <<  vipDays << " days of vip account.";
         	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, vipDaysMessage.str().c_str());
         }         
	}	
}
#endif

bool Commands::setWorldType(Creature* creature, const std::string& cmd, const std::string& param)
{
     Player* player = creature->getPlayer();
     if( !player )
         return false;

     if(param == "" || param == " ")
	 	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Command requiere param.");

          if(param == "pvp" || param == "open" || param == "openpvp" || 
		  param == "open-pvp")
          {
              g_game.setWorldType(WORLDTYPE_OPEN);
              player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type changed to Open PvP.");
          }
          else if(param == "no-pvp" || param == "non-pvp" || param == "nonpvp" || 
		  param == "optional" || param == "safe" || param == "optionalpvp" || 
		  param == "optional-pvp")
          {
               g_game.setWorldType(WORLDTYPE_OPTIONAL);
               player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type changed to Optional PvP.");
          }
          else if(param == "e-pvp" || param == "epvp" || param == "pvp-enforced" || 
		  param == "war" || param == "hardcore" || param == "enforced" || 
		  param == "hardcore-pvp" || param == "pvpe" || param == "pvp-e")
          {
               g_game.setWorldType(WORLDTYPE_HARDCORE);
               player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type changed to Hardcore PvP.");
          }
          
     return true;
}

bool Commands::checkWorldType(Creature* creature, const std::string& cmd, const std::string& param)
{
     Player* player = creature->getPlayer();
     if( !player )
         return false;

          if(g_game.getWorldType() == WORLDTYPE_OPEN)
              player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type is currently set to Open PvP.");

          else if(g_game.getWorldType() == WORLDTYPE_OPTIONAL)
               player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type is currently set to Optional PvP.");

          else if(g_game.getWorldType() == WORLDTYPE_HARDCORE)
               player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type is currently set to Hardcore PvP.");
     
     return true;
}

bool Commands::addBlessing(Creature* creature, const std::string& cmd, const std::string& param)
{
	std::string name;
	int32_t blessId = 0;
	std::istringstream in(param.c_str());

	std::getline(in, name, ',');
	in >> blessId;
    
    if(blessId > 5 || blessId < 1)
    blessId = 1;
    
	Player* player = g_game.getPlayerByName(name);
	if(player)
	{
		player->addBlessing(blessId);
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
		return true;
	}
	
	return true;
}

bool Commands::addAddon(Creature* creature, const std::string &cmd, const std::string &param)
{
	uint32_t lookType, addon = 0;
	std::string name;
	std::istringstream in(param.c_str());

	std::getline(in, name, ',');
	in >> lookType >> addon;	

	Player* player = g_game.getPlayerByName(name);
	if(player)
	{
		if(addon < 0 || addon > 3)
			addon = 3;
		
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
		player->addOutfit(lookType, addon);
		return true;
	}
	
	return true;
}

bool Commands::mcCheck(Creature* creature, const std::string& cmd, const std::string& param)
{
    Player* player = creature->getPlayer();
    std::stringstream info;
    unsigned char ip[4];
        
    if(player)
	{   
        info << "Multi Client List: \n";
        info << "Name          IP" << "\n";
        for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
		{
            Player* lol = (*it).second;                                                   
            for(AutoList<Player>::iterator cit = Player::autoList.begin(); cit != Player::autoList.end(); ++cit)
			{
                if((*cit).second != lol && (*cit).second->lastIP == lol->lastIP)
				{
                    *(unsigned long*)&ip = (*cit).second->lastIP;
                    info << (*cit).second->getName() << "      " << (unsigned int)ip[0] << "." << (unsigned int)ip[1] << 
        			"." << (unsigned int)ip[2] << "." << (unsigned int)ip[3] << "\n";       
                }                                                           
            }
        }      
        player->sendTextMessage(MSG_STATUS_CONSOLE_ORANGE, info.str().c_str());        
    }
    else
        return false;
       
    return true;                                     
}

bool Commands::saveServer(Creature* creature, const std::string& cmd, const std::string& param)
{
 	 Player* player = creature->getPlayer();
 	 
	 g_game.saveGameState(true);
	 player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Save server completed.");
}

bool Commands::sendBugReport(Creature* c, const std::string& cmd, const std::string& param)
{
	Player *player = dynamic_cast<Player*>(c);

	if(!player)
	    return false;

	if(param == " " || param == "")
    {
	    player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "You must enter a message.");
	    return false;
	}

    std::string filename;
    filename = g_config.getString(ConfigManager::DATA_DIRECTORY) + "logs/reports/" + player->getName() + ".txt";
    std::ofstream reportFile(filename.c_str(),std::ios::app);
    reportFile << "------------------------------------------------------\n";
    reportFile << "Name: " << player->getName() << " - Level: " << player->getLevel() << " - Access: " << player->getAccess() << "\n";
    reportFile << "Position: " << player->getPosition().x << " " << player->getPosition().y << " " << player->getPosition().z << " - Skull: " << player->getSkull() << "\n";
    reportFile << "Report: " << param << "\n";
    reportFile << "------------------------------------------------------\n";
    reportFile.close();
    std::cout << ":: Report added by " <<  player->getName()  << "! Please see it at reports folder!\n";
    player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Your report was sucessfully sent!");


	return true;
}

bool Commands::showExpForLvl(Creature* creature, const std::string &cmd, const std::string &param)
{
	if(Player* player = creature->getPlayer())
    {
		std::stringstream msg;
	msg << "You need " << player->getExpForLevel(player->getLevel() + 1) - player->getExperience() << " experience points to gain level" << std::endl;
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, msg.str().c_str());
	}
	
	return true;
}

bool Commands::showManaForLvl(Creature* creature, const std::string &cmd, const std::string &param)
{
	if(Player* player = creature->getPlayer())
    {
		std::stringstream msg;
		msg << "You need to spent " << (long)(player->vocation->getReqMana(player->getMagicLevel()+1) - player->getManaSpent()) / g_config.getDouble(ConfigManager::RATE_MAGIC) << " mana to gain magic level" << std::endl;
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, msg.str().c_str());
	}

	return true;
}

bool Commands::skillsPlayers(Creature* creature, const std::string &cmd, const std::string &param)
{
 	Player* player = creature->getPlayer();
 	
	if(!player)
	    return false;
 	    
	boost::char_separator<char> sep(",");
	tokenizer cmdtokens(param, sep);
	tokenizer::iterator cmdit = cmdtokens.begin();
	std::string param1, param2;
	param1 = parseParams(cmdit, cmdtokens.end());
	param2 = parseParams(cmdit, cmdtokens.end());
	trimString(param1);
	trimString(param2);

	Player* paramPlayer = g_game.getPlayerByName(param1);
	if(!paramPlayer)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "Couldn't find target.");
		return false;
	}

	if(param2[0] == 'l' || param2[0] == 'e')
		paramPlayer->addExperience(Player::getExpForLevel(paramPlayer->getLevel() + 1) - paramPlayer->experience);
	else if(param2[0] == 'm')
		paramPlayer->addManaSpent(player->vocation->getReqMana(paramPlayer->getMagicLevel() + 1) - paramPlayer->manaSpent, false);
	else
		paramPlayer->addSkillAdvance(getSkillId(param2), paramPlayer->vocation->getReqSkillTries(getSkillId(param2), paramPlayer->getSkill(getSkillId(param2), SKILL_LEVEL) + 1));
}
