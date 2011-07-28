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

#include "iomanager.h"
#include "ioban.h"

#include "player.h"
#include "npc.h"

#include "house.h"
#include "town.h"

#include "teleport.h"
#include "status.h"
#include "textlogger.h"

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
#include "outputmessage.h"
#include "connection.h"
#include "admin.h"
#include "manager.h"
#include "protocollogin.h"
#include "protocolold.h"
#endif

#include "configmanager.h"
#include "game.h"
#include "chat.h"
#include "tools.h"

extern ConfigManager g_config;
extern Game g_game;
extern Chat g_chat;
extern TalkActions* g_talkActions;

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
			std::cout << "[Warning - TalkAction::registerEvent] You cannot define more than one default talkAction." << std::endl;

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
				std::cout << "[Warning - TalkAction::registerEvent] Duplicate registered talkaction with words: " << (*it) << std::endl;
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
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}
	
	#ifdef __TCS_VIP__
	if(talkAction->isVip() && !player->isVip())
	{
		player->sendCancel("You need a vip account to execute this command.");
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}
	#endif

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
	#ifdef __TCS_VIP__
	m_vip = false;
	#endif
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
	#ifdef __TCS_VIP__
	m_vip = copy->m_vip;
	#endif
}

bool TalkAction::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	if(readXMLString(p, "words", strValue))
		m_words = strValue;
	else if(!readXMLString(p, "default", strValue) || !booleanString(strValue))
	{
		std::cout << "[Error - TalkAction::configureEvent] No words for TalkAction." << std::endl;
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
			std::cout << "[Warning - TalkAction::configureEvent] Unknown filter for TalkAction: " << strValue << ", using default." << std::endl;
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

#ifdef __TCS_VIP__
	if(readXMLString(p, "vip", strValue) || readXMLString(p, "needvip", strValue))
		m_vip = booleanString(strValue);
#endif

	if(readXMLString(p, "case-sensitive", strValue) || readXMLString(p, "casesensitive", strValue) || readXMLString(p, "sensitive", strValue))
		m_sensitive = booleanString(strValue);

	if(readXMLString(p, "exception", strValue))
		m_exceptions = explodeString(asLowerCaseString(strValue), ";");

	return true;
}

bool TalkAction::loadFunction(const std::string& functionName)
{
	m_functionName = asLowerCaseString(functionName);
	if(m_functionName == "housekick")
		m_function = houseKick;
	else if(m_functionName == "housedoorlist")
		m_function = houseDoorList;
	else if(m_functionName == "houseguestlist")
		m_function = houseGuestList;
	else if(m_functionName == "housesubownerlist")
		m_function = houseSubOwnerList;
	else
	{
		std::cout << "[Warning - TalkAction::loadFunction] Function \"" << m_functionName << "\" does not exist." << std::endl;
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
		std::cout << "[Error - TalkAction::executeSay] Call stack overflow." << std::endl;
		return 0;
	}
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
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		player->sendCancelMessage(RET_NOTPOSSIBLE);
	}
	else
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);

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
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Door* door = house->getDoorByPosition(getNextPosition(player->getDirection(), player->getPosition()));
	if(door && house->canEditAccessList(door->getDoorId(), player))
	{
		player->setEditHouse(house, door->getDoorId());
		player->sendHouseWindow(house, door->getDoorId());
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
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
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
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
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		g_game.addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	}

	return false;
}
