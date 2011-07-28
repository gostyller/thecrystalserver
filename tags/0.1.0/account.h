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

#ifndef __ACCOUNT__
#define __ACCOUNT__
#include "otsystem.h"
#ifndef __LOGIN_SERVER__

typedef std::list<std::string> Characters;
#else
#include "gameservers.h"
typedef std::map<std::string, GameServer*> Characters;

#endif
class Account
{
	public:
		#ifdef __TCS_VIP__
		Account() {number = premiumDays = lastDay = warnings = vipDays = lastVip = 0;}
		#else
		Account() {number = premiumDays = lastDay = warnings = 0;}
		#endif
		virtual ~Account() {charList.clear();}
		
		#ifdef __TCS_VIP__
		uint32_t number, premiumDays, lastDay, vipDays, lastVip;
		#else
		uint32_t number, premiumDays, lastDay;
		#endif
			
		int32_t warnings;
		std::string name, password, recoveryKey, salt;
		Characters charList;
};
#endif
