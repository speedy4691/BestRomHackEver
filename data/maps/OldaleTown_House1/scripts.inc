OldaleTown_House1_MapScripts::
	.byte 0

@ Opening 06: SAFARI GUIDE STEVE gives the PACKAGE once, during Birch's errand. No other dialogue.
OldaleTown_House1_EventScript_Steve::
	lock
	faceplayer
	goto_if_set FLAG_RECEIVED_CAMERA, OldaleTown_House1_EventScript_SteveCameraTips
	goto_if_set FLAG_RECEIVED_PACKAGE_FROM_STEVE, OldaleTown_House1_EventScript_SteveEnd
	goto_if_unset FLAG_OPENING_ERRAND_STARTED, OldaleTown_House1_EventScript_SteveEnd
	@ Birch's ULTRA BALLS have to be used up first (any ULTRA BALL left in the BAG counts).
	checkitem ITEM_ULTRA_BALL
	goto_if_eq VAR_RESULT, TRUE, OldaleTown_House1_EventScript_SteveNotYet
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

OldaleTown_House1_EventScript_SteveCameraTips::
	msgbox OldaleTown_House1_Text_SteveCameraTips, MSGBOX_DEFAULT
	release
	end

OldaleTown_House1_EventScript_SteveNotYet::
	msgbox OldaleTown_House1_Text_SteveNotYet, MSGBOX_DEFAULT
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
	.string "Yes, he and I explored TARANGIRE\n"
	.string "NATIONAL PARK together many years\l"
	.string "ago.\p"
	.string "Here, give this to him and tell him to\n"
	.string "keep an eye out for “BLACK DEATH.”$"

OldaleTown_House1_Text_ReceivedPackage:
	.string "{PLAYER} received the PACKAGE!$"

OldaleTown_House1_Text_SteveNotYet:
	.string "Hi there, {PLAYER}!\p"
	.string "Oh, PROF. BIRCH sent you?\p"
	.string "You've still got some ULTRA BALLS\n"
	.string "there.\p"
	.string "Go put them to use on some wild\n"
	.string "POKéMON first, then come see me.$"

OldaleTown_House1_Text_SteveCameraTips:
	.string "Ah, I see BIRCH BOI gave you the\n"
	.string "CAMERA.\p"
	.string "Here's how it works.\p"
	.string "In a wild battle, press L to bring up\n"
	.string "the CAMERA.\p"
	.string "Move the frame with the + Control\n"
	.string "Pad, and zoom in with L and out\l"
	.string "with R.\p"
	.string "Press A to take the picture, or B to\n"
	.string "put the CAMERA away.\p"
	.string "You get 3 shots per battle, so make\n"
	.string "them count!\p"
	.string "For the best pictures, keep the\n"
	.string "POKéMON in the middle, get it big in\l"
	.string "the frame, and catch it mid-move.\p"
	.string "One more thing: the flash startles\n"
	.string "them, so expect a nasty hit right\l"
	.string "after you take a picture.$"
