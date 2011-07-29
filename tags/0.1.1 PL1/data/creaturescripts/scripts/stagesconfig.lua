skillConfig = {skill = getConfigValue('rateSkill'), magiclevel = getConfigValue('rateMagic')}
skillStages = {}
skillStages[SKILL_FIST] = {{0,8},{60,5},{80,3},{100,2}}
skillStages[SKILL_CLUB] = {{0,8},{60,5},{80,2},{100,1}}
skillStages[SKILL_SWORD] = {{0,8},{60,5},{80,2},{100,1}}
skillStages[SKILL_AXE] = {{0,8},{60,5},{80,2},{100,1}}
skillStages[SKILL_DISTANCE] = {{0,8},{60,5},{80,2},{100,1}}
skillStages[SKILL_SHIELD] = {{0,9},{60,8},{80,7},{100,6},{110,3}}
skillStages[SKILL_FISHING] = {{0,5},{60,4},{80,3},{100,2},{110,1}}
skillStages[SKILL__MAGLEVEL] = {{0,10},{6,5},{15,7},{80,5},{90,2},{99,1}}
showInfoOnAdvance = true -- send player message about skill rate change
showInfoOnLogin = true -- send player message about skill rates when he login
 
function getPlayerSkillRatesText(cid)
 local skillInfo = getPlayerRates(cid)
 return "YOUR RATES: [ Magic Level: " .. skillInfo[SKILL__MAGLEVEL] * skillConfig.magiclevel .. "x || Fist: " .. skillInfo[SKILL_FIST] * skillConfig.skill .. "x | Club: " .. skillInfo[SKILL_CLUB] * skillConfig.skill .. "x |  Sword: " .. skillInfo[SKILL_SWORD] * skillConfig.skill .. "x | Axe: " .. skillInfo[SKILL_AXE] * skillConfig.skill .. "x |  Distance: " .. skillInfo[SKILL_DISTANCE] * skillConfig.skill .. " | Shielding: " .. skillInfo[SKILL_SHIELD] * skillConfig.skill .. "x | Fishing: " .. skillInfo[SKILL_FISHING] * skillConfig.skill .. "x ]"
end  