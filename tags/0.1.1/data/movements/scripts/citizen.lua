function onStepIn(cid, item, position, fromPosition)
	if (item.actionid == 30020) then
		doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You are the newest resident of City 1.")
		doPlayerSetTown(cid, 1)
	elseif (item.actionid == 30021) then
		doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You are the newest resident of City 2.")
		doPlayerSetTown(cid, 2)
	elseif (item.actionid == 30022) then
		doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You are the newest resident of City 3.")
		doPlayerSetTown(cid, 3)
	elseif (item.actionid == 30023) then
		doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You are the newest resident of City 4.")
		doPlayerSetTown(cid, 4)
	elseif (item.actionid == 30024) then
		doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You are the newest resident of City 5.")
		doPlayerSetTown(cid, 5)
	elseif (item.actionid == 30025) then
		doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You are the newest resident of City 6.")
		doPlayerSetTown(cid, 6)
	end
end