ScriptName OBodyNGScript extends Quest

bool Property ORefitEnabled auto
bool Property NippleSlidersORefitEnabled auto
bool Property NippleRandEnabled auto
bool Property GenitalRandEnabled auto
bool Property PerformanceMode auto
bool Property RespectfulMorphApplication auto
bool Property ForcePresetApplicationImmediate auto
bool Property LegacyStorageUtilUsageEnabled = true auto

int Property PresetKey auto

Actor property PlayerRef auto

Actor Property TargetOrPlayer
	Actor Function Get()
		Actor ret = Game.GetCurrentCrosshairRef() as Actor

		If !ret
			ret = PlayerRef
		EndIf

		Return ret
	EndFunction
EndProperty


Event OnInit()
	PlayerRef = Game.GetPlayer()
	Int femaleSize = OBodyNative.GetFemaleDatabaseSize()
	Int maleSize = OBodyNative.GetMaleDatabaseSize()

	Debug.Notification("OBody Installed: [F: " + femaleSize + "] [M: " + maleSize + "]")

    Quest OBodyMCMOldQuest = Game.GetFormFromFile(0x00001D69, "OBody.esp") as Quest

	if OBodyMCMOldQuest && OBodyMCMOldQuest.IsRunning()
        Console("Stopping old MCM quest....")
	 	OBodyMCMOldQuest.Stop()
	endif

	OnLoad()
EndEvent


Function OnLoad()
    Console("Game loaded...")
	PlayerRef = Game.GetPlayer()

	RegisterForKey(PresetKey)
	OBodyNative.SetORefit(ORefitEnabled)
	OBodyNative.SetNippleSlidersORefitEnabled(NippleSlidersORefitEnabled)
	OBodyNative.SetNippleRand(NippleRandEnabled)
	OBodyNative.SetGenitalRand(GenitalRandEnabled)
	OBodyNative.setPerformanceMode(PerformanceMode)
	OBodyNative.SetRespectfulMorphApplication(RespectfulMorphApplication)
	OBodyNative.SetLegacyStorageUtilUsageEnabled(LegacyStorageUtilUsageEnabled)

	string currentDistributionKey = StorageUtil.GetStringValue(none, "obody_ng_distribution_key", missing = "obody_processed")

	Console("Current distribution key is " + currentDistributionKey)

	OBodyNative.SetDistributionKey(currentDistributionKey)

	OBodyNative.RegisterForOBodyEvent(self as Quest)
EndFunction


Event OnActorGenerated(Actor akActor, string presetName)
	; Dear mod authors,
	; This method of preset assignment storage has been obsoleted by OBody's native code.
	; Please use `OBodyNative.GetPresetAssignedToActor` and `OBodyNative.AssignPresetToActor`
	; instead of manipulating this key directly.
	; Thank you.

	if LegacyStorageUtilUsageEnabled
		string actorPresetKey = "obody_" + akActor.GetFormID() + "_preset"
		StorageUtil.SetStringValue(none, actorPresetKey, presetName)
	endif
EndEvent


Function ResetDistribution()
	int distributionResetAmount = StorageUtil.GetIntValue(none, "obody_ng_distribution_reset_amount") + 1

	string newDistributionKey = "obody_processed" + distributionResetAmount

	StorageUtil.SetStringValue(none, "obody_ng_distribution_key", newDistributionKey)
	StorageUtil.SetIntValue(none, "obody_ng_distribution_reset_amount", distributionResetAmount)

	OBodyNative.SetDistributionKey(newDistributionKey)
EndFunction


Function updatePresetKey(int previousKey)
	UnregisterForKey(previousKey)
	RegisterForKey(PresetKey)
EndFunction


bool Function OBodyMenuOpen()
	return (Utility.IsInMenuMode() || UI.IsMenuOpen("console")) || UI.IsMenuOpen("Crafting Menu") || UI.IsMenuOpen("Dialogue Menu")
EndFunction


Event OnKeyDown(int KeyPress)
	If OBodyMenuOpen()
		Return
	EndIf

	if KeyPress == PresetKey
		ShowPresetMenu(TargetOrPlayer)
	endif
EndEvent


Function ShowPresetMenu(Actor act)
	Debug.Notification("Editing " + act.GetDisplayName())
	UIListMenu listMenu = UIExtensions.GetMenu("UIListMenu") as UIListMenu
	listMenu.ResetMenu()

	string currentPreset = OBodyNative.GetPresetAssignedToActor(act)

	if currentPreset == ""
		currentPreset = "Unknown/Unassigned Preset"
	endif

	string[] title = new String[4]
	title[0] = "-   OBody   -"
	title[1] = "Current Preset is:"
	title[2] = currentPreset
	title[3] = "-------------"

	string[] presets = OBodyNative.GetAllPossiblePresets(act)

	int l = presets.Length

	Console((l) + " presets found")

	int pagesNeeded

	If l > 124
		pagesNeeded = (l / 124) + 1

		int i = 0

		While i < pagesNeeded
			listMenu.AddEntryItem("OBody set " + (i + 1))
			i += 1
		EndWhile

		listMenu.OpenMenu(act)
		int num = listMenu.GetResultInt()
		If num < 0
			Return
		EndIf

		int startingPoint = num * 124
		int endPoint
		If num == (pagesNeeded - 1) ; last set
			endPoint = presets.Length - 1
		Else
			endPoint = startingPoint + 123
		EndIf

		listMenu.ResetMenu()
		presets = PapyrusUtil.SliceStringArray(presets, startingPoint, endPoint)
	EndIf

	presets = PapyrusUtil.MergeStringArray(title, presets)

	int i = 0
	int max = presets.length
	While (i < max)
		listMenu.AddEntryItem(presets[i])
		i += 1
	EndWhile

	listMenu.OpenMenu(act)
	string result = listMenu.GetResultString()

	int num = listMenu.GetResultInt()
	If !(num < 4)
		OBodyNative.ApplyPresetByName(act, result)
		Console("Applying: " + result)

		if LegacyStorageUtilUsageEnabled
			StorageUtil.SetStringValue(none, "obody_" + act.GetFormID() + "_preset", result)
		endif

		int me = ModEvent.Create("obody_manualchange")
		ModEvent.PushForm(me, act)
		ModEvent.Send(me)

		If ForcePresetApplicationImmediate
			Form armorIn32 = act.GetWornForm(0x00000004)

			If armorIn32 != none
				Act.UnequipItem(armorIn32, false, true)
				Utility.Wait(0.05)
				Act.EquipItem(armorIn32, false, true)
			Else
				Form armorNude = Game.GetFormFromFile(0x00000D6C, "OBody.esp")
				Act.EquipItem(armorNude, false, true)
				Utility.Wait(0.05)
				Act.UnequipItem(armorNude, false, true)
				Act.Removeitem(armorNude, 1, true)
			EndIf
		EndIf
	EndIf
EndFunction


Function Console(String In)
	MiscUtil.PrintConsole("OBody NG: " + In)
EndFunction
