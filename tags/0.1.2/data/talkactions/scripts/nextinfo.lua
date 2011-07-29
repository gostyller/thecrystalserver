function onSay(cid, words, param, channel)
	local manainfo = getPlayerManaSpent(cid)
	local expinfo = getPlayerExperience(cid)

	if (words == "!mana") then
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "You need ".. manainfo .. " mana spent to gain magic level")
	elseif (words == "!exp") then
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "You need ".. expinfo.. " experience points to next level")
	end
	return true
end
