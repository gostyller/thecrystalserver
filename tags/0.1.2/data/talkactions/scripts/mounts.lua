local montConfig = 
{
	['widow queen'] 		= {cost = 50000, id = 1},
	['racing bird'] 		= {cost = 50000, id = 2},
	['war bear'] 		= {cost = 50000, id = 3},
	['black sheep'] 		= {cost = 50000, id = 4},
	['midnight panther'] 	= {cost = 50000, id = 5},
	['draptor'] 		= {cost = 50000, id = 6},
	['titanica'] 		= {cost = 50000, id = 7},
	['tin lizard'] 		= {cost = 50000, id = 8},
	['blazebringer'] 		= {cost = 50000, id = 9},
	['rapid boar'] 		= {cost = 50000, id = 10},
	['stampor'] 		= {cost = 50000, id = 11},
	['undead cavebear'] 	= {cost = 50000, id = 12}
}

function onSay(cid, words, param)
	if(param == '') then
		local str = ""
		for name, options in pairs(montConfig) do
			str = str .. "\n" .. name
		end

		doPlayerPopupFYI(cid, "List of mounts:\n\n" .. str)
		return true
	end

	local mount = montConfig[param]
	if(mount ~= nil) then
		if(not doPlayerRemoveMoney(cid, mount.cost)) then
			doPlayerSendCancel(cid, "You doesn't enough money to buy this mount " .. param .. ".\n It costs (" .. mount.cost .. "gp)")
			doSendMagicEffect(getCreaturePosition(cid), CONST_ME_POFF)
			return true
		end

		doPlayerAddMount(cid, mount.id)
		doSendMagicEffect(getCreaturePosition(cid), CONST_ME_GIFT_WRAPS)
	else
		doPlayerSendCancel(cid, "The mount not found in list. Please use '!mount' to see the mounts list.")
	end
	return true
end