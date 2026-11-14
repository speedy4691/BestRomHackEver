@ L1 route lock. The text is set up in gStringVar4 by src/route_lock.c before these run.

RouteLock_EventScript_Message::
	lockall
	msgbox gStringVar4, MSGBOX_DEFAULT
	releaseall
	end

RouteLock_EventScript_Cleared::
	lockall
	playfanfare MUS_LEVEL_UP
	message gStringVar4
	waitmessage
	waitfanfare
	waitbuttonpress
	closemessage
	releaseall
	end
