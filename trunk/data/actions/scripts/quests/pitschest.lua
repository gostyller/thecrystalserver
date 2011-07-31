function onUse(cid, item, frompos, item2, topos)
   		if item.uid == 9500 then
   			queststatus = getPlayerStorageValue(cid, 8742)
   				if queststatus == -1 then
   					doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You have found a Arcane staff.")
   					doPlayerAddItem(cid, 2453, 1)
   					setPlayerStorageValue(cid, 8742, 1)
   				else
   					doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "The chest is empty.")
   				end
   		elseif item.uid == 9501 then
   			queststatus = getPlayerStorageValue(cid, 8742)
   				if queststatus == -1 then
   					doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You have found a Avenger.")
   					doPlayerAddItem(cid, 6528, 1)
   					setPlayerStorageValue(cid, 8742, 1)
   				else
   					doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "The chest is empty.")
   				end
   		elseif item.uid == 9502 then
   			queststatus = getPlayerStorageValue(cid, 8742)
   				if queststatus == -1 then
   					doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You have found a Arbalest.")
   					doPlayerAddItem(cid, 5803, 1)
   					setPlayerStorageValue(cid, 8742, 1)
   				else
   					doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "The chest is empty.")
   				end
			else
			return FALSE
   		end
   	return TRUE
end
