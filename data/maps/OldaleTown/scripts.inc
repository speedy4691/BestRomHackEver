OldaleTown_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, OldaleTown_OnTransition
	.byte 0

OldaleTown_OnTransition:
	call Common_EventScript_SetupRivalGfxId
	setflag FLAG_VISITED_OLDALE_TOWN
	@ Opening 12: STEVE is only on this map during his run-up (also hides him on saves from before this flag).
	setflag FLAG_HIDE_OLDALE_TOWN_STEVE
	@ Access gates row 2: VINNY blocks the west exit until he's beaten; he only battles once the red-eyed
	@ WURMPLE on Route 103 is photographed (before that: his vanilla lines).
	call_if_unset FLAG_DEFEATED_OLDALE_FOOTPRINTS_MAN, OldaleTown_EventScript_BlockWestEntrance
	call_if_unset FLAG_RECEIVED_POTION_OLDALE, OldaleTown_EventScript_MoveMartEmployee
	call_if_set FLAG_DEFEATED_OLDALE_FOOTPRINTS_MAN, OldaleTown_EventScript_SetOldaleState
	end

@ VAR_OLDALE_TOWN_STATE 0 arms the blocked-path trigger at the west exit; 1 turns it off.
OldaleTown_EventScript_SetOldaleState::
	setvar VAR_OLDALE_TOWN_STATE, 1
	return

OldaleTown_EventScript_BlockWestEntrance::
	setobjectxyperm LOCALID_FOOTPRINTS_MAN, 1, 11
	setobjectmovementtype LOCALID_FOOTPRINTS_MAN, MOVEMENT_TYPE_FACE_LEFT
	return

OldaleTown_EventScript_MoveMartEmployee::
	setobjectxyperm LOCALID_OLDALE_MART_EMPLOYEE, 13, 14
	setobjectmovementtype LOCALID_OLDALE_MART_EMPLOYEE, MOVEMENT_TYPE_FACE_DOWN
	return

OldaleTown_EventScript_TownSign::
	msgbox OldaleTown_Text_TownSign, MSGBOX_SIGN
	end

OldaleTown_EventScript_Girl::
	msgbox OldaleTown_Text_SavingMyProgress, MSGBOX_NPC
	end

OldaleTown_EventScript_MartEmployee::
	lock
	faceplayer
	goto_if_set FLAG_RECEIVED_POTION_OLDALE, OldaleTown_EventScript_RareCandyBestSeller
	goto_if_set FLAG_TEMP_1, OldaleTown_EventScript_ExplainPotion
	setflag FLAG_TEMP_1
	playbgm MUS_FOLLOW_ME, FALSE
	msgbox OldaleTown_Text_IWorkAtPokemonMart, MSGBOX_DEFAULT
	closemessage
	switch VAR_FACING
	case DIR_SOUTH, OldaleTown_EventScript_GoToMartSouth
	case DIR_NORTH, OldaleTown_EventScript_GoToMartNorth
	case DIR_EAST, OldaleTown_EventScript_GoToMartEast
	end

OldaleTown_EventScript_GoToMartSouth::
	applymovement LOCALID_OLDALE_MART_EMPLOYEE, OldaleTown_Movement_EmployeeSouth
	applymovement LOCALID_PLAYER, OldaleTown_Movement_PlayerSouth
	waitmovement 0
	goto OldaleTown_EventScript_ExplainPokemonMart
	end

OldaleTown_EventScript_GoToMartNorth::
	applymovement LOCALID_OLDALE_MART_EMPLOYEE, OldaleTown_Movement_EmployeeNorth
	applymovement LOCALID_PLAYER, OldaleTown_Movement_PlayerNorth
	waitmovement 0
	goto OldaleTown_EventScript_ExplainPokemonMart
	end

OldaleTown_EventScript_GoToMartEast::
	applymovement LOCALID_PLAYER, OldaleTown_Movement_PlayerEast
	applymovement LOCALID_OLDALE_MART_EMPLOYEE, OldaleTown_Movement_EmployeeEast
	waitmovement 0
	goto OldaleTown_EventScript_ExplainPokemonMart
	end

OldaleTown_EventScript_ExplainPokemonMart::
	msgbox OldaleTown_Text_ThisIsAPokemonMart, MSGBOX_DEFAULT
	giveitem ITEM_POTION
	goto_if_eq VAR_RESULT, FALSE, OldaleTown_EventScript_BagIsFull
	msgbox OldaleTown_Text_PotionExplanation, MSGBOX_DEFAULT
	setflag FLAG_RECEIVED_POTION_OLDALE
	fadedefaultbgm
	release
	end

OldaleTown_EventScript_RareCandyBestSeller::
	msgbox OldaleTown_Text_RareCandyBestSeller, MSGBOX_DEFAULT
	release
	end

OldaleTown_EventScript_ExplainPotion::
	msgbox OldaleTown_Text_PotionExplanation, MSGBOX_DEFAULT
	release
	end

OldaleTown_EventScript_BagIsFull::
	msgbox gText_TooBadBagIsFull, MSGBOX_DEFAULT
	fadedefaultbgm
	release
	end

OldaleTown_Movement_EmployeeEast:
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_in_place_faster_down
	step_end

OldaleTown_Movement_EmployeeSouth:
	walk_left
	walk_up
	walk_up
	walk_right
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_in_place_faster_down
	step_end

OldaleTown_Movement_EmployeeNorth:
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_in_place_faster_down
	step_end

OldaleTown_Movement_Unknown1:
	walk_up
	walk_up
	walk_right
	walk_right
	walk_right
	walk_right
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	delay_8
	walk_in_place_faster_down
	step_end

OldaleTown_Movement_PlayerEast:
	walk_right
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	step_end

OldaleTown_Movement_PlayerSouth:
	delay_16
	delay_16
	delay_16
	delay_16
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	step_end

OldaleTown_Movement_PlayerNorth:
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	step_end

OldaleTown_Movement_Unknown2:
	walk_left
	walk_up
	walk_up
	walk_right
	walk_right
	walk_right
	walk_right
	walk_up
	walk_up
	walk_up
	walk_up
	step_end

OldaleTown_EventScript_FootprintsMan::
	lock
	faceplayer
	goto_if_set FLAG_DEFEATED_OLDALE_FOOTPRINTS_MAN, OldaleTown_EventScript_NotBlockingPath
	goto_if_set FLAG_PHOTOGRAPHED_ROUTE103_WURMPLE, OldaleTown_EventScript_BattleVinny
	msgbox OldaleTown_Text_DiscoveredFootprints, MSGBOX_DEFAULT
	closemessage
	applymovement LOCALID_FOOTPRINTS_MAN, Common_Movement_FaceOriginalDirection
	waitmovement 0
	release
	end

OldaleTown_EventScript_BlockedPath::
	lockall
	applymovement LOCALID_PLAYER, OldaleTown_Movement_PlayerStepBack
	applymovement LOCALID_FOOTPRINTS_MAN, OldaleTown_Movement_BackUp
	waitmovement 0
	goto_if_set FLAG_PHOTOGRAPHED_ROUTE103_WURMPLE, OldaleTown_EventScript_BattleVinny
	msgbox OldaleTown_Text_WaitDontComeInHere, MSGBOX_DEFAULT
	closemessage
	applymovement LOCALID_FOOTPRINTS_MAN, OldaleTown_Movement_ReturnToOriginalPosition
	waitmovement 0
	releaseall
	end

@ ARTIST VINNY (Trainers row 1830): stops the player at the exit, or when talked to. Losing is a normal
@ whiteout and he blocks again next time. Winning opens the exit: he stays where he is (at the exit trigger he
@ has already stepped back to 2,10, east of the player; when talked to he's at 1,11, beside the open row 10),
@ so no step aside is needed. Next visit he's at his usual spot (8,9) from the map data.
OldaleTown_EventScript_BattleVinny::
	msgbox OldaleTown_Text_VinnyIntro, MSGBOX_DEFAULT
	trainerbattle_no_intro TRAINER_OLDALE_FOOTPRINTS_MAN, OldaleTown_Text_VinnyDefeat
	setflag FLAG_DEFEATED_OLDALE_FOOTPRINTS_MAN
	setvar VAR_OLDALE_TOWN_STATE, 1
	goto_if_unset FLAG_STEVE_CAMERA_TALK_DONE, OldaleTown_EventScript_SteveRunUp
	releaseall
	end

@ Opening 12: right after VINNY, STEVE comes out of his house (door 5,7), calls out, runs up beside the player,
@ explains the CAMERA, and goes back inside. Plays once. Where the player stands depends on how VINNY was
@ battled: at the exit trigger the player is at 1,10 with VINNY at 2,10; when talked to, VINNY is at 1,11 and the
@ player is at 1,10, 2,11 or 0,11. Each case has its own path so STEVE never walks through anyone.
OldaleTown_EventScript_SteveRunUp::
	lockall
	getplayerxy VAR_0x8004, VAR_0x8005
	getobjectxy LOCALID_FOOTPRINTS_MAN, CURRENT_POSITION, VAR_0x8006, VAR_0x8007
	setvar VAR_0x8008, 0
	call_if_eq VAR_0x8004, 0, OldaleTown_EventScript_SteveCaseWest
	call_if_eq VAR_0x8004, 2, OldaleTown_EventScript_SteveCaseBesideVinny
	call_if_eq VAR_0x8004, 1, OldaleTown_EventScript_SteveCaseAboveVinny
	opendoor 5, 7
	waitdooranim
	clearflag FLAG_HIDE_OLDALE_TOWN_STEVE
	addobject LOCALID_OLDALE_STEVE
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveExitHouse
	waitmovement 0
	closedoor 5, 7
	waitdooranim
	msgbox OldaleTown_Text_SteveWaitUp, MSGBOX_DEFAULT
	closemessage
	call_if_eq VAR_0x8008, 1, OldaleTown_EventScript_SteveApproach1
	call_if_eq VAR_0x8008, 2, OldaleTown_EventScript_SteveApproach2
	call_if_eq VAR_0x8008, 3, OldaleTown_EventScript_SteveApproach3
	call_if_eq VAR_0x8008, 4, OldaleTown_EventScript_SteveApproach4
	msgbox OldaleTown_Text_SteveCameraTalk, MSGBOX_DEFAULT
	closemessage
	call_if_eq VAR_0x8008, 0, OldaleTown_EventScript_SteveReturn0
	call_if_eq VAR_0x8008, 1, OldaleTown_EventScript_SteveReturn1
	call_if_eq VAR_0x8008, 2, OldaleTown_EventScript_SteveReturn2
	call_if_eq VAR_0x8008, 3, OldaleTown_EventScript_SteveReturn3
	call_if_eq VAR_0x8008, 4, OldaleTown_EventScript_SteveReturn4
	opendoor 5, 7
	waitdooranim
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveEnterHouse
	waitmovement 0
	removeobject LOCALID_OLDALE_STEVE
	closedoor 5, 7
	waitdooranim
	setflag FLAG_STEVE_CAMERA_TALK_DONE
	releaseall
	end

@ Player at 0,11 (talked to VINNY from the west): STEVE stops at 0,10, above the player.
OldaleTown_EventScript_SteveCaseWest::
	setvar VAR_0x8008, 4
	return

@ Player at 2,11 (talked to VINNY from the east): STEVE stops at 3,11, east of the player.
OldaleTown_EventScript_SteveCaseBesideVinny::
	setvar VAR_0x8008, 3
	return

@ Player at 1,10: VINNY at 2,10 (exit trigger) -> STEVE stops at 1,11, below the player;
@ VINNY at 1,11 (talked to from above) -> STEVE stops at 2,10, east of the player.
OldaleTown_EventScript_SteveCaseAboveVinny::
	setvar VAR_0x8008, 1
	call_if_eq VAR_0x8006, 2, OldaleTown_EventScript_SteveCaseVinnyEast
	return

OldaleTown_EventScript_SteveCaseVinnyEast::
	setvar VAR_0x8008, 2
	return

OldaleTown_EventScript_SteveApproach1::
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveApproach1
	waitmovement 0
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterRight
	waitmovement 0
	return

OldaleTown_EventScript_SteveApproach2::
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveApproach2
	waitmovement 0
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterDown
	waitmovement 0
	return

OldaleTown_EventScript_SteveApproach3::
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveApproach3
	waitmovement 0
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterRight
	waitmovement 0
	return

OldaleTown_EventScript_SteveApproach4::
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveApproach4
	waitmovement 0
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterUp
	waitmovement 0
	return

@ Unknown spot (shouldn't happen): STEVE talks from his doorstep.
OldaleTown_EventScript_SteveReturn0::
	applymovement LOCALID_OLDALE_STEVE, Common_Movement_WalkInPlaceFasterUp
	waitmovement 0
	return

OldaleTown_EventScript_SteveReturn1::
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveReturn1
	waitmovement 0
	return

OldaleTown_EventScript_SteveReturn2::
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveReturn2
	waitmovement 0
	return

OldaleTown_EventScript_SteveReturn3::
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveReturn3
	waitmovement 0
	return

OldaleTown_EventScript_SteveReturn4::
	applymovement LOCALID_OLDALE_STEVE, OldaleTown_Movement_SteveReturn4
	waitmovement 0
	return

OldaleTown_Movement_SteveExitHouse:
	walk_down
	step_end

@ From 5,8 to 2,10.
OldaleTown_Movement_SteveApproach1:
	walk_fast_down
	walk_fast_down
	walk_fast_left
	walk_fast_left
	walk_fast_left
	step_end

@ From 5,8 to 1,11, then face up.
OldaleTown_Movement_SteveApproach2:
	walk_fast_down
	walk_fast_down
	walk_fast_down
	walk_fast_left
	walk_fast_left
	walk_fast_left
	walk_fast_left
	face_up
	step_end

@ From 5,8 to 3,11.
OldaleTown_Movement_SteveApproach3:
	walk_fast_down
	walk_fast_down
	walk_fast_down
	walk_fast_left
	walk_fast_left
	step_end

@ From 5,8 to 0,10, then face down.
OldaleTown_Movement_SteveApproach4:
	walk_fast_down
	walk_fast_down
	walk_fast_left
	walk_fast_left
	walk_fast_left
	walk_fast_left
	walk_fast_left
	face_down
	step_end

@ Back to 5,8 below his door.
OldaleTown_Movement_SteveReturn1:
	walk_right
	walk_right
	walk_right
	walk_up
	walk_up
	step_end

OldaleTown_Movement_SteveReturn2:
	walk_right
	walk_right
	walk_right
	walk_right
	walk_up
	walk_up
	walk_up
	step_end

OldaleTown_Movement_SteveReturn3:
	walk_right
	walk_right
	walk_up
	walk_up
	walk_up
	step_end

OldaleTown_Movement_SteveReturn4:
	walk_right
	walk_right
	walk_right
	walk_right
	walk_right
	walk_up
	walk_up
	step_end

OldaleTown_Movement_SteveEnterHouse:
	walk_up
	step_end

OldaleTown_EventScript_NotBlockingPath::
	msgbox OldaleTown_Text_FinishedSketchingFootprints, MSGBOX_DEFAULT
	release
	end

OldaleTown_EventScript_Rival::
	lockall
	applymovement LOCALID_OLDALE_RIVAL, Common_Movement_FacePlayer
	waitmovement 0
	setvar VAR_0x8009, 0
	goto OldaleTown_EventScript_ShowRivalMessage
	end

OldaleTown_EventScript_RivalTrigger1::
	lockall
	applymovement LOCALID_OLDALE_RIVAL, OldaleTown_Movement_RivalApproachPlayer1
	waitmovement 0
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterRight
	waitmovement 0
	setvar VAR_0x8009, 1
	goto OldaleTown_EventScript_ShowRivalMessage
	end

OldaleTown_EventScript_RivalTrigger2::
	lockall
	applymovement LOCALID_OLDALE_RIVAL, OldaleTown_Movement_RivalApproachPlayer2
	waitmovement 0
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterRight
	waitmovement 0
	setvar VAR_0x8009, 1
	goto OldaleTown_EventScript_ShowRivalMessage
	end

OldaleTown_EventScript_RivalTrigger3::
	lockall
	applymovement LOCALID_OLDALE_RIVAL, OldaleTown_Movement_RivalApproachPlayer3
	waitmovement 0
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterRight
	waitmovement 0
	setvar VAR_0x8009, 1
	goto OldaleTown_EventScript_ShowRivalMessage
	end

OldaleTown_EventScript_ShowRivalMessage::
	checkplayergender
	goto_if_eq VAR_RESULT, MALE, OldaleTown_EventScript_ShowMayMessage
	goto_if_eq VAR_RESULT, FEMALE, OldaleTown_EventScript_ShowBrendanMessage
	end

OldaleTown_EventScript_ShowMayMessage::
	msgbox OldaleTown_Text_MayLetsGoBack, MSGBOX_DEFAULT
	goto OldaleTown_EventScript_RivalFinish
	end

OldaleTown_EventScript_ShowBrendanMessage::
	msgbox OldaleTown_Text_BrendanLetsGoBack, MSGBOX_DEFAULT
	goto OldaleTown_EventScript_RivalFinish
	end

OldaleTown_EventScript_RivalFinish::
	closemessage
	call_if_eq VAR_0x8009, 0, OldaleTown_EventScript_DoExitMovement1
	call_if_eq VAR_0x8009, 1, OldaleTown_EventScript_DoExitMovement2
	applymovement LOCALID_OLDALE_RIVAL, OldaleTown_Movement_RivalExit
	waitmovement 0
	removeobject LOCALID_OLDALE_RIVAL
	setvar VAR_OLDALE_RIVAL_STATE, 2
	setflag FLAG_HIDE_OLDALE_TOWN_RIVAL
	releaseall
	end

OldaleTown_EventScript_DoExitMovement1::
	goto_if_ne VAR_FACING, DIR_SOUTH, OldaleTown_EventScript_DoExitMovement2
	applymovement LOCALID_OLDALE_RIVAL, OldaleTown_Movement_RivalExit
	waitmovement 0
	return

OldaleTown_EventScript_DoExitMovement2::
	applymovement LOCALID_PLAYER, OldaleTown_Movement_WatchRivalExit
	applymovement LOCALID_OLDALE_RIVAL, OldaleTown_Movement_RivalExit
	waitmovement 0
	return

OldaleTown_Movement_RivalApproachPlayer1:
	walk_left
	walk_left
	step_end

OldaleTown_Movement_RivalApproachPlayer2:
	walk_left
	step_end

OldaleTown_Movement_RivalApproachPlayer3:
	face_left
	step_end

OldaleTown_Movement_RivalExit:
	walk_down
	walk_down
	walk_down
	walk_down
	walk_down
	walk_down
	step_end

OldaleTown_Movement_WatchRivalExit:
	delay_8
	delay_4
	walk_in_place_faster_down
	step_end

OldaleTown_Movement_PlayerStepBack:
	delay_8
	walk_right
	step_end

OldaleTown_Movement_BackUp:
	walk_fast_up
	walk_in_place_faster_left
	lock_facing_direction
	walk_right
	unlock_facing_direction
	step_end

OldaleTown_Movement_ReturnToOriginalPosition:
	walk_down
	walk_left
	step_end

OldaleTown_Text_SavingMyProgress:
	.string "DEVON CORP is hiring in RUSTBORO\n"
	.string "again.\p"
	.string "They pay really well, but nobody\n"
	.string "ever says what they're working on.$"

OldaleTown_Text_RareCandyBestSeller:
	.string "RARE CANDIES are our best seller.\p"
	.string "DEVON CORP makes them from the EXP\n"
	.string "in BILL's PC, you know.\p"
	.string "They're not cheap, though!$"

OldaleTown_Text_IWorkAtPokemonMart:
	.string "Hi!\n"
	.string "I work at a POKéMON MART.\p"
	.string "Can I get you to come with me?$"

OldaleTown_Text_ThisIsAPokemonMart:
	.string "This is a POKéMON MART.\n"
	.string "Just look for our blue roof.\p"
	.string "We sell a variety of goods including\n"
	.string "POKé BALLS for catching POKéMON.\p"
	.string "Here, I'd like you to have this as\n"
	.string "a promotional item.$"

OldaleTown_Text_PotionExplanation:
	.string "A POTION can be used anytime, so it's\n"
	.string "even more useful than a POKéMON CENTER\l"
	.string "in certain situations.$"

OldaleTown_Text_WaitDontComeInHere:
	.string "Aaaaah! Wait!\n"
	.string "Please don't come in here.\p"
	.string "I just discovered the footprints of\n"
	.string "a rare POKéMON!\p"
	.string "Wait until I finish sketching\n"
	.string "them, okay?$"

OldaleTown_Text_DiscoveredFootprints:
	.string "I just discovered the footprints of\n"
	.string "a rare POKéMON!\p"
	.string "Wait until I finish sketching\n"
	.string "them, okay?$"

OldaleTown_Text_FinishedSketchingFootprints:
	.string "I finished sketching the footprints of\n"
	.string "a rare POKéMON.\p"
	.string "But it turns out they were only my\n"
	.string "own footprints…$"

OldaleTown_Text_MayLetsGoBack:
	.string "MAY: {PLAYER}{KUN}!\n"
	.string "Over here!\l"
	.string "Let's hurry home!$"

OldaleTown_Text_BrendanLetsGoBack:
	.string "BRENDAN: I'm heading back to my dad's\n"
	.string "LAB now.\l"
	.string "{PLAYER}, you should hustle back, too.$"

OldaleTown_Text_TownSign:
	.string "OLDALE TOWN\n"
	.string "“Where things start off scarce.”$"


OldaleTown_Text_SteveWaitUp:
	.string "STEVE: {PLAYER}! Wait up!$"

OldaleTown_Text_SteveCameraTalk:
	.string "STEVE: BIRCH BOI told me you got your\n"
	.string "first picture. Nice work!\p"
	.string "There's more to that CAMERA than\n"
	.string "research, you know.\p"
	.string "Once you've taken a picture of a\n"
	.string "POKéMON, you'll know more about every\l"
	.string "one of that kind you meet after.\p"
	.string "In battle, press SELECT to see its\n"
	.string "nature and ability.\p"
	.string "Take a good picture and you'll see\n"
	.string "its IVs (how strong it could get)\l"
	.string "too.\p"
	.string "The better your picture, the better\n"
	.string "the ones you find, and they'll be\l"
	.string "easier to catch too!\p"
	.string "Well, that's all from me.\n"
	.string "Say hi to BIRCH BOI for me!$"

OldaleTown_Text_VinnyIntro:
	.string "Hey buddy, good art takes time, so why\n"
	.string "don't you just buzz off…\p"
	.string "Ah, you know what… screw it, we are\n"
	.string "battling!$"

OldaleTown_Text_VinnyDefeat:
	.string "Maybe I should go into finance\n"
	.string "instead…$"
