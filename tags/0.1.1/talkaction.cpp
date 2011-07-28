////////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////
#include "otpch.h"
#include "talkaction.h"

#include <boost/config.hpp>
#include <boost/version.hpp>
#include <string>
#include <sstream>
#include <utility>
#include <iostream>
#include <fstream>

#include "iomanager.h"
#include "ioban.h"

#include "player.h"
#include "npc.h"

#include "house.h"
#include "town.h"

#include "teleport.h"
#include "status.h"
#include "textlogger.h"
#include "actions.h"
#include "spells.h"

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
#include "outputmessage.h"
#include "connection.h"
#include "admin.h"
#include "manager.h"
#include "protocollogin.h"
#include "protocolold.h"
#endif

#ifdef __LOGIN_SERVER__
#include "gameservers.h"
#endif

#include "configmanager.h"
#include "game.h"
#include "chat.h"
#include "tools.h"
#include "mounts.h"
#include "globalevent.h"
#include "monsters.h"
#include "movement.h"
#include "weapons.h"
#include "quests.h"

extern ConfigManager g_config;
extern Game g_game;
extern Chat g_chat;
extern TalkActions* g_talkActions;
extern Actions* g_actions;
extern Monsters g_monsters;
extern CreatureEvents* g_creatureEvents;
extern GlobalEvents* g_globalEvents;
extern MoveEvents* g_moveEvents;
extern Spells* g_spells;
extern Weapons* g_weapons;
extern Npcs g_npcs;

TalkActions::TalkActions():
m_interface("TalkAction Interface")
{
	m_interface.initState();
	defaultTalkAction = NULL;
}

TalkActions::~TalkActions()
{
	clear();
}

void TalkActions::clear()
{
	for(TalkActionsMap::iterator it = talksMap.begin(); it != talksMap.end(); ++it)
		delete it->second;

	talksMap.clear();
	m_interface.reInitState();

	delete defaultTalkAction;
	defaultTalkAction = NULL;
}

Event* TalkActions::getEvent(const std::string& nodeName)
{
	if(asLowerCaseString(nodeName) == "talkaction")
		return new TalkAction(&m_interface);

	return NULL;
}

bool TalkActions::registerEvent(Event* event, xmlNodePtr p, bool override)
{
	TalkAction* talkAction = dynamic_cast<TalkAction*>(event);
	if(!talkAction)
		return false;

	std::string strValue;
	if(readXMLString(p, "default", strValue) && booleanString(strValue))
	{
		if(!defaultTalkAction)
			defaultTalkAction = talkAction;
		else if(override)
		{
			delete defaultTalkAction;
			defaultTalkAction = talkAction;
		}
		else
			std::clog << "[Warning - TalkAction::registerEvent] You cannot define more than one default talkAction." << std::endl;

		return true;
	}

	if(!readXMLString(p, "separator", strValue) || strValue.empty())
		strValue = ";";

	StringVec strVector = explodeString(talkAction->getWords(), strValue);
	for(StringVec::iterator it = strVector.begin(); it != strVector.end(); ++it)
	{
		trimString(*it);
		talkAction->setWords(*it);
		if(talksMap.find(*it) != talksMap.end())
		{
			if(!override)
			{
				std::clog << "[Warning - TalkAction::registerEvent] Duplicate registered talkaction with words: " << (*it) << std::endl;
				continue;
			}
			else
				delete talksMap[(*it)];
		}

		talksMap[(*it)] = new TalkAction(talkAction);
	}

	delete talkAction;
	return true;
}

bool TalkActions::onPlayerSay(Creature* creature, uint16_t channelId, const std::string& words, bool ignoreAccess)
{
	std::string cmd[TALKFILTER_LAST] = words, param[TALKFILTER_LAST] = "";
	std::string::size_type loc = words.find('"', 0);
	if(loc != std::string::npos)
	{
		cmd[TALKFILTER_QUOTATION] = std::string(words, 0, loc);
		param[TALKFILTER_QUOTATION] = std::string(words, (loc + 1), (words.size() - (loc - 1)));
		trimString(cmd[TALKFILTER_QUOTATION]);
	}

	loc = words.find(" ", 0);
	if(loc != std::string::npos)
	{
		cmd[TALKFILTER_WORD] = std::string(words, 0, loc);
		param[TALKFILTER_WORD] = std::string(words, (loc + 1), (words.size() - (loc - 1)));

		std::string::size_type spaceLoc = words.find(" ", ++loc);
		if(spaceLoc != std::string::npos)
		{
			cmd[TALKFILTER_WORD_SPACED] = std::string(words, 0, spaceLoc);
			param[TALKFILTER_WORD_SPACED] = std::string(words, (spaceLoc + 1), (words.size() - (spaceLoc - 1)));
		}
	}

	TalkAction* talkAction = NULL;
	for(TalkActionsMap::iterator it = talksMap.begin(); it != talksMap.end(); ++it)
	{
		if(it->first == cmd[it->second->getFilter()] || (!it->second->isSensitive()
			&& boost::algorithm::iequals(it->first, cmd[it->second->getFilter()])))
		{
			talkAction = it->second;
			break;
		}
	}

	if(!talkAction && defaultTalkAction)
		talkAction = defaultTalkAction;

	if(!talkAction || (talkAction->getChannel() != -1 && talkAction->getChannel() != channelId))
		return false;

	Player* player = creature->getPlayer();
	StringVec exceptions = talkAction->getExceptions();
	if(player && ((!ignoreAccess && std::find(exceptions.begin(), exceptions.end(), asLowerCaseString(
		player->getName())) == exceptions.end() && talkAction->getAccess() > player->getAccess())
		|| player->isAccountManager()))
	{
		if(player->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges))
		{
			player->sendTextMessage(MSG_STATUS_SMALL, "You cannot execute this command.");
			return true;
		}

		return false;
	}
	
	if(talkAction->isPremium() && !player->isPremium())
	{
		player->sendCancel("You need a premium account to execute this command.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	if(talkAction->isLogged())
	{
		if(player)
			player->sendTextMessage(MSG_STATUS_CONSOLE_RED, words.c_str());

		Logger::getInstance()->eFile("talkactions/" + creature->getName() + ".log", words, true);
	}

	if(talkAction->isScripted())
		return talkAction->executeSay(creature, cmd[talkAction->getFilter()], param[talkAction->getFilter()], channelId);

	if(TalkFunction* function = talkAction->getFunction())
		return function(creature, cmd[talkAction->getFilter()], param[talkAction->getFilter()]);

	return false;
}

TalkAction::TalkAction(LuaInterface* _interface):
Event(_interface)
{
	m_function = NULL;
	m_filter = TALKFILTER_WORD;
	m_access = 0;
	m_channel = -1;
	m_logged = m_hidden = false;
	m_sensitive = false;
	m_premium = false;
}

TalkAction::TalkAction(const TalkAction* copy):
Event(copy)
{
	m_words = copy->m_words;
	m_function = copy->m_function;
	m_filter = copy->m_filter;
	m_access = copy->m_access;
	m_channel = copy->m_channel;
	m_logged = copy->m_logged;
	m_hidden = copy->m_hidden;
	m_sensitive = copy->m_sensitive;
	m_exceptions = copy->m_exceptions;
	m_premium = copy->m_premium;
}

bool TalkAction::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	if(readXMLString(p, "words", strValue))
		m_words = strValue;
	else if(!readXMLString(p, "default", strValue) || !booleanString(strValue))
	{
		std::clog << "[Error - TalkAction::configureEvent] No words for TalkAction." << std::endl;
		return false;
	}

	if(readXMLString(p, "filter", strValue))
	{
		std::string tmpStrValue = asLowerCaseString(strValue);
		if(tmpStrValue == "quotation")
			m_filter = TALKFILTER_QUOTATION;
		else if(tmpStrValue == "word")
			m_filter = TALKFILTER_WORD;
		else if(tmpStrValue == "word-spaced")
			m_filter = TALKFILTER_WORD_SPACED;
		else
			std::clog << "[Warning - TalkAction::configureEvent] Unknown filter for TalkAction: " << strValue << ", using default." << std::endl;
	}

	int32_t intValue;
	if(readXMLInteger(p, "access", intValue))
		m_access = intValue;

	if(readXMLInteger(p, "channel", intValue))
		m_channel = intValue;

	if(readXMLString(p, "logged", strValue) || readXMLString(p, "log", strValue))
		m_logged = booleanString(strValue);

	if(readXMLString(p, "hidden", strValue) || readXMLString(p, "hide", strValue))
		m_hidden = booleanString(strValue);
		
	if(readXMLString(p, "prem", strValue) || readXMLString(p, "premium", strValue))
		m_premium = booleanString(strValue);

	if(readXMLString(p, "case-sensitive", strValue) || readXMLString(p, "casesensitive", strValue) || readXMLString(p, "sensitive", strValue))
		m_sensitive = booleanString(strValue);

	if(readXMLString(p, "exception", strValue))
		m_exceptions = explodeString(asLowerCaseString(strValue), ";");

	return true;
}

bool TalkAction::loadFunction(const std::string& functionName)
{
	m_functionName = asLowerCaseString(functionName);
	if(m_functionName == "housebuy")
		m_function = houseBuy;
 	else if(m_functionName == "housesell")
		m_function = houseSell;
	else if(m_functionName == "housekick")
		m_function = houseKick;
	else if(m_functionName == "housedoorlist")
		m_function = houseDoorList;
	else if(m_functionName == "houseguestlist")
		m_function = houseGuestList;
	else if(m_functionName == "housesubownerlist")
		m_function = houseSubOwnerList;
 	else if(m_functionName == "guildjoin")
		m_function = guildJoin;
 	else if(m_functionName == "guildcreate")
		m_function = guildCreate;
	else if(m_functionName == "thingproporties")
		m_function = thingProporties;
	else if(m_functionName == "banishmentinfo")
		m_function = banishmentInfo;
	else if(m_functionName == "diagnostics")
		m_function = diagnostics;
	else if(m_functionName == "ghost")
		m_function = ghost;
	else if(m_functionName == "software")
		m_function = software;
	else if(m_functionName == "addpremiumdays")
		m_function = addPremiumDays;
	else if(m_functionName == "reloadinfo")
		m_function = reloadInfo;
	else if(m_functionName == "setworldtype")
		m_function = setWorldType;
	else if(m_functionName == "addbless")
		m_function = addBless;
	else if(m_functionName == "addaddon")
		m_function = addAddon;
	else if(m_functionName == "saveserver")
		m_function = saveServer;
	else if(m_functionName == "addskill")
		m_function = addSkill;
	else if(m_functionName == "checkpvptype")
		m_function = checkPvpType;
	else if(m_functionName == "sendbugreport")
		m_function = sendBugReport;
	else if(m_functionName == "showexp")
		m_function = showExp;
	else if(m_functionName == "showmana")
		m_function = showMana;
	else
	{
		std::clog << "[Warning - TalkAction::loadFunction] Function \"" << m_functionName << "\" does not exist." << std::endl;
		return false;
	}

	m_scripted = EVENT_SCRIPT_FALSE;
	return true;
}

int32_t TalkAction::executeSay(Creature* creature, const std::string& words, std::string param, uint16_t channel)
{
	//onSay(cid, words, param, channel)
	if(m_interface->reserveEnv())
	{
		trimString(param);
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			scriptstream << "local words = \"" << words << "\"" << std::endl;
			scriptstream << "local param = \"" << param << "\"" << std::endl;
			scriptstream << "local channel = " << channel << std::endl;

			scriptstream << m_scriptData;
			bool result = true;
			if(m_interface->loadBuffer(scriptstream.str()))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[125];
			sprintf(desc, "%s - %s- %s", creature->getName().c_str(), words.c_str(), param.c_str());
			env->setEvent(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, env->addThing(creature));

			lua_pushstring(L, words.c_str());
			lua_pushstring(L, param.c_str());
			lua_pushnumber(L, channel);

			bool result = m_interface->callFunction(4);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		std::clog << "[Error - TalkAction::executeSay] Call stack overflow." << std::endl;
		return 0;
	}
}

bool TalkAction::houseBuy(Creature* creature, const std::string&, const std::string&)
{
	Player* player = creature->getPlayer();
	if(!player || !g_config.getBool(ConfigManager::HOUSE_BUY_AND_SELL))
		return false;

	const Position& pos = getNextPosition(player->getDirection(), player->getPosition());
	Tile* tile = g_game.getTile(pos);
	if(!tile)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	HouseTile* houseTile = tile->getHouseTile();
	if(!houseTile)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	House* house = houseTile->getHouse();
	if(!house)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	if(!house->getDoorByPosition(pos))
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	if(!house->isGuild())
	{
		if(Houses::getInstance()->getHouseByPlayerId(player->getGUID()))
		{
			player->sendCancel("You already rent another house.");
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}

		uint16_t accountHouses = g_config.getNumber(ConfigManager::HOUSES_PER_ACCOUNT);
		if(accountHouses > 0 && Houses::getInstance()->getHousesCount(player->getAccount()) >= accountHouses)
		{
			char buffer[80];
			sprintf(buffer, "You may own only %d house%s per account.", accountHouses, (accountHouses != 1 ? "s" : ""));

			player->sendCancel(buffer);
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}

		if(g_config.getBool(ConfigManager::HOUSE_NEED_PREMIUM) && !player->isPremium())
		{
			player->sendCancelMessage(RET_YOUNEEDPREMIUMACCOUNT);
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}

		uint32_t levelToBuyHouse = g_config.getNumber(ConfigManager::LEVEL_TO_BUY_HOUSE);
		if(player->getLevel() < levelToBuyHouse)
		{
			char buffer[90];
			sprintf(buffer, "You have to be at least Level %d to purchase a house.", levelToBuyHouse);
			player->sendCancel(buffer);
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}
	}
	else
	{
		if(!player->getGuildId() || player->getGuildLevel() != GUILDLEVEL_LEADER)
		{
			player->sendCancel("You have to be at least a guild leader to purchase a hall.");
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}

		if(Houses::getInstance()->getHouseByGuildId(player->getGuildId()))
		{
			player->sendCancel("Your guild rents already another hall.");
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}
	}

	if(house->getOwner())
	{
		player->sendCancel("This flat is already owned by someone else.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	if((uint32_t)g_game.getMoney(player) < house->getPrice() || !g_game.removeMoney(player, house->getPrice()))
	{
		player->sendCancel("You do not have enough money.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
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

	g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);
	return false;
}

bool TalkAction::houseSell(Creature* creature, const std::string&, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !g_config.getBool(ConfigManager::HOUSE_BUY_AND_SELL))
		return false;

	House* house = Houses::getInstance()->getHouseByPlayerId(player->getGUID());
	if(!house && (!player->getGuildId() || !(house = Houses::getInstance()->getHouseByGuildId(player->getGuildId()))))
	{
		player->sendCancel("You do not rent any flat.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	if(house->isGuild() && player->getGuildLevel() != GUILDLEVEL_LEADER)
	{
		player->sendCancel("You have to be at least a guild leader to sell this hall.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	Tile* tile = g_game.getTile(player->getPosition());
	if(!tile || !tile->getHouseTile() || tile->getHouseTile()->getHouse() != house)
	{
		player->sendCancel("You have to be inside a house that you would like to sell.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	Player* tradePartner = NULL;
	ReturnValue ret = g_game.getPlayerByNameWildcard(param, tradePartner);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	if(tradePartner == player)
	{
		player->sendCancel("You cannot trade with yourself.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	if(!house->isGuild())
	{
		if(Houses::getInstance()->getHouseByPlayerId(tradePartner->getGUID()))
		{
			player->sendCancel("Trade player already rents another house.");
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}

		uint16_t housesPerAccount = g_config.getNumber(ConfigManager::HOUSES_PER_ACCOUNT);
		if(housesPerAccount > 0 && Houses::getInstance()->getHousesCount(tradePartner->getAccount()) >= housesPerAccount)
		{
			char buffer[100];
			sprintf(buffer, "Trade player has reached limit of %d house%s per account.", housesPerAccount, (housesPerAccount != 1 ? "s" : ""));

			player->sendCancel(buffer);
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}

		if(!tradePartner->isPremium() && !g_config.getBool(ConfigManager::HOUSE_NEED_PREMIUM))
		{
			player->sendCancel("Trade player does not have a premium account.");
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}

		uint32_t levelToBuyHouse = g_config.getNumber(ConfigManager::LEVEL_TO_BUY_HOUSE);
		if(tradePartner->getLevel() < levelToBuyHouse)
		{
			char buffer[100];
			sprintf(buffer, "Trade player has to be at least Level %d to buy house.", levelToBuyHouse);

			player->sendCancel(buffer);
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}
	}
	else
	{
		if(!tradePartner->getGuildId() || tradePartner->getGuildLevel() != GUILDLEVEL_LEADER)
		{
			player->sendCancel("Trade player has to be at least a guild leader to buy a hall.");
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}

		if(Houses::getInstance()->getHouseByGuildId(tradePartner->getGuildId()))
		{
			player->sendCancel("Trade player's guild already rents another hall.");
			g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
			return false;
		}
	}

	if(!Position::areInRange<3,3,0>(tradePartner->getPosition(), player->getPosition()))
	{
		player->sendCancel("Trade player is too far away.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	if(!Houses::getInstance()->payRent(player, house, 0))
	{
		player->sendCancel("You have to pay a pre-rent first.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	Item* transferItem = TransferItem::createTransferItem(house);
	player->transferContainer.__addThing(NULL, transferItem);
	if(!g_game.internalStartTrade(player, tradePartner, transferItem))
		transferItem->onTradeEvent(ON_TRADE_CANCEL, player, NULL);

	g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);
	return false;
}

bool TalkAction::houseKick(Creature* creature, const std::string&, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	Player* targetPlayer = NULL;
	if(g_game.getPlayerByNameWildcard(param, targetPlayer) != RET_NOERROR)
		targetPlayer = player;

	House* house = Houses::getInstance()->getHouseByPlayer(targetPlayer);
	if(!house || !house->kickPlayer(player, targetPlayer))
	{
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		player->sendCancelMessage(RET_NOTPOSSIBLE);
	}
	else
		g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);

	return false;
}

bool TalkAction::houseDoorList(Creature* creature, const std::string&, const std::string&)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	House* house = Houses::getInstance()->getHouseByPlayer(player);
	if(!house)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return false;
	}

	Door* door = house->getDoorByPosition(getNextPosition(player->getDirection(), player->getPosition()));
	if(door && house->canEditAccessList(door->getDoorId(), player))
	{
		player->setEditHouse(house, door->getDoorId());
		player->sendHouseWindow(house, door->getDoorId());
		g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
	}

	return false;
}

bool TalkAction::houseGuestList(Creature* creature, const std::string&, const std::string&)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	House* house = Houses::getInstance()->getHouseByPlayer(player);
	if(house && house->canEditAccessList(GUEST_LIST, player))
	{
		player->setEditHouse(house, GUEST_LIST);
		player->sendHouseWindow(house, GUEST_LIST);
		g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
	}

	return false;
}

bool TalkAction::houseSubOwnerList(Creature* creature, const std::string&, const std::string&)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	House* house = Houses::getInstance()->getHouseByPlayer(player);
	if(house && house->canEditAccessList(SUBOWNER_LIST, player))
	{
		player->setEditHouse(house, SUBOWNER_LIST);
		player->sendHouseWindow(house, SUBOWNER_LIST);
		g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
	}

	return false;
}

bool TalkAction::guildJoin(Creature* creature, const std::string&, const std::string& param)
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
					guildChannel->talk(player, SPEAK_CHANNEL_RA, buffer);
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

bool TalkAction::guildCreate(Creature* creature, const std::string&, const std::string& param)
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
		std::stringstream stream;
		stream << "You have to be at least Level " << levelToFormGuild << " to form a guild.";
		player->sendCancel(stream.str().c_str());
		return true;
	}

	const int32_t premiumDays = g_config.getNumber(ConfigManager::GUILD_PREMIUM_DAYS);
	if(player->getPremiumDays() < premiumDays && !g_config.getBool(ConfigManager::FREE_PREMIUM))
	{
		std::stringstream stream;
		stream << "You need to have at least " << premiumDays << " premium days to form a guild.";
		player->sendCancel(stream.str().c_str());
		return true;
	}

	player->setGuildName(param_);
	IOGuild::getInstance()->createGuild(player);

	std::stringstream stream;
	stream << "You have formed guild \"" << param.c_str() << "\"!";
	player->sendTextMessage(MSG_INFO_DESCR, stream.str().c_str());
	return true;
}

bool TalkAction::thingProporties(Creature* creature, const std::string&, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	const Position& pos = getNextPosition(player->getDirection(), player->getPosition());
	Tile* tile = g_game.getTile(pos);
	if(!tile)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "No tile found.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
		return true;
	}

	Thing* thing = tile->getTopVisibleThing(creature);
	if(!thing)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "No object found.");
		g_game.addMagicEffect(player->getPosition(), NM_ME_POFF);
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

	g_game.addMagicEffect(pos, NM_ME_WRAPS_GREEN);
	if(invalid.empty())
		return true;

	std::string tmp = "Following action was invalid: " + invalid;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, tmp.c_str());
	return true;
}

bool TalkAction::banishmentInfo(Creature* creature, const std::string&, const std::string& param)
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

	std::stringstream stream;
	stream << what.c_str() << " has been " << (deletion ? "deleted" : "banished") << " at:\n" << formatDateEx(ban.added, "%d %b %Y").c_str()
		   << " by: " << admin.c_str() << ",\nfor the following reason:\n" << getReason(ban.reason).c_str() << ".\nThe action taken was:\n"
		   << getAction(ban.action, false).c_str() << ".\nThe comment given was:\n" << ban.comment.c_str() << ".\n" << end.c_str()
		   << (deletion ? "." : formatDateEx(ban.expires).c_str()) << ".";

	player->sendFYIBox(stream.str().c_str());
	return true;
}

bool TalkAction::diagnostics(Creature* creature, const std::string&, const std::string&)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;
#ifdef __ENABLE_SERVER_DIAGNOSTIC__

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

bool TalkAction::ghost(Creature* creature, const std::string&, const std::string&)
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
				tmpPlayer->sendMagicEffect(player->getPosition(), NM_ME_TELEPORT);
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
				tmpPlayer->sendMagicEffect(player->getPosition(), NM_ME_POFF);
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

bool TalkAction::software(Creature* creature, const std::string&, const std::string&)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Crystal Server - Version 0.1.1 (Ice Fenix).");
	return true;
}

bool TalkAction::addPremiumDays(Creature* creature, const std::string& cmd, const std::string& param)
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
            g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);
            return true;
        }
    }

	return false;
}

bool TalkAction::reloadInfo(Creature* creature, const std::string& cmd, const std::string& param)
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

bool TalkAction::setWorldType(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

     if(param.empty())
     {
         player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Command requiere param.");
         return false;
     }

	if(param == "2" || param == "pvp" || param == "open" || param == "openpvp" || param == "open-pvp")
	{
		g_game.setWorldType(WORLDTYPE_OPEN);
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type changed to Open PvP.");
	}

	else if(param == "1" || param == "no-pvp" || param == "non-pvp" || param == "nonpvp" || param == "optional" || param == "safe" || param == "optionalpvp" || param == "optional-pvp")
	{
		g_game.setWorldType(WORLDTYPE_OPTIONAL);
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type changed to Optional PvP.");
	}

	else if(param == "3" || param == "e-pvp" || param == "epvp" || param == "pvp-enforced" || param == "war" || param == "hardcore" || param == "enforced" || param == "hardcore-pvp" || param == "pvpe" || param == "pvp-e")
	{
		g_game.setWorldType(WORLDTYPE_HARDCORE);
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type changed to Hardcore PvP.");
	}

     return true;
}

bool TalkAction::checkPvpType(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	if(g_game.getWorldType() == WORLDTYPE_OPEN)
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type is currently set to Open PvP.");

	else if(g_game.getWorldType() == WORLDTYPE_OPTIONAL)
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type is currently set to Optional PvP.");

	else if(g_game.getWorldType() == WORLDTYPE_HARDCORE)
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "World type is currently set to Hardcore PvP.");

     return true;
}

bool TalkAction::addBless(Creature* creature, const std::string& cmd, const std::string& param)
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
		g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);
		return true;
	}

	return true;
}

bool TalkAction::addAddon(Creature* creature, const std::string &cmd, const std::string &param)
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

		g_game.addMagicEffect(player->getPosition(), NM_ME_WRAPS_BLUE);
		player->addOutfit(lookType, addon);
		return true;
	}

	return true;
}

bool TalkAction::saveServer(Creature* creature, const std::string& cmd, const std::string& param)
{
 	 Player* player = creature->getPlayer();

	 g_game.saveGameState(true);
	 player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Save server completed.");
}

bool TalkAction::sendBugReport(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player *player = dynamic_cast<Player*>(creature);
	if(!player)
	    return false;

	if(param.empty())
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
    std::clog << ":: Report added by " <<  player->getName()  << "! Please see it at reports folder!\n";
    player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Your report was sucessfully sent!");


	return true;
}

bool TalkAction::showExp(Creature* creature, const std::string &cmd, const std::string &param)
{
	if(Player* player = creature->getPlayer())
    {
		std::stringstream msg;
		msg << "You need " << player->getExpForLevel(player->getLevel() + 1) - player->getExperience() << " experience points to gain level" << std::endl;
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, msg.str().c_str());
	}

	return true;
}

bool TalkAction::showMana(Creature* creature, const std::string &cmd, const std::string &param)
{
	if(Player* player = creature->getPlayer())
    {
		std::stringstream msg;
		msg << "You need to spent " << (long)(player->vocation->getReqMana(player->getMagicLevel()+1) - player->getManaSpent()) / g_config.getDouble(ConfigManager::RATE_MAGIC) << " mana to gain magic level" << std::endl;
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, msg.str().c_str());
	}

	return true;
}

bool TalkAction::addSkill(Creature* creature, const std::string &cmd, const std::string &param)
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
