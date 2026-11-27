Route102_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, Route102_OnTransition
	.byte 0

Route102_OnTransition:
	@ Opening 12: STEVE is only on this map during his run-up.
	setflag FLAG_HIDE_ROUTE102_STEVE
	end

@ Opening 12: on the first step onto ROUTE 102 after beating ARTIST VINNY (VAR_OLDALE_TOWN_STATE 1, trigger at
@ 49,10 / 49,11), STEVE runs in from the OLDALE side, explains the CAMERA, and runs back. Plays once (state 2).
@ He starts at 57 (inside the OLDALE TOWN strip, just off the right of the screen) on whichever of rows 10/11 is
@ clear of people (OLDALE objects such as VINNY stay on screen after crossing), runs to 50, and steps up or
@ down if needed to end beside the player. He leaves the same way (GetRoute102SteveRunUpPath).
Route102_EventScript_SteveRunUp::
	lockall
	goto_if_set FLAG_STEVE_CAMERA_TALK_DONE, Route102_EventScript_SteveRunUpDone
	getplayerxy VAR_0x8004, VAR_0x8005
	msgbox Route102_Text_SteveWaitUp, MSGBOX_DEFAULT
	closemessage
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterRight
	waitmovement 0
	special GetRoute102SteveRunUpPath
	call_if_eq VAR_0x8006, 11, Route102_EventScript_SteveStartRow11
	clearflag FLAG_HIDE_ROUTE102_STEVE
	addobject LOCALID_ROUTE102_STEVE
	applymovement LOCALID_ROUTE102_STEVE, Route102_Movement_SteveRunIn
	waitmovement 0
	call_if_eq VAR_0x8007, 1, Route102_EventScript_SteveStepDown
	call_if_eq VAR_0x8007, 2, Route102_EventScript_SteveStepUp
	msgbox Route102_Text_SteveCameraTalk, MSGBOX_DEFAULT
	closemessage
	call_if_eq VAR_0x8007, 1, Route102_EventScript_SteveStepBackUp
	call_if_eq VAR_0x8007, 2, Route102_EventScript_SteveStepBackDown
	applymovement LOCALID_ROUTE102_STEVE, Route102_Movement_SteveRunOut
	waitmovement 0
	removeobject LOCALID_ROUTE102_STEVE
	setflag FLAG_STEVE_CAMERA_TALK_DONE
Route102_EventScript_SteveRunUpDone::
	setvar VAR_OLDALE_TOWN_STATE, 2
	releaseall
	end

Route102_EventScript_SteveStartRow11::
	setobjectxyperm LOCALID_ROUTE102_STEVE, 57, 11
	return

Route102_EventScript_SteveStepDown::
	applymovement LOCALID_ROUTE102_STEVE, Route102_Movement_SteveStepDown
	waitmovement 0
	return

Route102_EventScript_SteveStepUp::
	applymovement LOCALID_ROUTE102_STEVE, Route102_Movement_SteveStepUp
	waitmovement 0
	return

Route102_EventScript_SteveStepBackUp::
	applymovement LOCALID_ROUTE102_STEVE, Common_Movement_WalkUp
	waitmovement 0
	return

Route102_EventScript_SteveStepBackDown::
	applymovement LOCALID_ROUTE102_STEVE, Common_Movement_WalkDown
	waitmovement 0
	return

Route102_Movement_SteveStepDown:
	walk_fast_down
	face_left
	step_end

Route102_Movement_SteveStepUp:
	walk_fast_up
	face_left
	step_end

Route102_Movement_SteveRunIn:
	walk_fast_left
	walk_fast_left
	walk_fast_left
	walk_fast_left
	walk_fast_left
	walk_fast_left
	walk_fast_left
	step_end

Route102_Movement_SteveRunOut:
	walk_fast_right
	walk_fast_right
	walk_fast_right
	walk_fast_right
	walk_fast_right
	walk_fast_right
	walk_fast_right
	step_end

Route102_Text_SteveWaitUp:
	.string "STEVE: {PLAYER}! Wait up!$"

Route102_Text_SteveCameraTalk:
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


Route102_EventScript_LittleBoy::
	msgbox Route102_Text_ImNotVeryTall, MSGBOX_NPC
	end

Route102_EventScript_RouteSignOldale::
	msgbox Route102_Text_RouteSignOldale, MSGBOX_SIGN
	end

Route102_EventScript_RouteSignPetalburg::
	msgbox Route102_Text_RouteSignPetalburg, MSGBOX_SIGN
	end

Route102_EventScript_Boy::
	msgbox Route102_Text_CatchWholeBunchOfPokemon, MSGBOX_NPC
	end

Route102_EventScript_Calvin::
	trainerbattle_single TRAINER_CALVIN_1, Route102_Text_CalvinIntro, Route102_Text_CalvinDefeated, Route102_EventScript_CalvinRegisterMatchCallAfterBattle
	specialvar VAR_RESULT, ShouldTryRematchBattle
	goto_if_eq VAR_RESULT, TRUE, Route102_EventScript_CalvinRematch
	setvar VAR_0x8004, TRAINER_CALVIN_1
	specialvar VAR_RESULT, IsTrainerRegistered
	goto_if_eq VAR_RESULT, FALSE, Route102_EventScript_CalvinTryRegister
	msgbox Route102_Text_CalvinPostBattle, MSGBOX_DEFAULT
	release
	end

Route102_EventScript_CalvinRegisterMatchCallAfterBattle::
	special PlayerFaceTrainerAfterBattle
	waitmovement 0
	goto_if_set FLAG_HAS_MATCH_CALL, Route102_EventScript_CalvinRegisterMatchCall
	release
	end

Route102_EventScript_CalvinRegisterMatchCall::
	msgbox Route102_Text_CalvinRegisterShort, MSGBOX_DEFAULT
	register_matchcall TRAINER_CALVIN_1
	release
	end

Route102_EventScript_CalvinTryRegister::
	goto_if_set FLAG_HAS_MATCH_CALL, Route102_EventScript_CalvinRegister
	msgbox Route102_Text_CalvinPostBattle, MSGBOX_DEFAULT
	release
	end

Route102_EventScript_CalvinRegister::
	msgbox Route102_Text_CalvinRegister, MSGBOX_DEFAULT
	register_matchcall TRAINER_CALVIN_1
	release
	end

Route102_EventScript_CalvinRematch::
	trainerbattle_rematch TRAINER_CALVIN_1, Route102_Text_CalvinRematchIntro, Route102_Text_CalvinRematchDefeated
	msgbox Route102_Text_CalvinRematchPostBattle, MSGBOX_AUTOCLOSE
	end

Route102_EventScript_Rick::
	trainerbattle_single TRAINER_RICK, Route102_Text_RickIntro, Route102_Text_RickDefeated
	msgbox Route102_Text_RickPostBattle, MSGBOX_AUTOCLOSE
	end

Route102_EventScript_Tiana::
	trainerbattle_single TRAINER_TIANA, Route102_Text_TianaIntro, Route102_Text_TianaDefeated
	msgbox Route102_Text_TianaPostBattle, MSGBOX_AUTOCLOSE
	end

Route102_EventScript_Allen::
	trainerbattle_single TRAINER_ALLEN, Route102_Text_AllenIntro, Route102_Text_AllenDefeated
	msgbox Route102_Text_AllenPostBattle, MSGBOX_AUTOCLOSE
	end

Route102_Text_WatchMeCatchPokemon:
	.string "WALLY: {PLAYER}…\n"
	.string "POKéMON hide in tall grass like this,\l"
	.string "don't they?\p"
	.string "Please watch me and see if I can\n"
	.string "catch one properly.\p"
	.string "…Whoa!$"

Route102_Text_WallyIDidIt:
	.string "WALLY: I did it… It's my…\n"
	.string "My POKéMON!$"

Route102_Text_LetsGoBack:
	.string "{PLAYER}, thank you!\n"
	.string "Let's go back to the GYM!$"

Route102_Text_ImNotVeryTall:
	.string "I'm…not very tall, so I sink right\n"
	.string "into tall grass.\p"
	.string "The grass goes up my nose and…\n"
	.string "Fwafwafwafwafwa…\p"
	.string "Fwatchoo!$"

Route102_Text_CatchWholeBunchOfPokemon:
	.string "I'm going to catch a whole bunch of\n"
	.string "POKéMON!$"

Route102_Text_RouteSignOldale:
	.string "ROUTE 102\n"
	.string "{RIGHT_ARROW} OLDALE TOWN$"

Route102_Text_RouteSignPetalburg:
	.string "ROUTE 102\n"
	.string "{LEFT_ARROW} PETALBURG CITY$"
