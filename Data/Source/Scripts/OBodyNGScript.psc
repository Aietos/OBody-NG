ScriptName OBodyNGScript extends Quest

bool Property ORefitEnabled auto
bool Property NippleSlidersORefitEnabled auto
bool Property NippleRandEnabled auto
bool Property GenitalRandEnabled auto
bool Property PerformanceMode auto
bool Property ForcePresetApplicationImmediate auto
bool Property LegacyStorageUtilUsageEnabled auto

int Property PresetKey auto

Actor property PlayerRef auto


Event OnInit()
	PlayerRef = Game.GetPlayer()

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

	OBodyNative.SetORefit(ORefitEnabled)
	OBodyNative.SetNippleSlidersORefitEnabled(NippleSlidersORefitEnabled)
	OBodyNative.SetNippleRand(NippleRandEnabled)
	OBodyNative.SetGenitalRand(GenitalRandEnabled)
	OBodyNative.setPerformanceMode(PerformanceMode)
	OBodyNative.SetLegacyStorageUtilUsageEnabled(LegacyStorageUtilUsageEnabled)
	OBodyNative.UpdatePresetMenuKey(PresetKey)
	OBodyNative.SetForcePresetApplicationImmediate(ForcePresetApplicationImmediate)

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


Event OnActorPresetChangedWithoutGeneration(Actor akActor, String asPresetName)
    If !akActor
        Return
    EndIf

    If ForcePresetApplicationImmediate
		Form armorIn32 = akActor.GetWornForm(0x00000004)

		If armorIn32 != none
			akActor.UnequipItem(armorIn32, false, true)
			Utility.Wait(0.01)
			akActor.EquipItem(armorIn32, false, true)
		Else
			Form armorNude = Game.GetFormFromFile(0x00000D6C, "OBody.esp")
			akActor.EquipItem(armorNude, false, true)
			Utility.Wait(0.01)
			akActor.UnequipItem(armorNude, false, true)
			akActor.Removeitem(armorNude, 1, true)
		EndIf
	EndIf
EndEvent


Function ResetDistribution()
	int distributionResetAmount = StorageUtil.GetIntValue(none, "obody_ng_distribution_reset_amount") + 1

	string newDistributionKey = "obody_processed" + distributionResetAmount

	StorageUtil.SetStringValue(none, "obody_ng_distribution_key", newDistributionKey)
	StorageUtil.SetIntValue(none, "obody_ng_distribution_reset_amount", distributionResetAmount)

	OBodyNative.SetDistributionKey(newDistributionKey)
EndFunction


Function Console(String In)
	MiscUtil.PrintConsole("OBody NG: " + In)
EndFunction
