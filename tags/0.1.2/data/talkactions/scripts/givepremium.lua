function onSay(cid, words, param, channel)
	local pid = 0
	if(param == '') then
		pid = getCreatureTarget(cid)
		if(pid == 0) then
			doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Command param required.")
			return true
		end
	else
		pid = getPlayerByNameWildcard(param)
	end

	if (pid) then
		db.executeQuery("UPDATE `accounts` SET `premdays` = `premdays` + 7;")
		doPlayerSave(cid, true)
		doRemoveCreature(pid)
	end 
	return true
end
