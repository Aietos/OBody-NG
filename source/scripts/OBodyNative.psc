ScriptName OBodyNative

Function GenActor(Actor a_actor) Global Native

; Example: "Data\\CalienteTools\\BodySlide\\SliderPresets\\Teru Apex V2 3BBB.xml"
Function ApplyPresetByFile(Actor a_actor, string a_pathToFile) Global Native

; Must be a preset loaded into memory, i.e. already in the preset folder
Function ApplyPresetByName(Actor a_actor, String a_name) Global Native

Function RegisterForOBodyEvent(Quest a_quest) Global Native

Function RegisterForOBodyNakedEvent(Quest a_quest) Global Native

Function RegisterForOBodyRemovingClothesEvent(Quest a_quest) Global Native

Function MarkForReprocess(Actor a_actor) Global
	NiOverride.SetBodyMorph(a_actor, "obody_processed", "OBody", 0.0)
EndFunction

Function RemoveClothesOverlay(Actor a_actor) Global
	RemoveClothesOverlay_(a_actor)
EndFunction

Function RemoveClothesOverlay_(Actor a_actor) Global Native

Function AddClothesOverlay(Actor a_actor) Global Native

String[] Function GetAllPossiblePresets(actor a_actor) Global Native

Int Function GetFemaleDatabaseSize() Global Native

Int Function GetMaleDatabaseSize() Global Native

Function SetORefit(Bool a_enabled) Global Native

Function SetNippleSlidersORefitEnabled(Bool a_enabled) Global Native

Function SetNippleRand(Bool a_enabled) Global Native

Function SetGenitalRand(Bool a_enabled) Global Native

Function SetPerformanceMode(Bool a_enabled) Global Native

Function SetRespectfulMorphApplication(Bool a_enabled) Global Native

Function SetLegacyStorageUtilUsageEnabled(Bool a_enabled) Global Native

Function SetDistributionKey(String a_distributionKey) Global Native

Function ResetActorOBodyMorphs(Actor a_actor) Global Native

Function ReapplyActorOBodyMorphs(Actor a_actor) Global Native

; OBody began keeping track of the preset assigned to an actor via native code
; starting after version 4.3.7.
String Function GetPresetAssignedToActor(Actor a_actor) Global Native

; Unlike `ApplyPresetByName` this applies a preset only if a preset with the name
; is found instead of falling back to a random preset.
; Additionally, this can be used to remove the preset assignment from an actor
; by supplying an empty string.
; `a_doNotApplyMorphs` takes precedence over `a_forceImmediateApplicationOfMorphs`.
; This returns whether the preset assignment succeeded or not.
Bool Function AssignPresetToActor(Actor a_actor, String a_presetName, Bool a_forceImmediateApplicationOfMorphs = True, Bool a_doNotApplyMorphs = False) Global Native
