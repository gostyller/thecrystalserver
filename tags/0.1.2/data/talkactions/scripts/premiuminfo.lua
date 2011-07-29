function onSay(cid, words, param, channel)
	local premiumDays = getPlayerPremiumDays(cid)
	if (premiumDays == 0) then
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE,  "You have doesn't have a premium account.")

	elseif (premiumDays == 1) then
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE,  "You have 1 premium day left.")

	elseif (premiumDays == 5) then
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE,  "You have 5 premium days left.")
	else
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE,  "You have: " .. premiumDays .. " premium days left.")
	end
	return true
end
