local config = {
	loginMessage = getConfigValue('loginMessage'),
	useFragHandler = getBooleanFromString(getConfigValue('useFragHandler'))
}

function onLogin(cid)
	local loss = getConfigValue('deathLostPercent')
	if(loss ~= nil) then
		doPlayerSetLossPercent(cid, PLAYERLOSS_EXPERIENCE, loss * 10)
	end

	local accountManager = getPlayerAccountManager(cid)
	if(accountManager == MANAGER_NONE) then
		local lastLogin, str = getPlayerLastLoginSaved(cid), config.loginMessage
		if(lastLogin > 0) then
			doPlayerSendTextMessage(cid, MESSAGE_STATUS_DEFAULT, str)
			str = "Your last visit was on " .. os.date("%a %b %d %X %Y", lastLogin) .. "."
		else
			str = str .. " Please choose your outfit."
			doPlayerSendOutfitWindow(cid)
		end

		doPlayerSendTextMessage(cid, MESSAGE_STATUS_DEFAULT, str)
	elseif(accountManager == MANAGER_NAMELOCK) then
		doPlayerSendTextMessage(cid, MESSAGE_FIRST, "Hello, it appears that your character has been namelocked, what would you like as your new name?")
	elseif(accountManager == MANAGER_ACCOUNT) then
		doPlayerSendTextMessage(cid, MESSAGE_FIRST, "Hello, type 'account' to manage your account and if you want to start over then type 'cancel'.")
	else
		doPlayerSendTextMessage(cid, MESSAGE_FIRST, "Hello, type 'account' to create an account or type 'recover' to recover an account.")
	end

	if(not isPlayerGhost(cid)) then
		doSendMagicEffect(getCreaturePosition(cid), CONST_ME_TELEPORT)
	end

	doPlayerAddMount(cid, 1)
	doPlayerAddMount(cid, 2)
	doPlayerAddMount(cid, 3)
	doPlayerAddMount(cid, 4)
	doPlayerAddMount(cid, 5)
	doPlayerAddMount(cid, 6)
	doPlayerAddMount(cid, 7)
	doPlayerAddMount(cid, 8)
	doPlayerAddMount(cid, 9)
	doPlayerAddMount(cid, 10)
	doPlayerAddMount(cid, 11)
	doPlayerAddMount(cid, 12)

	registerCreatureEvent(cid, "Mail")
	registerCreatureEvent(cid, "GuildMotd")

	registerCreatureEvent(cid, "Idle")
	if(config.useFragHandler) then
		registerCreatureEvent(cid, "SkullCheck")
	end

	registerCreatureEvent(cid, "ReportBug")
	registerCreatureEvent(cid, "AdvanceSave")
	return true
end
