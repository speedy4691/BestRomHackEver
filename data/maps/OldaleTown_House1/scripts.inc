OldaleTown_House1_MapScripts::
	.byte 0

@ Opening 06: SAFARI GUIDE STEVE gives the PACKAGE once, during Birch's errand. No other dialogue.
OldaleTown_House1_EventScript_Steve::
	lock
	faceplayer
	goto_if_set FLAG_RECEIVED_PACKAGE_FROM_STEVE, OldaleTown_House1_EventScript_SteveEnd
	goto_if_unset FLAG_OPENING_ERRAND_STARTED, OldaleTown_House1_EventScript_SteveEnd
	msgbox OldaleTown_House1_Text_SteveGivesPackage, MSGBOX_DEFAULT
	playfanfare MUS_OBTAIN_ITEM
	message OldaleTown_House1_Text_ReceivedPackage
	waitmessage
	waitfanfare
	additem ITEM_PACKAGE
	setflag FLAG_RECEIVED_PACKAGE_FROM_STEVE
	waitbuttonpress
	closemessage
OldaleTown_House1_EventScript_SteveEnd::
	release
	end

OldaleTown_House1_Text_LeftPokemonGoesOutFirst:
	.string "When a POKéMON battle starts, the one\n"
	.string "at the left of the list goes out first.\p"
	.string "So, when you get more POKéMON in your\n"
	.string "party, try switching around the order\l"
	.string "of your POKéMON.\p"
	.string "It could give you an advantage.$"


OldaleTown_House1_Text_SteveGivesPackage:
	.string "Hi there, {PLAYER}!\p"
	.string "Oh, I see, PROF. BIRCH sent you.\p"
	.string "Yes, him and I explored TARANGIRE\n"
	.string "NATIONAL PARK together many years\l"
	.string "ago.\p"
	.string "Here, give this to him and tell him to\n"
	.string "keep an eye out for “BLACK DEATH.”$"

OldaleTown_House1_Text_ReceivedPackage:
	.string "{PLAYER} received the PACKAGE!$"
