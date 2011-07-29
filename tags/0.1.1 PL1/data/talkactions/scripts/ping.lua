function onSay(cid, words, param, channel)
	ping.getPing(cid,29894,function(cid,_,ping_)
		doPlayerSendTextMessage(cid,25,'Your ping is '..ping_..' ms.')
	end)
	doPlayerSendTextMessage(cid,25,'Sending an request...')
	return true
end