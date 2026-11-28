PetalburgCity_House2_MapScripts::
	.byte 0

PetalburgCity_House2_EventScript_Woman::
	msgbox PetalburgCity_House2_Text_NormanBecameGymLeader, MSGBOX_NPC
	end

PetalburgCity_House2_EventScript_SchoolKid::
	msgbox PetalburgCity_House2_Text_BattledNormanOnce, MSGBOX_NPC
	end

PetalburgCity_House2_Text_NormanBecameGymLeader:
	.string "NORMAN became our town's new\n"
	.string "GYM LEADER.\p"
	.string "I think he called his family over from\n"
	.string "somewhere far away.$"

PetalburgCity_House2_Text_BattledNormanOnce:
	.string "I battled NORMAN once, but, whew,\n"
	.string "he was way too strong.\p"
	.string "How would I put it?\p"
	.string "I just got the feeling that he\n"
	.string "lives for POKéMON.$"


@ "W5" on Chase's Petalburg map: the NATURE GUY (menu) and the IV/EV person replace the two NORMAN fans.
PetalburgCity_House2_EventScript_NatureGuy::
	lock
	faceplayer
	message PetalburgCity_House2_Text_WhichStat
	waitmessage
PetalburgCity_House2_EventScript_NatureMenu::
	dynmultipush PetalburgCity_House2_Text_Attack, 0
	dynmultipush PetalburgCity_House2_Text_Defense, 1
	dynmultipush PetalburgCity_House2_Text_Speed, 2
	dynmultipush PetalburgCity_House2_Text_SpAtk, 3
	dynmultipush PetalburgCity_House2_Text_SpDef, 4
	dynmultipush PetalburgCity_House2_Text_NoChange, 5
	dynmultipush PetalburgCity_House2_Text_Exit, 6
	dynmultistack 0, 0, FALSE, 7, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, PetalburgCity_House2_EventScript_NatureAttack
	case 1, PetalburgCity_House2_EventScript_NatureDefense
	case 2, PetalburgCity_House2_EventScript_NatureSpeed
	case 3, PetalburgCity_House2_EventScript_NatureSpAtk
	case 4, PetalburgCity_House2_EventScript_NatureSpDef
	case 5, PetalburgCity_House2_EventScript_NatureNoChange
	closemessage
	release
	end

PetalburgCity_House2_EventScript_NatureAttack::
	message PetalburgCity_House2_Text_NatureAttack
	goto PetalburgCity_House2_EventScript_NatureAgain
PetalburgCity_House2_EventScript_NatureDefense::
	message PetalburgCity_House2_Text_NatureDefense
	goto PetalburgCity_House2_EventScript_NatureAgain
PetalburgCity_House2_EventScript_NatureSpeed::
	message PetalburgCity_House2_Text_NatureSpeed
	goto PetalburgCity_House2_EventScript_NatureAgain
PetalburgCity_House2_EventScript_NatureSpAtk::
	message PetalburgCity_House2_Text_NatureSpAtk
	goto PetalburgCity_House2_EventScript_NatureAgain
PetalburgCity_House2_EventScript_NatureSpDef::
	message PetalburgCity_House2_Text_NatureSpDef
	goto PetalburgCity_House2_EventScript_NatureAgain
PetalburgCity_House2_EventScript_NatureNoChange::
	message PetalburgCity_House2_Text_NatureNoChange
PetalburgCity_House2_EventScript_NatureAgain::
	waitmessage
	waitbuttonpress
	goto PetalburgCity_House2_EventScript_NatureMenu
	end

PetalburgCity_House2_EventScript_IVEVPerson::
	msgbox PetalburgCity_House2_Text_IVsAndEVs, MSGBOX_NPC
	end

PetalburgCity_House2_Text_WhichStat:
	.string "Every POKéMON has a nature. It raises\n"
	.string "one stat and lowers another.\p"
	.string "Which stat do you want to hear about?$"

PetalburgCity_House2_Text_Attack:
	.string "ATTACK$"
PetalburgCity_House2_Text_Defense:
	.string "DEFENSE$"
PetalburgCity_House2_Text_Speed:
	.string "SPEED$"
PetalburgCity_House2_Text_SpAtk:
	.string "SP. ATK$"
PetalburgCity_House2_Text_SpDef:
	.string "SP. DEF$"
PetalburgCity_House2_Text_NoChange:
	.string "NO CHANGE$"
PetalburgCity_House2_Text_Exit:
	.string "EXIT$"

PetalburgCity_House2_Text_NatureAttack:
	.string "LONELY, BRAVE, ADAMANT and NAUGHTY\n"
	.string "raise ATTACK.\p"
	.string "They lower DEFENSE, SPEED, SP. ATK\n"
	.string "and SP. DEF, in that order.$"

PetalburgCity_House2_Text_NatureDefense:
	.string "BOLD, RELAXED, IMPISH and LAX\n"
	.string "raise DEFENSE.\p"
	.string "They lower ATTACK, SPEED, SP. ATK\n"
	.string "and SP. DEF, in that order.$"

PetalburgCity_House2_Text_NatureSpeed:
	.string "TIMID, HASTY, JOLLY and NAIVE\n"
	.string "raise SPEED.\p"
	.string "They lower ATTACK, DEFENSE, SP. ATK\n"
	.string "and SP. DEF, in that order.$"

PetalburgCity_House2_Text_NatureSpAtk:
	.string "MODEST, MILD, QUIET and RASH\n"
	.string "raise SP. ATK.\p"
	.string "They lower ATTACK, DEFENSE, SPEED\n"
	.string "and SP. DEF, in that order.$"

PetalburgCity_House2_Text_NatureSpDef:
	.string "CALM, GENTLE, SASSY and CAREFUL\n"
	.string "raise SP. DEF.\p"
	.string "They lower ATTACK, DEFENSE, SPEED\n"
	.string "and SP. ATK, in that order.$"

PetalburgCity_House2_Text_NatureNoChange:
	.string "HARDY, DOCILE, SERIOUS, BASHFUL and\n"
	.string "QUIRKY don't change any stats.$"

PetalburgCity_House2_Text_IVsAndEVs:
	.string "IVs are how strong a POKéMON could get.\p"
	.string "Each stat gets a number from 0 to 31\n"
	.string "when you find it, and it never changes.\p"
	.string "EVs are what it learns from battling.\p"
	.string "Beat a fast POKéMON, and yours gets\n"
	.string "a little faster.\p"
	.string "A POKéMON can only learn so much,\n"
	.string "so pick your battles!$"
