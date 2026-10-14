ScriptName OBodyScript extends Quest


OBodyScript Function Get() Global
	return Game.GetFormFromFile(0x00001800, "OBody.esp") as OBodyScript
EndFunction


Event OnInit()
	Stop()
EndEvent


Function OnLoad()
	Console("Game loaded, stopping old quest..")
	Stop()
EndFunction

int Function GetAPIVersion()
	return 2
endfunction 


Event OnGameLoad()
	OnLoad()
EndEvent


Function Console(String In)
	MiscUtil.PrintConsole("OBody NG: " + In)
EndFunction
