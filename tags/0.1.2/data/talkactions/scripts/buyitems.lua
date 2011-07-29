local config = 
{
	items = 
	{
		['aol'] 			= {cost = 10000, id = 2173},

		['brown backpack'] 	= {cost = 50, id = 1988},
		['green backpack'] 	= {cost = 50, id = 1998},
		['yellow backpack'] 	= {cost = 50, id = 1999},
		['red backpack'] 		= {cost = 50, id = 2000},
		['purple backpack'] 	= {cost = 50, id = 2001},
		['blue backpack'] 	= {cost = 50, id = 2002},
		['grey backpack'] 	= {cost = 50, id = 2003},
		['gold backpack'] 	= {cost = 50, id = 2004},

		['pirate backpack'] 	= {cost = 50, id = 5926},

		['demon backpack'] 	= {cost = 50, id = 10518},
		['orange backpack'] 	= {cost = 50, id = 10519},
		['moon backpack'] 	= {cost = 50, id = 10521},
		['crown backpack'] 	= {cost = 50, id = 10522},

		['hearth backpack'] 	= {cost = 50, id = 11119},
	}
}

function onSay(cid, words, param)
	if(param == '') then
		local str = ""
		for name, options in pairs(config.items) do
			str = str .. "\n" .. name
		end

		doPlayerPopupFYI(cid, "List of items in shop:\n\n" .. str)
		return true
	end

	local item = config.items[param]
	if(item ~= nil) then
		if(not doPlayerRemoveMoney(cid, item.cost)) then
			doPlayerSendCancel(cid, "You doesn't enough money to buy " .. param .. ".\n It costs (" .. item.cost .. "gp)")
			doSendMagicEffect(getCreaturePosition(cid), CONST_ME_POFF)
			return true
		end

		local amount = item.amount and item.amount or 1
		doPlayerAddItem(cid, item.id, amount)
		doSendMagicEffect(getCreaturePosition(cid), CONST_ME_GIFT_WRAPS)
	else
		doPlayerSendCancel(cid, "Item not found in list. Please use '!buy' to see the list.")
	end
	return true
end