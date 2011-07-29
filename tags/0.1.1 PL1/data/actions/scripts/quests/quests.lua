function onUse(cid, item, fromPosition, itemEx, toPosition)
local reward = item.uid
local count = item.actionid
	if reward > 0 and reward < 9000 then
		local queststatus = getPlayerStorageValue(cid, reward)
		if queststatus == -1 then
			if count > 1 then
				doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, 'You have found '.. count ..' of ' .. getItemNameById(reward) .. '.')
				doPlayerAddItem(cid, reward, count)
				setPlayerStorageValue(cid, reward, 1)
			else
				doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, 'You have found a ' .. getItemNameById(reward) .. '.')
				doPlayerAddItem(cid, reward, 1)
				setPlayerStorageValue(cid, reward, 1)
			end
		else
			doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "It is empty.")
		end

		return 1
	else
		return 0
	end
end
