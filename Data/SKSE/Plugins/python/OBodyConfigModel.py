import argparse
import json
from pathlib import Path

from pydantic import BaseModel, ConfigDict, Field, ValidationError
from typing import Dict, List, Annotated, Literal, Union

type NonEmptyTrimmedString = Annotated[str, Field(pattern=r"^(?=\S)(?=.*\S$).*")]
type NonEmptyString = Annotated[str, Field(pattern=r"^(?!\s*$).+")]

type BSTFile = NonEmptyTrimmedString

type FormID = Annotated[str, Field(pattern=r'^[0-9A-Fa-f]{3,8}$')]

type EditorID = NonEmptyTrimmedString

type RaceName = NonEmptyTrimmedString

type PresetName = NonEmptyString

type NPCName = NonEmptyTrimmedString

type OutfitName = NonEmptyTrimmedString

type ClassName = NonEmptyTrimmedString

type KeywordName = NonEmptyTrimmedString

type npcFormID = Annotated[Dict[BSTFile, Dict[FormID, List[PresetName]]], Field(default={},
                                                                                description="Here you can set which presets should be applied to specific NPCs by their FormID. The FormID is their unique identifier. Works with modded NPCs!")]
type npc = Annotated[Dict[NPCName, List[PresetName]], Field(default={},
                                                            description="Same as npcFormID, but you use the NPC names instead of the FormID.")]
type factionFemale = Annotated[Dict[EditorID, List[PresetName]], Field(default={},
                                                                       description="Here you can set which presets to distribute by faction for female NPCs.")]
type factionMale = Annotated[
    Dict[EditorID, List[PresetName]], Field(default={}, description="Same as factionFemale, but for male NPCs.")]
type npcPluginFemale = Annotated[Dict[BSTFile, List[PresetName]], Field(default={},
                                                                        description="Here you can set which presets should be applied to female NPCs from a specific plugin/mod.")]
type npcPluginMale = Annotated[
    Dict[BSTFile, List[PresetName]], Field(default={}, description="Same as npcPluginFemale but for male NPCs.")]
type raceFemale = Annotated[Dict[RaceName, List[PresetName]], Field(default={},
                                                                    description="Here you can define which presets should be applied to females of certain races. Works with custom races too! ONLY put female body presets here!")]
type raceMale = Annotated[Dict[RaceName, List[PresetName]], Field(default={},
                                                                  description="Same as above, but for males. ONLY put male body presets here (if you don't have any, leave it empty)!")]
type blacklistedNpcsFormID = Annotated[Dict[BSTFile, List[FormID]], Field(default={},
                                                                          description="Set which NPCs by their FormID should be ignored by OBody. Works with modded NPCs. Useful if you want modded NPCs to have a custom body you want to handle separately.")]
type blacklistedOutfitsFromORefitFormID = Annotated[Dict[BSTFile, List[FormID]], Field(default={},
                                                                                       description="Here you can write outfit FormIDs if you don't want ORefit to be applied to them. Further details and explanation is available further below.")]
type outfitsForceRefitFormID = Annotated[Dict[BSTFile, List[FormID]], Field(default={},
                                                                            description="Here you can write outfit FormIDs if you want to force ORefit to be applied to them, in case ORefit can't detect them. Further details and explanation is available further below. You will not need to write anything in this key 99% of the time.")]
type blacklistedNpcs = Annotated[List[NPCName], Field(default=[],
                                                      description="Same as blacklistedNpcsFormID, but you use NPC names instead of the FormID.")]
type blacklistedNpcsPluginFemale = Annotated[List[BSTFile], Field(default=[],
                                                                  description="Here you can blacklist all female NPCs from an entire plugin/mod by simply writing the plugin name.")]
type blacklistedNpcsPluginMale = Annotated[
    List[BSTFile], Field(default=[], description="Same as blacklistedNpcsPluginFemale, but for males.")]
type blacklistedOutfitsFromORefitPlugin = Annotated[
    List[BSTFile], Field(default=[], description="Same as blacklistedOutfitsFromORefitFormID, but you use filenames")]
type blacklistedRacesFemale = Annotated[List[RaceName], Field(default=["ElderRace"],
                                                              description="Here you can blacklist females of entire races instead of individual NPCs.")]
type blacklistedRacesMale = Annotated[
    List[RaceName], Field(default=["ElderRace"], description="Same as blacklistedRacesFemale, but for male NPCs.")]
type blacklistedPresetsFromRandomDistribution = Annotated[List[PresetName], Field(
    default=["- Zeroed Sliders -", "-Zeroed Sliders-", "Zeroed Sliders", "HIMBO Zero for OBody"],
    description="Should be self explanatory. Set the presets you do NOT want OBody to distribute randomly.")]
type blacklistedOutfitsFromORefit = Annotated[List[OutfitName], Field(default=["LS Force Naked", "OBody Nude 32"],
                                                                      description="Same as blacklistedOutfitsFromORefitFormID, but you use outfit names instead of their FormID.")]
type outfitsForceRefit = Annotated[List[OutfitName], Field(default=[],
                                                           description="Same as outfitsForceRefitFormID, but you use outfit names instead of their FormID.")]
type blacklistedPresetsShowInOBodyMenu = Annotated[
    bool, Field(default=True, description="Whether you want the blacklisted presets to show in the O menu or not.")]

type RefitPresetNameOrList = Union[PresetName, List[PresetName]]  # a single string is tolerated by the schema, but ignored by the parser

type refitOutfitPresetsFemale = Annotated[Dict[OutfitName, RefitPresetNameOrList], Field(default={}, description="Outfit name -> array of \"-Refit\" presets. For each outfit, the refit whose base preset (the text before \"-Refit\") matches the actor's current preset is used. Example: \"Blades Armor\": [\"Warrior Preset-Refit Heavy Armors\", \"Citizen Preset-Refit Heavy Armors\"].")]

type refitOutfitPresetsMale = Annotated[Dict[OutfitName, RefitPresetNameOrList], Field(default={}, description="Same as refitOutfitPresetsFemale, but for male NPCs.")]

type refitOutfitPresetsFemaleFormID = Annotated[Dict[BSTFile, Dict[FormID, RefitPresetNameOrList]], Field(default={}, description="Same as refitOutfitPresetsFemale, but you use the outfit's FormID instead of its name. This is unambiguous when several outfits share the same display name. Takes priority over refitOutfitPresetsFemale.")]

type refitOutfitPresetsMaleFormID = Annotated[Dict[BSTFile, Dict[FormID, RefitPresetNameOrList]], Field(default={}, description="Same as refitOutfitPresetsMale, but you use the outfit's FormID instead of its name. This is unambiguous when several outfits share the same display name. Takes priority over refitOutfitPresetsMale.")]

type refitOutfitKeywordsFemale = Annotated[List[KeywordName], Field(default=[], json_schema_extra={"uniqueItems": True}, description="Keyword EditorIDs (e.g. Topless, or a custom keyword distributed with SPID) for which to look up refit presets automatically on any worn outfit that has the keyword. The order matters: when an outfit has several of these keywords, the first one written here wins. For each keyword K, the preset \"<Actor's Preset>-Refit K\" is a differential refit for that actor preset, and \"Female-Refit K\" is an additive refit that works for any preset. ArmorHeavy, ArmorLight and ArmorClothing are always checked after the keywords written here, so they do not need to be listed (write them here only to give them a higher priority than your own keywords). Outfit-specific keys (refitOutfitPresets*) have priority over this one.")]

type refitOutfitKeywordsMale = Annotated[List[KeywordName], Field(default=[], json_schema_extra={"uniqueItems": True}, description="Same as refitOutfitKeywordsFemale, but for males (\"Male-Refit K\" is the additive refit).")]

type classFemale = Annotated[Dict[ClassName, List[PresetName]], Field(default={},
                                                                       description="Here you can define which presets should be applied to females of certain classes. Works with custom classes too! ONLY put female body presets here!")]
type classMale = Annotated[
    Dict[ClassName, List[PresetName]], Field(default={}, description="Same as above, but for males. ONLY put male body presets here (if you don't have any, leave it empty)!")]
type blacklistedClassesFemale = Annotated[List[ClassName], Field(default=[],
                                                                  description="Here you can blacklist females of entire classes instead of individual NPCs.")]
type blacklistedClassesMale = Annotated[
    List[ClassName], Field(default=[], description="Same as blacklistedClassesFemale, but for male NPCs.")]

type disablePresetDistribution = Annotated[Literal["none", "male", "female", "all"], Field(default="none",
                                                                                          description="Globally disable preset distribution for a gender, or all genders. Valid values: \"none\", \"male\", \"female\", \"all\".")]


class OBodyConfigModel(BaseModel):
    model_config = ConfigDict(extra='forbid', strict=True, regex_engine='python-re', populate_by_name=True)

    npcFormID: npcFormID
    npc: npc
    factionFemale: factionFemale
    factionMale: factionMale
    npcPluginFemale: npcPluginFemale
    npcPluginMale: npcPluginMale
    raceFemale: raceFemale
    raceMale: raceMale
    classFemale: classFemale
    classMale: classMale
    blacklistedNpcs: blacklistedNpcs
    blacklistedNpcsFormID: blacklistedNpcsFormID
    blacklistedNpcsPluginFemale: blacklistedNpcsPluginFemale
    blacklistedNpcsPluginMale: blacklistedNpcsPluginMale
    blacklistedClassesFemale: blacklistedClassesFemale
    blacklistedClassesMale: blacklistedClassesMale
    blacklistedRacesFemale: blacklistedRacesFemale
    blacklistedRacesMale: blacklistedRacesMale
    blacklistedOutfitsFromORefitFormID: blacklistedOutfitsFromORefitFormID
    blacklistedOutfitsFromORefit: blacklistedOutfitsFromORefit
    blacklistedOutfitsFromORefitPlugin: blacklistedOutfitsFromORefitPlugin
    outfitsForceRefitFormID: outfitsForceRefitFormID
    outfitsForceRefit: outfitsForceRefit
    blacklistedPresetsFromRandomDistribution: blacklistedPresetsFromRandomDistribution
    blacklistedPresetsShowInOBodyMenu: blacklistedPresetsShowInOBodyMenu
    refitOutfitPresetsFemale: refitOutfitPresetsFemale
    refitOutfitPresetsMale: refitOutfitPresetsMale
    refitOutfitPresetsFemaleFormID: refitOutfitPresetsFemaleFormID
    refitOutfitPresetsMaleFormID: refitOutfitPresetsMaleFormID
    refitOutfitKeywordsFemale: refitOutfitKeywordsFemale
    refitOutfitKeywordsMale: refitOutfitKeywordsMale
    disablePresetDistribution: disablePresetDistribution


def main(using_rapidjson: bool):
    # src: https://www.nexusmods.com/skyrimspecialedition/articles/4756
    test_examples: list[str] = [
        """{"npcFormID":{"Skyrim.esm":{"00013BA3":["Bardmaid"],"00013BA2":["Wench Preset","IA - Demonic","Tasty Temptress - BHUNP Preset (Nude)"]},"Immersive Wenches.esp":{"0403197F":["Petite Mommy"],"0400C3C0":["s4rMs' - Gaia"]}},"npc":{"Mjoll the Lioness":["Hardass Warrior"],"Haelga":["Petite Mommy","IA - Demonic","s4rMs' - Gaia"],"Temba Wide-Arm":["Tasty Temptress - BHUNP Preset (Nude)"]},"factionFemale":{"SolitudeBardsCollegeFaction":["Hardass Warrior","Fantasy Figure - Nude","Royal Battle Maiden - BHUNP"],"TownSolitudeFaction":["QC-The Everywoman"],"CollegeofWinterholdFaction":["Tasty Temptress - BHUNP Preset (Nude)"]},"factionMale":{"CompanionsCircle":["HIMBO Muscled"],"TownWhiterunFaction":["HIMBO Simple"]},"npcPluginFemale":{"Bijin_AIO_Merged.esp":["Hardass warrior","SilverR1baka"],"Skyrim.esm":["Nordic Oppai - BHUNP - Nude"]},"npcPluginMale":{"Dawnguard.esm":["HIMBO Simple"]},"raceFemale":{"NordRace":["QC-The Everywoman","D*sney Mommy NG","Fantasy Figure - Nude"],"OrcRace":["Hardass warrior"],"WoodElfRace":["-Zeroed Sliders-"]},"raceMale":{"NordRace":["HIMBO Simple"],"BretonRace":["HIMBO Simple"]},"blacklistedNpcs":["Saffir","Vilja","Lydia"],"blacklistedNpcsFormID":{"Skyrim.esm":["00013BB8","00013BBD"],"CS_Vayne.esp":["0400083D","0402CC59"]},"blacklistedNpcsPluginFemale":["CS_Coralyn.esp","3DNPC.esp","Hearthfires.esm"],"blacklistedNpcsPluginMale":["Immersive Wenches.esp","018Auri.esp"],"blacklistedRacesFemale":["ElderRace","ArgonianRace"],"blacklistedRacesMale":["ElderRace","DarkElfRace"],"blacklistedOutfitsFromORefitFormID":{"[full_inu] Queen Marika's Dress.esp":["FE000817"]},"blacklistedOutfitsFromORefit":["Demon Hunter's Clothes Light","White Sexy Top Ouvert","Wrap Around Dress (Slutty) - 14"],"blacklistedOutfitsFromORefitPlugin":["[COCO] Mysterious Mage.esp"],"outfitsForceRefitFormID":{"[full_inu] Queen Marika's Dress.esp":["FE000803"]},"outfitsForceRefit":["Demon Hunter's Lingerie Light","Demon Hunter's Lingerie Heavy"],"blacklistedPresetsFromRandomDistribution":["- Zeroed Sliders -","-Zeroed Sliders-","Zeroed Sliders","s4mRs'' - Juno","Royal Battlemaiden - BHUNP"],"blacklistedPresetsShowInOBodyMenu":true}""",
        """{"npcFormID":{},"npc":{},"factionFemale":{},"factionMale":{},"npcPluginFemale":{},"npcPluginMale":{},"raceFemale":{},"raceMale":{},"blacklistedNpcs":[],"blacklistedNpcsFormID":{},"blacklistedNpcsPluginFemale":[],"blacklistedNpcsPluginMale":[],"blacklistedRacesFemale":["ElderRace"],"blacklistedRacesMale":["ElderRace"],"blacklistedOutfitsFromORefitFormID":{},"blacklistedOutfitsFromORefit":["LS Force Naked","OBody Nude 32"],"blacklistedOutfitsFromORefitPlugin":[],"outfitsForceRefitFormID":{},"outfitsForceRefit":[],"blacklistedPresetsFromRandomDistribution":["- Zeroed Sliders -","-Zeroed Sliders-","Zeroed Sliders","HIMBO Zero for OBody"],"blacklistedPresetsShowInOBodyMenu":true,"refitOutfitPresetsFemale":{},"refitOutfitPresetsMale":{},"refitOutfitPresetsFemaleFormID":{},"refitOutfitPresetsMaleFormID":{},"refitOutfitKeywordsFemale":["Topless","ArmorLight"],"refitOutfitKeywordsMale":[],"disablePresetDistribution":"none"}""",
    ]
    base_dir = Path(__file__).parent.parent.resolve()
    try:
        for i in test_examples:
            OBodyConfigModel.model_validate_json(json_data=i)
    except ValidationError as e:
        print(e)
    except Exception as e:
        print(type(e).__name__, e)

    if (schema := base_dir / "OBody_presetDistributionConfig_schema.json").exists():
        with open(schema, 'w') as f:
            # noinspection PyRedundantParentheses
            if using_rapidjson:  # rapidjson uses draft 4, so a workaround
                temp = OBodyConfigModel.model_json_schema(ref_template="#/definitions/{model}")
                temp["definitions"] = temp.pop("$defs")
                # noinspection PyTypeChecker
                json.dump(obj=temp, indent=2, fp=f)
            else:
                # noinspection PyTypeChecker
                json.dump(obj=OBodyConfigModel.model_json_schema(), indent=2,
                          fp=f)  # this creates a draft 2020-12 schema
    del schema
    if (json_file := base_dir / "OBody_presetDistributionConfig.json").exists():
        with open(json_file, 'w') as f:
            # noinspection PyArgumentList
            f.write(OBodyConfigModel().model_dump_json(indent=2))
    del json_file, base_dir


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Toggle between using rapidjson(draft 4) or draft 2020-12.")
    parser.add_argument('--using-rapidjson', action='store_true', default=True,
                        help="Using rapidjson for JSON operations (default is True).")

    args = parser.parse_args()

    main(args.using_rapidjson)
