function onSay(cid, words, param, channel)
 
local param = string.explode(param, ",")
local item =
{
	player = getPlayerByNameWildcard(param[1]),
	itemid = tonumber(param[2]),
	type = tonumber(param[3]),
	charges = 1
}
local str =
{
	"",
	""
}
 
	if(item.player == 0 or item.player == nil) then
		doPlayerSendCancel(cid, "Player " .. param[1] .. " is not online.")
		doSendMagicEffect(getCreaturePosition(cid), CONST_ME_POFF)
		return TRUE
	end
 
	if(not item.itemid) then
		item.itemid = getItemIdByName(param[2], false)
		if not item.itemid then
			doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Item wich such name does not exists.")
			doSendMagicEffect(getCreaturePosition(cid), CONST_ME_POFF)
			return TRUE
		end
	end
 
	if(item.itemid < 1) then
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "No item specified.")
		doSendMagicEffect(getCreaturePosition(cid), CONST_ME_POFF)
		return TRUE
	end
 
	if(not item.type) then
		if(isItemRune(item.itemid) or isItemStackable(item.itemid)) == TRUE then
			item.type = 100
			item.charges = 1
		else
			item.type = 1
			item.charges = 1
		end
	end
 
	if(isItemMovable(item.itemid) == FALSE) then
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "You cannot give that item.")
		doSendMagicEffect(getCreaturePosition(cid), CONST_ME_POFF)
		return TRUE
	end
 
local str =
{
	"You give " .. item.type .."x " .. getItemNameById(item.itemid) .. " to " .. param[1] .. ".",
	"You received " .. item.type .. "x " .. getItemNameById(item.itemid) .. " from " .. getCreatureName(cid) .. "."
}
 
	if(isItemRune(item.itemid) == TRUE) then
		item.charges = item.type
		item.type = 1
		str[1] = "You give " .. item.type .. "x " .. getItemNameById(item.itemid) .. " with " .. item.charges .. "x charges to " .. param[1] .. "."
		str[2] = "You received " .. item.type .. "x " .. getItemNameById(item.itemid) .. " with " .. item.charges .. " charges from " .. getCreatureName(cid) .. "."
	end
 
	doPlayerGiveItem(item.player, item.itemid, item.type, item.charges)
	doSendMagicEffect(getCreaturePosition(cid), CONST_ME_MAGIC_RED)
	doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, str[1])
	doSendMagicEffect(getCreaturePosition(item.player), CONST_ME_MAGIC_RED)
	doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, str[2])
	return TRUE
end