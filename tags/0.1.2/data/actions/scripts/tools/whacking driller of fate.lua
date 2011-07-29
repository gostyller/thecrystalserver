function onUse(cid, item, fromPosition, itemEx, toPosition)
	-- Rope
	if(toPosition.x == CONTAINER_POSITION) then
		doPlayerSendDefaultCancel(cid, RETURNVALUE_NOTPOSSIBLE)
		return true
	end

	toPosition.stackpos = STACKPOS_GROUND
	local itemGround = getThingFromPos(toPosition)
	if(isInArray(SPOTS, itemGround.itemid)) then
		doTeleportThing(cid, {x = toPosition.x, y = toPosition.y + 1, z = toPosition.z - 1}, false)
		return true
	elseif(isInArray(ROPABLE, itemEx.itemid)) then
		local hole = getThingFromPos({x = toPosition.x, y = toPosition.y, z = toPosition.z + 1, stackpos = STACKPOS_TOP_MOVEABLE_ITEM_OR_CREATURE})
		if(isPlayer(hole.uid) and (not isPlayerGhost(hole.uid) or getPlayerGhostAccess(cid) >= getPlayerGhostAccess(hole.uid))) then
			doTeleportThing(hole.uid, {x = toPosition.x, y = toPosition.y + 1, z = toPosition.z}, false)
		else
			doPlayerSendDefaultCancel(cid, RETURNVALUE_NOTPOSSIBLE)
		end

		return true
	end

	-- Shovel
	if(isInArray(HOLES, itemEx.itemid)) then
		if(itemEx.itemid ~= 8579) then
			itemEx.itemid = itemEx.itemid + 1
		else
			itemEx.itemid = 8585
		end

		doTransformItem(itemEx.uid, itemEx.itemid)
		doDecayItem(itemEx.uid)
		return true
	elseif(SAND_HOLES[itemEx.itemid] ~= nil) then
		doSendMagicEffect(toPosition, CONST_ME_POFF)
		doTransformItem(itemEx.uid, SAND_HOLES[itemEx.itemid])

		doDecayItem(itemEx.uid)
		return true
	elseif(itemEx.itemid == SAND and not isRookie(cid)) then
		local rand = math.random(1, 100)
		if(rand >= 1 and rand <= 5) then
			doCreateItem(ITEM_SCARAB_COIN, 1, toPosition)
		elseif(rand > 85) then
			doCreateMonster("Scarab", toPosition, false)
		end

		doSendMagicEffect(toPosition, CONST_ME_POFF)
		return true
	end

	-- Pick
	local itemPickGround = getThingFromPos({x = toPosition.x, y = toPosition.y, z = toPosition.z + 1, stackpos = STACKPOS_GROUND})
	if(isInArray(SPOTS, itemPickGround.itemid) and isInArray({354, 355}, itemEx.itemid)) then
		doTransformItem(itemEx.uid, 392)
		doDecayItem(itemEx.uid)

		doSendMagicEffect(toPosition, CONST_ME_POFF)
		return true
	end

	if(itemEx.itemid == 7200) then
		doTransformItem(itemEx.uid, 7236)
		doSendMagicEffect(toPosition, CONST_ME_BLOCKHIT)
		return true
	end

	-- Machete
	if(isInArray(JUNGLE_GRASS, itemEx.itemid)) then
		doTransformItem(itemEx.uid, itemEx.itemid - 1)
		doDecayItem(itemEx.uid)
		return true
	end

	if(isInArray(SPIDER_WEB, itemEx.itemid)) then
		doTransformItem(itemEx.uid, (itemEx.itemid + 6))
		doDecayItem(itemEx.uid)
		return true
	end

	if(isInArray(WILD_GROWTH, itemEx.itemid)) then
		doSendMagicEffect(toPosition, CONST_ME_POFF)
		doRemoveItem(itemEx.uid)
		return destroyItem(cid, itemEx, toPosition)
	end

	return false
end

