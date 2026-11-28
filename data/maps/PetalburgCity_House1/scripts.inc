PetalburgCity_House1_MapScripts::
	.byte 0

PetalburgCity_House1_EventScript_Man::
	msgbox PetalburgCity_House1_Text_TravelingIsWonderful, MSGBOX_NPC
	end

PetalburgCity_House1_EventScript_Woman::
	msgbox PetalburgCity_House1_Text_GoOnAdventure, MSGBOX_NPC
	end

PetalburgCity_House1_Text_TravelingIsWonderful:
	.string "Traveling is wonderful!\p"
	.string "When I was young, I roamed the seas\n"
	.string "and the mountains!$"

PetalburgCity_House1_Text_GoOnAdventure:
	.string "Sigh…\p"
	.string "I wish I could go on an adventure\n"
	.string "with some POKéMON…\p"
	.string "Crawl through some damp grass…\n"
	.string "Climb rocky, rugged mountains…\p"
	.string "Cross the raging seas…\n"
	.string "Wander about in dark caves…\p"
	.string "And, sometimes, even get a little\n"
	.string "homesick…\p"
	.string "It must be fabulous to travel!$"


@ "W1" on Chase's Petalburg map: mother of a TEAM AQUA member (replaces the girl who wished for an adventure).
PetalburgCity_House1_EventScript_AquaMom::
	msgbox PetalburgCity_House1_Text_SonJoinedTeamAqua, MSGBOX_NPC
	end

PetalburgCity_House1_Text_SonJoinedTeamAqua:
	.string "My son joined TEAM AQUA last year.\p"
	.string "He says they're cleaning up\n"
	.string "the oceans.\p"
	.string "He doesn't call as much as he used to,\n"
	.string "but he always sends a postcard.$"
