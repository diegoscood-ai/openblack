/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The tree that a computer player's mind weighs. The game reads it from no file: it is built from fixed rows, each a
/// need, a desire or an action with its weights, and every mind gets its own copy of the tree made from them. A row can
/// sit under several parents, so the tree has more nodes than rows.
namespace openblack::computer_player
{

/// How a row scores its node, one value for each way the game has: each is named after the first row of the tree, root
/// first and each row before its children, that scores that way. Rows of other names that share it score the same way.
enum class ScoreKind : uint8_t
{
	/// The node has no score of its own: it is worth the best of its children
	None,
	TalktoPlayer,
	GiveToPlayer,
	ThrowThingAtPlayer,
	CreatureInteract,
	GiveCreatureFood,
	GetFoodFromField,
	GetFoodFromFishFarm,
	CastFoodSpellOn,
	MakeCreatureDrink,
	RestoreCreatureHealth,
	RestoreMyCreatureHealth,
	MakeCreatureSleep,
	SendCreatureHome,
	MoveCreaturetoWarmerArea,
	MoveCreaturetoCoollerArea,
	TeachCreatureNormalAction,
	TeachCreatureAggressiveMagic,
	TeachCreatureAggressiveMagic164,
	TeachCreatureCompassionateMagic,
	TeachCreatureCompassionateMagic166,
	TeachCreatureImpressiveMagic,
	TeachCreatureImpressiveMagic168,
	ThrowFoodatCreature,
	SlapCreature,
	StrokeCreature,
	AttachLeashtoMovingObject,
	FindFreeVillagerInArea,
	CastSpellOnMyCreatureForALaugh,
	CastSpellOnMyCreatureForALaugh170,
	ReacttoCreatureStarvingEmergency,
	GetFoodFromOtherTownForObject,
	ReacttoCreatureIllEmergency,
	TownWithHighestFoodDesire,
	CreateDiscipleToFulfillTownDesire,
	GetCreaturetoHelp,
	TownWithHighestWoodDesire,
	ShallCastWoodSpell,
	GetWoodFromForest,
	GetWoodFromOtherTown,
	GiveBuildingSiteWood,
	GiveBuildingSiteWorkers,
	FindFreeVillagerBuilderInTown,
	CombineScaffolds,
	FindScaffoldToCombineWith,
	FindPlaceToUseScaffoldForBuilding,
	FindLocationToUseScaffold,
	FillWorkshop,
	GiveWorshippersFood,
	GiveWorshippersRest,
	TakeVillagerFromWorshipSiteToTown,
	WorshipSiteThatNeedsVillagers,
	RaiseTotemInTown,
	TakeVillagerFromTownToWorshipSite,
	ChargeUpSpell,
	MakeDiscipleBreader,
	DefeatPlayer,
	ExpandInfluence,
	UseCreatureToImpressTown,
	ThrowThingsToImpress,
	TakeOverTownByFulfillingFoodDesire,
	DefeatPlayer37,
	TakeOverTown,
	CastSpellToFulfilTownNeeds,
	DestroyTown,
	DestroytownWithCreature,
	PlayerWithMoreSpells,
	PlayerWithBetterCreature,
	AttackCreature,
	AttackCreatureWithCreature,
	ThrowrockatCreature,
	ReactToAggressiveCreature,
	ReactToPlayerCastingSpells,
	UseCreatureAsDiverseionAgainstPlayer,
	ReactToPlayerThrowingStuff,
	CastPhysicalShieldToProtect,
	DefendTown,
	DefendtownWithSpells,
	PlayerWithMoreWorshippers,
	CheckTownForDamage,
	FindBurningAbode,
	CastWaterSpell,
	FindDamagedAbode,
	CheckTownsForBeliefAttack,
	CastSpellToDefendAgainstBelief,
	CheckTownForProtection,
	FindUnprotectedAbode,
};

/// The action that a row's node does when it is chosen, named as the score ways are
enum class ActionKind : uint8_t
{
	/// The node has no action: running it runs its parent's
	None,
	DirectPickupAndDropObject,
	ThrowThingAtPlayer,
	GiveCreatureFood,
	CastFoodSpellOn,
	CastFoodForMyCreature,
	MakeCreatureDrink,
	RestoreMyCreatureHealth,
	MakeCreatureSleep,
	SendCreatureHome,
	MoveCreaturetoWarmerArea,
	MoveCreaturetoCoollerArea,
	TeachCreatureNormalAction,
	TeachCreatureAggressiveMagic,
	TeachCreatureCompassionateMagic,
	TeachCreatureImpressiveMagic,
	ThrowFoodatCreature,
	SlapCreature,
	StrokeCreature,
	AttachLeashtoMovingObject,
	CastSpellOnMyCreatureForALaugh,
	ReacttoCreatureStarvingEmergency,
	GetFoodFromOtherTownForObject,
	ReacttoCreatureIllEmergency,
	TownWithHighestFoodDesire,
	CreateDiscipleToFulfillTownDesire,
	GetCreaturetoHelp,
	ShallCastFoodSpell,
	GetFoodFromOtherTownForStoragePit,
	GetWoodFromOtherTown,
	GiveBuildingSiteWood,
	ShallCastWoodSpell,
	GiveBuildingSiteWorkers,
	GetCreaturetoHelpBuild,
	CombineScaffolds,
	FindPlaceToUseScaffoldForBuilding,
	FillWorkshop,
	GiveWorshippersFood,
	CastFoodSpellOnWorshipSite,
	CastHealSpellAtWorshipSite,
	TakeVillagerFromWorshipSiteToTown,
	RaiseTotemInTown,
	TakeVillagerFromTownToWorshipSite,
	ChargeUpSpell,
	UseCreatureToImpressTown,
	ThrowThingsToImpress,
	CastSpellToImpress,
	DestroytownWithSpell,
	DestroytownWithCreature,
	DestroytownWithObject,
	AttackCreatureWithSpells,
	AttackCreatureWithCreature,
	ThrowrockatCreature,
	UseCreatureAsDiverseionAgainstPlayer,
	CastPhysicalShieldToProtect,
	DefendTownWithCreature,
	DefendtownWithSpells,
	FindBurningAbode,
	CastSpellToDefendAgainstBelief,
};

/// A row of the tree
struct MindRow
{
	/// The name as the game has it, its number and the space after it included
	std::string_view name;
	ScoreKind score;
	ActionKind action;
	/// What the node's weight starts at
	float personality;
	/// What the node's suppression becomes when its action was done
	float suppressionAfterSuccess;
	/// How fast a suppression comes back, by the turn: 1 for not at all
	float recoveryRate;
	/// For the six aims under the root, they are weighed while the hand is busy every this many turns (0: every turn)
	uint32_t period;
	/// The rows under this one, in the order they are weighed. A row may be named twice
	uint8_t childCount;
	std::array<uint8_t, 8> children;
};

/// The rows, each where the tree first meets it: the root first, then each row before its children. Each is its name and
/// score, then its action, personality, suppression after success, recovery, period and children; the number after it
/// is its place
// clang-format off
inline constexpr std::array<MindRow, 126> k_MindRows {
    MindRow {"Master Decision Tree", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.0f, 0, 6, {1, 12, 49, 84, 114, 125}}, // 0
    MindRow {"PlayerInteract: 13 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 5000, 2, {2, 8}}, // 1
    MindRow {"TalktoPlayer: 24 ", ScoreKind::TalktoPlayer,
             ActionKind::None, 0.25f, 1.0f, 1.001f, 0, 4, {3, 4, 5, 7}}, // 2
    MindRow {"SendNiceTextToPlayer(Notdone): 52 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 3
    MindRow {"SendNastyTextToPlayer(Notdone): 53 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 4
    MindRow {"TauntPlayer(Notdone): 54 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {6}}, // 5
    MindRow {"DIRECTPickupAndDropObject: 182 ", ScoreKind::None,
             ActionKind::DirectPickupAndDropObject, 0.0f, 0.0f, 1.0f, 1, 0, {}}, // 6
    MindRow {"ScarePlayer(Notdone): 55 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 7
    MindRow {"GiveToPlayer: 23 ", ScoreKind::GiveToPlayer,
             ActionKind::None, 0.25f, 1.0f, 1.001f, 0, 3, {9, 10, 11}}, // 8
    MindRow {"GiveCreatureThingToPlayer(Notdone): 56 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 9
    MindRow {"GiveToPlayer(Notdone): 57 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 10
    MindRow {"ThrowThingAtPlayer: 50 ", ScoreKind::ThrowThingAtPlayer,
             ActionKind::ThrowThingAtPlayer, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 11
    MindRow {"CreatureInteract: 14 ", ScoreKind::CreatureInteract,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 100, 4, {13, 27, 35, 43}}, // 12
    MindRow {"Lookaftercreature: 27 ", ScoreKind::None,
             ActionKind::None, 0.4f, 1.0f, 1.001f, 0, 8, {14, 18, 20, 21, 23, 24, 25, 26}}, // 13
    MindRow {"GiveCreatureFood: 59 ", ScoreKind::GiveCreatureFood,
             ActionKind::GiveCreatureFood, 1.0f, 1.0f, 1.001f, 0, 3, {15, 16, 17}}, // 14
    MindRow {"GetFoodFromField: 127 ", ScoreKind::GetFoodFromField,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 15
    MindRow {"GetFoodFromFishFarm: 128 ", ScoreKind::GetFoodFromFishFarm,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 16
    MindRow {"CastFoodSpellOn: 137 ", ScoreKind::CastFoodSpellOn,
             ActionKind::CastFoodSpellOn, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 17
    MindRow {"CastFoodForMyCreature: 60 ", ScoreKind::GiveCreatureFood,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {19}}, // 18
    MindRow {"CastFoodForMyCreature: 160 ", ScoreKind::CastFoodSpellOn,
             ActionKind::CastFoodForMyCreature, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 19
    MindRow {"MakeCreatureDrink: 61 ", ScoreKind::MakeCreatureDrink,
             ActionKind::MakeCreatureDrink, 0.0f, 1.0f, 1.001f, 0, 0, {}}, // 20
    MindRow {"RestoreCreatureHealth: 62 ", ScoreKind::RestoreCreatureHealth,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {22}}, // 21
    MindRow {"RestoreMyCreatureHealth: 162 ", ScoreKind::RestoreMyCreatureHealth,
             ActionKind::RestoreMyCreatureHealth, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 22
    MindRow {"MakeCreatureSleep: 63 ", ScoreKind::MakeCreatureSleep,
             ActionKind::MakeCreatureSleep, 0.0f, 1.0f, 1.001f, 0, 0, {}}, // 23
    MindRow {"SendCreatureHome: 64 ", ScoreKind::SendCreatureHome,
             ActionKind::SendCreatureHome, 0.5f, 1.0f, 1.001f, 0, 0, {}}, // 24
    MindRow {"MoveCreaturetoWarmerArea: 65 ", ScoreKind::MoveCreaturetoWarmerArea,
             ActionKind::MoveCreaturetoWarmerArea, 0.0f, 1.0f, 1.001f, 0, 0, {}}, // 25
    MindRow {"MoveCreaturetoCoollerArea: 66 ", ScoreKind::MoveCreaturetoCoollerArea,
             ActionKind::MoveCreaturetoCoollerArea, 0.0f, 1.0f, 1.001f, 0, 0, {}}, // 26
    MindRow {"TeachCreature: 28 ", ScoreKind::None,
             ActionKind::None, 0.5f, 1.0f, 1.001f, 0, 4, {28, 29, 31, 33}}, // 27
    MindRow {"TeachCreatureNormalAction: 68 ", ScoreKind::TeachCreatureNormalAction,
             ActionKind::TeachCreatureNormalAction, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 28
    MindRow {"TeachCreatureAggressiveMagic: 69 ", ScoreKind::TeachCreatureAggressiveMagic,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {30}}, // 29
    MindRow {"TeachCreatureAggressiveMagic: 164 ", ScoreKind::TeachCreatureAggressiveMagic164,
             ActionKind::TeachCreatureAggressiveMagic, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 30
    MindRow {"TeachCreatureCompassionateMagic: 70 ", ScoreKind::TeachCreatureCompassionateMagic,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {32}}, // 31
    MindRow {"TeachCreatureCompassionateMagic: 166 ", ScoreKind::TeachCreatureCompassionateMagic166,
             ActionKind::TeachCreatureCompassionateMagic, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 32
    MindRow {"TeachCreatureImpressiveMagic: 71 ", ScoreKind::TeachCreatureImpressiveMagic,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {34}}, // 33
    MindRow {"TeachCreatureImpressiveMagic: 168 ", ScoreKind::TeachCreatureImpressiveMagic168,
             ActionKind::TeachCreatureImpressiveMagic, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 34
    MindRow {"PlayWithCreature: 29 ", ScoreKind::None,
             ActionKind::None, 0.25f, 1.0f, 1.001f, 0, 5, {36, 37, 38, 39, 41}}, // 35
    MindRow {"ThrowFoodatCreature: 73 ", ScoreKind::ThrowFoodatCreature,
             ActionKind::ThrowFoodatCreature, 1.0f, 1.0f, 1.001f, 0, 1, {15}}, // 36
    MindRow {"SlapCreature: 74 ", ScoreKind::SlapCreature,
             ActionKind::SlapCreature, 0.0f, 1.0f, 1.001f, 0, 0, {}}, // 37
    MindRow {"StrokeCreature: 75 ", ScoreKind::StrokeCreature,
             ActionKind::StrokeCreature, 0.0f, 1.0f, 1.001f, 0, 0, {}}, // 38
    MindRow {"AttachLeashtoMovingObject: 76 ", ScoreKind::AttachLeashtoMovingObject,
             ActionKind::AttachLeashtoMovingObject, 0.0f, 1.0f, 1.001f, 0, 1, {40}}, // 39
    MindRow {"FindFreeVillagerInArea: 140 ", ScoreKind::FindFreeVillagerInArea,
             ActionKind::None, 0.9f, 0.5f, 1.001f, 0, 0, {}}, // 40
    MindRow {"CastSpellOnMyCreatureForALaugh: 77 ", ScoreKind::CastSpellOnMyCreatureForALaugh,
             ActionKind::None, 0.0f, 1.0f, 1.001f, 0, 1, {42}}, // 41
    MindRow {"CastSpellOnMyCreatureForALaugh: 170 ", ScoreKind::CastSpellOnMyCreatureForALaugh170,
             ActionKind::CastSpellOnMyCreatureForALaugh, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 42
    MindRow {"ReactToCreatureEmergency: 30 ", ScoreKind::None,
             ActionKind::None, 0.95f, 1.0f, 1.001f, 0, 3, {44, 46, 48}}, // 43
    MindRow {"ReacttoCreatureStarvingEmergency: 79 ", ScoreKind::ReacttoCreatureStarvingEmergency,
             ActionKind::ReacttoCreatureStarvingEmergency, 1.0f, 1.0f, 1.001f, 0, 4, {17, 15, 16, 45}}, // 44
    MindRow {"GetFoodFromOtherTownForObject: 130 ", ScoreKind::GetFoodFromOtherTownForObject,
             ActionKind::GetFoodFromOtherTownForObject, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 45
    MindRow {"ReacttoCreatureDamagedEmergency: 80 ", ScoreKind::RestoreCreatureHealth,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {47}}, // 46
    MindRow {"RestoreMyCreatureHealth: 172 ", ScoreKind::RestoreMyCreatureHealth,
             ActionKind::RestoreMyCreatureHealth, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 47
    MindRow {"ReacttoCreatureIllEmergency: 81 ", ScoreKind::ReacttoCreatureIllEmergency,
             ActionKind::ReacttoCreatureIllEmergency, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 48
    MindRow {"TownInteract: 15 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 300, 4, {50, 65, 72, 82}}, // 49
    MindRow {"AttendTownDesires: 32 ", ScoreKind::None,
             ActionKind::None, 0.7f, 1.0f, 1.001f, 0, 5, {51, 56, 60, 62, 60}}, // 50
    MindRow {"TownWithHighestFoodDesire: 83 ", ScoreKind::TownWithHighestFoodDesire,
             ActionKind::TownWithHighestFoodDesire, 1.0f, 1.0f, 1.001f, 0, 6, {52, 53, 54, 15, 16, 55}}, // 51
    MindRow {"CreateDiscipleToFulfillTownDesire: 187 ", ScoreKind::CreateDiscipleToFulfillTownDesire,
             ActionKind::CreateDiscipleToFulfillTownDesire, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 52
    MindRow {"GetCreaturetoHelp: 124 ", ScoreKind::GetCreaturetoHelp,
             ActionKind::GetCreaturetoHelp, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 53
    MindRow {"ShallCastFoodSpell: 126 ", ScoreKind::CastFoodSpellOn,
             ActionKind::ShallCastFoodSpell, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 54
    MindRow {"GetFoodFromOtherTownForStoragePit: 129 ", ScoreKind::GetFoodFromOtherTownForObject,
             ActionKind::GetFoodFromOtherTownForStoragePit, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 55
    MindRow {"TownWithHighestWoodDesire: 84 ", ScoreKind::TownWithHighestWoodDesire,
             ActionKind::TownWithHighestFoodDesire, 1.0f, 1.0f, 1.001f, 0, 5, {52, 53, 57, 58, 59}}, // 56
    MindRow {"ShallCastWoodSpell: 132 ", ScoreKind::ShallCastWoodSpell,
             ActionKind::ShallCastFoodSpell, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 57
    MindRow {"GetWoodFromForest: 133 ", ScoreKind::GetWoodFromForest,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 58
    MindRow {"GetWoodFromOtherTown: 134 ", ScoreKind::GetWoodFromOtherTown,
             ActionKind::GetWoodFromOtherTown, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 59
    MindRow {"GiveBuildingSiteWood: 86 ", ScoreKind::GiveBuildingSiteWood,
             ActionKind::GiveBuildingSiteWood, 1.0f, 1.0f, 1.001f, 0, 3, {61, 58, 59}}, // 60
    MindRow {"ShallCastWoodSpell: 136 ", ScoreKind::ShallCastWoodSpell,
             ActionKind::ShallCastWoodSpell, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 61
    MindRow {"GiveBuildingSiteWorkers: 85 ", ScoreKind::GiveBuildingSiteWorkers,
             ActionKind::GiveBuildingSiteWorkers, 1.0f, 1.0f, 1.001f, 0, 3, {52, 63, 64}}, // 62
    MindRow {"GetCreaturetoHelpBuild: 125 ", ScoreKind::GetCreaturetoHelp,
             ActionKind::GetCreaturetoHelpBuild, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 63
    MindRow {"FindFreeVillagerBuilderInTown: 139 ", ScoreKind::FindFreeVillagerBuilderInTown,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 64
    MindRow {"ExpandTown: 33 ", ScoreKind::None,
             ActionKind::None, 0.7f, 1.0f, 1.001f, 0, 3, {66, 68, 70}}, // 65
    MindRow {"CombineScaffolds: 112 ", ScoreKind::CombineScaffolds,
             ActionKind::CombineScaffolds, 1.0f, 1.0f, 1.001f, 0, 1, {67}}, // 66
    MindRow {"FindScaffoldToCombineWith: 180 ", ScoreKind::FindScaffoldToCombineWith,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 67
    MindRow {"FindPlaceToUseScaffoldForBuilding: 113 ", ScoreKind::FindPlaceToUseScaffoldForBuilding,
             ActionKind::FindPlaceToUseScaffoldForBuilding, 1.0f, 1.0f, 1.001f, 0, 1, {69}}, // 68
    MindRow {"FindLocationToUseScaffold: 181 ", ScoreKind::FindLocationToUseScaffold,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 69
    MindRow {"FillWorkshop: 114 ", ScoreKind::FillWorkshop,
             ActionKind::FillWorkshop, 1.0f, 1.0f, 1.001f, 0, 3, {71, 58, 59}}, // 70
    MindRow {"CastWoodSpellOn: 138 ", ScoreKind::ShallCastWoodSpell,
             ActionKind::CastFoodSpellOn, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 71
    MindRow {"AttendToWorshippersWorshippers: 34 ", ScoreKind::None,
             ActionKind::None, 0.7f, 1.0f, 1.001f, 0, 4, {73, 75, 78, 81}}, // 72
    MindRow {"GiveWorshippersFood: 89 ", ScoreKind::GiveWorshippersFood,
             ActionKind::GiveWorshippersFood, 1.0f, 1.0f, 1.001f, 0, 4, {74, 15, 16, 45}}, // 73
    MindRow {"CastFoodSpellOnWorshipSite: 135 ", ScoreKind::CastFoodSpellOn,
             ActionKind::CastFoodSpellOnWorshipSite, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 74
    MindRow {"GiveWorshippersRest: 90 ", ScoreKind::GiveWorshippersRest,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 2, {76, 77}}, // 75
    MindRow {"CastHealSpellAtWorshipSite: 188 ", ScoreKind::RestoreMyCreatureHealth,
             ActionKind::CastHealSpellAtWorshipSite, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 76
    MindRow {"TakeVillagerFromWorshipSiteToTown: 185 ", ScoreKind::TakeVillagerFromWorshipSiteToTown,
             ActionKind::TakeVillagerFromWorshipSiteToTown, 1.0f, 1.0f, 1.0f, 0, 0, {}}, // 77
    MindRow {"WorshipSiteThatNeedsVillagers: 91 ", ScoreKind::WorshipSiteThatNeedsVillagers,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 2, {79, 80}}, // 78
    MindRow {"RaiseTotemInTown: 183 ", ScoreKind::RaiseTotemInTown,
             ActionKind::RaiseTotemInTown, 1.0f, 1.0f, 1.0f, 0, 0, {}}, // 79
    MindRow {"TakeVillagerFromTownToWorshipSite: 184 ", ScoreKind::TakeVillagerFromTownToWorshipSite,
             ActionKind::TakeVillagerFromTownToWorshipSite, 1.0f, 1.0f, 1.0f, 0, 0, {}}, // 80
    MindRow {"ChargeUpSpell: 96 ", ScoreKind::ChargeUpSpell,
             ActionKind::ChargeUpSpell, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 81
    MindRow {"IncreaseTownPopulation: 35 ", ScoreKind::None,
             ActionKind::None, 0.7f, 1.0f, 1.001f, 0, 1, {83}}, // 82
    MindRow {"MakeDiscipleBreader: 119 ", ScoreKind::MakeDiscipleBreader,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {52}}, // 83
    MindRow {"DefeatPlayer: 16 ", ScoreKind::DefeatPlayer,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 200, 6, {85, 91, 98, 99, 104, 113}}, // 84
    MindRow {"ExpandInfluence: 25 ", ScoreKind::ExpandInfluence,
             ActionKind::None, 0.75f, 1.0f, 1.001f, 0, 3, {86, 90, 90}}, // 85
    MindRow {"TakeOverTownByImpressing: 115 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 3, {87, 88, 89}}, // 86
    MindRow {"UseCreatureToImpressTown: 142 ", ScoreKind::UseCreatureToImpressTown,
             ActionKind::UseCreatureToImpressTown, 1.0f, 1.0f, 1.001f, 3300, 0, {}}, // 87
    MindRow {"ThrowThingsToImpress: 143 ", ScoreKind::ThrowThingsToImpress,
             ActionKind::ThrowThingsToImpress, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 88
    MindRow {"CastSpellToImpress: 144 ", ScoreKind::TeachCreatureImpressiveMagic168,
             ActionKind::CastSpellToImpress, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 89
    MindRow {"TakeOverTownByFulfillingFoodDesire: 116 ", ScoreKind::TakeOverTownByFulfillingFoodDesire,
             ActionKind::TownWithHighestFoodDesire, 1.0f, 1.0f, 1.001f, 0, 4, {54, 15, 16, 55}}, // 90
    MindRow {"DefeatPlayer: 37 ", ScoreKind::DefeatPlayer37,
             ActionKind::None, 0.1f, 1.0f, 1.001f, 0, 3, {92, 94, 81}}, // 91
    MindRow {"TakeOverTown: 93 ", ScoreKind::TakeOverTown,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 4, {87, 88, 89, 93}}, // 92
    MindRow {"CastSpellToFulfilTownNeeds: 145 ", ScoreKind::CastSpellToFulfilTownNeeds,
             ActionKind::ShallCastFoodSpell, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 93
    MindRow {"DestroyTown: 94 ", ScoreKind::DestroyTown,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 3, {95, 96, 97}}, // 94
    MindRow {"DestroytownWithSpell: 147 ", ScoreKind::CastSpellOnMyCreatureForALaugh170,
             ActionKind::DestroytownWithSpell, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 95
    MindRow {"DestroytownWithCreature: 148 ", ScoreKind::DestroytownWithCreature,
             ActionKind::DestroytownWithCreature, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 96
    MindRow {"DestroytownWithObject: 149 ", ScoreKind::None,
             ActionKind::DestroytownWithObject, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 97
    MindRow {"PlayerWithMoreSpells: 38 ", ScoreKind::PlayerWithMoreSpells,
             ActionKind::None, 0.1f, 1.0f, 1.001f, 0, 3, {92, 94, 81}}, // 98
    MindRow {"PlayerWithBetterCreature: 39 ", ScoreKind::PlayerWithBetterCreature,
             ActionKind::None, 0.1f, 1.0f, 1.001f, 0, 1, {100}}, // 99
    MindRow {"AttackCreature: 100 ", ScoreKind::AttackCreature,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 3, {101, 102, 103}}, // 100
    MindRow {"AttackCreatureWithSpells: 151 ", ScoreKind::TeachCreatureAggressiveMagic164,
             ActionKind::AttackCreatureWithSpells, 1.0f, 0.9f, 1.001f, 0, 0, {}}, // 101
    MindRow {"AttackCreatureWithCreature: 152 ", ScoreKind::AttackCreatureWithCreature,
             ActionKind::AttackCreatureWithCreature, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 102
    MindRow {"ThrowrockatCreature: 153 ", ScoreKind::ThrowrockatCreature,
             ActionKind::ThrowrockatCreature, 0.95f, 1.0f, 1.001f, 0, 0, {}}, // 103
    MindRow {"ReactToPlayer: 42 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 4, {105, 106, 108, 110}}, // 104
    MindRow {"ReactToAggressiveCreature: 102 ", ScoreKind::ReactToAggressiveCreature,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 3, {101, 102, 103}}, // 105
    MindRow {"ReactToPlayerCastingSpells: 103 ", ScoreKind::ReactToPlayerCastingSpells,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {107}}, // 106
    MindRow {"UseCreatureAsDiverseionAgainstPlayer: 174 ", ScoreKind::UseCreatureAsDiverseionAgainstPlayer,
             ActionKind::UseCreatureAsDiverseionAgainstPlayer, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 107
    MindRow {"ReactToPlayerThrowingStuff: 104 ", ScoreKind::ReactToPlayerThrowingStuff,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 2, {109, 107}}, // 108
    MindRow {"CastPhysicalShieldToProtect: 177 ", ScoreKind::CastPhysicalShieldToProtect,
             ActionKind::CastPhysicalShieldToProtect, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 109
    MindRow {"DefendTown: 105 ", ScoreKind::DefendTown,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 2, {111, 112}}, // 110
    MindRow {"DefendTownWithCreature: 155 ", ScoreKind::UseCreatureAsDiverseionAgainstPlayer,
             ActionKind::DefendTownWithCreature, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 111
    MindRow {"DefendtownWithSpells: 156 ", ScoreKind::DefendtownWithSpells,
             ActionKind::DefendtownWithSpells, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 112
    MindRow {"PlayerWithMoreWorshippers: 40 ", ScoreKind::PlayerWithMoreWorshippers,
             ActionKind::None, 0.1f, 1.0f, 1.001f, 0, 2, {92, 94}}, // 113
    MindRow {"ReactToPlayer: 17 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 50, 4, {104, 115, 119, 122}}, // 114
    MindRow {"CheckTownForDamage: 44 ", ScoreKind::CheckTownForDamage,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 2, {116, 118}}, // 115
    MindRow {"FindBurningAbode: 109 ", ScoreKind::FindBurningAbode,
             ActionKind::FindBurningAbode, 1.0f, 1.0f, 1.001f, 0, 1, {117}}, // 116
    MindRow {"CastWaterSpell: 179 ", ScoreKind::CastWaterSpell,
             ActionKind::CastFoodSpellOn, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 117
    MindRow {"FindDamagedAbode: 110 ", ScoreKind::FindDamagedAbode,
             ActionKind::FindBurningAbode, 1.0f, 1.0f, 1.001f, 0, 1, {53}}, // 118
    MindRow {"CheckTownsForBeliefAttack: 43 ", ScoreKind::CheckTownsForBeliefAttack,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {120}}, // 119
    MindRow {"DefendTownAgainstBeliefAttack: 118 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 2, {121, 87}}, // 120
    MindRow {"CastSpellToDefendAgainstBelief: 186 ", ScoreKind::CastSpellToDefendAgainstBelief,
             ActionKind::CastSpellToDefendAgainstBelief, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 121
    MindRow {"CheckTownForProtection: 45 ", ScoreKind::CheckTownForProtection,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {123}}, // 122
    MindRow {"FindUnprotectedAbode: 111 ", ScoreKind::FindUnprotectedAbode,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 0, 1, {124}}, // 123
    MindRow {"CastShieldToProtect: 175 ", ScoreKind::DefendtownWithSpells,
             ActionKind::DefendtownWithSpells, 1.0f, 1.0f, 1.001f, 0, 0, {}}, // 124
    MindRow {"WorldInteract: 18 ", ScoreKind::None,
             ActionKind::None, 1.0f, 1.0f, 1.001f, 5000, 0, {}}, // 125
};
// clang-format on

/// A node of the tree as every mind has it: its row, its parent and its children, by their places in the tree
struct MindNode
{
	uint8_t row {0};
	/// None for the root
	std::optional<uint16_t> parent;
	uint8_t childCount {0};
	std::array<uint16_t, 8> children {};
};

/// The number of nodes of the tree, and the root's place
inline constexpr size_t k_MindNodeCount = 198;
inline constexpr uint16_t k_RootNode = 0;

namespace detail
{
/// The nodes, as the game builds them from the rows: the root first, each node before its children and the children in
/// their order. Each is its row, its parent and its children; the number after it is its place
// clang-format off
inline constexpr std::array<MindNode, k_MindNodeCount> k_BuiltNodes {
    MindNode {0, std::nullopt, 6, {1, 12, 53, 105, 171, 197}}, // 0
    MindNode {1, 0, 2, {2, 8}}, // 1
    MindNode {2, 1, 4, {3, 4, 5, 7}}, // 2
    MindNode {3, 2, 0, {}}, // 3
    MindNode {4, 2, 0, {}}, // 4
    MindNode {5, 2, 1, {6}}, // 5
    MindNode {6, 5, 0, {}}, // 6
    MindNode {7, 2, 0, {}}, // 7
    MindNode {8, 1, 3, {9, 10, 11}}, // 8
    MindNode {9, 8, 0, {}}, // 9
    MindNode {10, 8, 0, {}}, // 10
    MindNode {11, 8, 0, {}}, // 11
    MindNode {12, 0, 4, {13, 27, 35, 44}}, // 12
    MindNode {13, 12, 8, {14, 18, 20, 21, 23, 24, 25, 26}}, // 13
    MindNode {14, 13, 3, {15, 16, 17}}, // 14
    MindNode {15, 14, 0, {}}, // 15
    MindNode {16, 14, 0, {}}, // 16
    MindNode {17, 14, 0, {}}, // 17
    MindNode {18, 13, 1, {19}}, // 18
    MindNode {19, 18, 0, {}}, // 19
    MindNode {20, 13, 0, {}}, // 20
    MindNode {21, 13, 1, {22}}, // 21
    MindNode {22, 21, 0, {}}, // 22
    MindNode {23, 13, 0, {}}, // 23
    MindNode {24, 13, 0, {}}, // 24
    MindNode {25, 13, 0, {}}, // 25
    MindNode {26, 13, 0, {}}, // 26
    MindNode {27, 12, 4, {28, 29, 31, 33}}, // 27
    MindNode {28, 27, 0, {}}, // 28
    MindNode {29, 27, 1, {30}}, // 29
    MindNode {30, 29, 0, {}}, // 30
    MindNode {31, 27, 1, {32}}, // 31
    MindNode {32, 31, 0, {}}, // 32
    MindNode {33, 27, 1, {34}}, // 33
    MindNode {34, 33, 0, {}}, // 34
    MindNode {35, 12, 5, {36, 38, 39, 40, 42}}, // 35
    MindNode {36, 35, 1, {37}}, // 36
    MindNode {15, 36, 0, {}}, // 37
    MindNode {37, 35, 0, {}}, // 38
    MindNode {38, 35, 0, {}}, // 39
    MindNode {39, 35, 1, {41}}, // 40
    MindNode {40, 40, 0, {}}, // 41
    MindNode {41, 35, 1, {43}}, // 42
    MindNode {42, 42, 0, {}}, // 43
    MindNode {43, 12, 3, {45, 50, 52}}, // 44
    MindNode {44, 44, 4, {46, 47, 48, 49}}, // 45
    MindNode {17, 45, 0, {}}, // 46
    MindNode {15, 45, 0, {}}, // 47
    MindNode {16, 45, 0, {}}, // 48
    MindNode {45, 45, 0, {}}, // 49
    MindNode {46, 44, 1, {51}}, // 50
    MindNode {47, 50, 0, {}}, // 51
    MindNode {48, 44, 0, {}}, // 52
    MindNode {49, 0, 4, {54, 80, 89, 102}}, // 53
    MindNode {50, 53, 5, {55, 62, 68, 72, 76}}, // 54
    MindNode {51, 54, 6, {56, 57, 58, 59, 60, 61}}, // 55
    MindNode {52, 55, 0, {}}, // 56
    MindNode {53, 55, 0, {}}, // 57
    MindNode {54, 55, 0, {}}, // 58
    MindNode {15, 55, 0, {}}, // 59
    MindNode {16, 55, 0, {}}, // 60
    MindNode {55, 55, 0, {}}, // 61
    MindNode {56, 54, 5, {63, 64, 65, 66, 67}}, // 62
    MindNode {52, 62, 0, {}}, // 63
    MindNode {53, 62, 0, {}}, // 64
    MindNode {57, 62, 0, {}}, // 65
    MindNode {58, 62, 0, {}}, // 66
    MindNode {59, 62, 0, {}}, // 67
    MindNode {60, 54, 3, {69, 70, 71}}, // 68
    MindNode {61, 68, 0, {}}, // 69
    MindNode {58, 68, 0, {}}, // 70
    MindNode {59, 68, 0, {}}, // 71
    MindNode {62, 54, 3, {73, 74, 75}}, // 72
    MindNode {52, 72, 0, {}}, // 73
    MindNode {63, 72, 0, {}}, // 74
    MindNode {64, 72, 0, {}}, // 75
    MindNode {60, 54, 3, {77, 78, 79}}, // 76
    MindNode {61, 76, 0, {}}, // 77
    MindNode {58, 76, 0, {}}, // 78
    MindNode {59, 76, 0, {}}, // 79
    MindNode {65, 53, 3, {81, 83, 85}}, // 80
    MindNode {66, 80, 1, {82}}, // 81
    MindNode {67, 81, 0, {}}, // 82
    MindNode {68, 80, 1, {84}}, // 83
    MindNode {69, 83, 0, {}}, // 84
    MindNode {70, 80, 3, {86, 87, 88}}, // 85
    MindNode {71, 85, 0, {}}, // 86
    MindNode {58, 85, 0, {}}, // 87
    MindNode {59, 85, 0, {}}, // 88
    MindNode {72, 53, 4, {90, 95, 98, 101}}, // 89
    MindNode {73, 89, 4, {91, 92, 93, 94}}, // 90
    MindNode {74, 90, 0, {}}, // 91
    MindNode {15, 90, 0, {}}, // 92
    MindNode {16, 90, 0, {}}, // 93
    MindNode {45, 90, 0, {}}, // 94
    MindNode {75, 89, 2, {96, 97}}, // 95
    MindNode {76, 95, 0, {}}, // 96
    MindNode {77, 95, 0, {}}, // 97
    MindNode {78, 89, 2, {99, 100}}, // 98
    MindNode {79, 98, 0, {}}, // 99
    MindNode {80, 98, 0, {}}, // 100
    MindNode {81, 89, 0, {}}, // 101
    MindNode {82, 53, 1, {103}}, // 102
    MindNode {83, 102, 1, {104}}, // 103
    MindNode {52, 103, 0, {}}, // 104
    MindNode {84, 0, 6, {106, 121, 132, 143, 148, 161}}, // 105
    MindNode {85, 105, 3, {107, 111, 116}}, // 106
    MindNode {86, 106, 3, {108, 109, 110}}, // 107
    MindNode {87, 107, 0, {}}, // 108
    MindNode {88, 107, 0, {}}, // 109
    MindNode {89, 107, 0, {}}, // 110
    MindNode {90, 106, 4, {112, 113, 114, 115}}, // 111
    MindNode {54, 111, 0, {}}, // 112
    MindNode {15, 111, 0, {}}, // 113
    MindNode {16, 111, 0, {}}, // 114
    MindNode {55, 111, 0, {}}, // 115
    MindNode {90, 106, 4, {117, 118, 119, 120}}, // 116
    MindNode {54, 116, 0, {}}, // 117
    MindNode {15, 116, 0, {}}, // 118
    MindNode {16, 116, 0, {}}, // 119
    MindNode {55, 116, 0, {}}, // 120
    MindNode {91, 105, 3, {122, 127, 131}}, // 121
    MindNode {92, 121, 4, {123, 124, 125, 126}}, // 122
    MindNode {87, 122, 0, {}}, // 123
    MindNode {88, 122, 0, {}}, // 124
    MindNode {89, 122, 0, {}}, // 125
    MindNode {93, 122, 0, {}}, // 126
    MindNode {94, 121, 3, {128, 129, 130}}, // 127
    MindNode {95, 127, 0, {}}, // 128
    MindNode {96, 127, 0, {}}, // 129
    MindNode {97, 127, 0, {}}, // 130
    MindNode {81, 121, 0, {}}, // 131
    MindNode {98, 105, 3, {133, 138, 142}}, // 132
    MindNode {92, 132, 4, {134, 135, 136, 137}}, // 133
    MindNode {87, 133, 0, {}}, // 134
    MindNode {88, 133, 0, {}}, // 135
    MindNode {89, 133, 0, {}}, // 136
    MindNode {93, 133, 0, {}}, // 137
    MindNode {94, 132, 3, {139, 140, 141}}, // 138
    MindNode {95, 138, 0, {}}, // 139
    MindNode {96, 138, 0, {}}, // 140
    MindNode {97, 138, 0, {}}, // 141
    MindNode {81, 132, 0, {}}, // 142
    MindNode {99, 105, 1, {144}}, // 143
    MindNode {100, 143, 3, {145, 146, 147}}, // 144
    MindNode {101, 144, 0, {}}, // 145
    MindNode {102, 144, 0, {}}, // 146
    MindNode {103, 144, 0, {}}, // 147
    MindNode {104, 105, 4, {149, 153, 155, 158}}, // 148
    MindNode {105, 148, 3, {150, 151, 152}}, // 149
    MindNode {101, 149, 0, {}}, // 150
    MindNode {102, 149, 0, {}}, // 151
    MindNode {103, 149, 0, {}}, // 152
    MindNode {106, 148, 1, {154}}, // 153
    MindNode {107, 153, 0, {}}, // 154
    MindNode {108, 148, 2, {156, 157}}, // 155
    MindNode {109, 155, 0, {}}, // 156
    MindNode {107, 155, 0, {}}, // 157
    MindNode {110, 148, 2, {159, 160}}, // 158
    MindNode {111, 158, 0, {}}, // 159
    MindNode {112, 158, 0, {}}, // 160
    MindNode {113, 105, 2, {162, 167}}, // 161
    MindNode {92, 161, 4, {163, 164, 165, 166}}, // 162
    MindNode {87, 162, 0, {}}, // 163
    MindNode {88, 162, 0, {}}, // 164
    MindNode {89, 162, 0, {}}, // 165
    MindNode {93, 162, 0, {}}, // 166
    MindNode {94, 161, 3, {168, 169, 170}}, // 167
    MindNode {95, 167, 0, {}}, // 168
    MindNode {96, 167, 0, {}}, // 169
    MindNode {97, 167, 0, {}}, // 170
    MindNode {114, 0, 4, {172, 185, 190, 194}}, // 171
    MindNode {104, 171, 4, {173, 177, 179, 182}}, // 172
    MindNode {105, 172, 3, {174, 175, 176}}, // 173
    MindNode {101, 173, 0, {}}, // 174
    MindNode {102, 173, 0, {}}, // 175
    MindNode {103, 173, 0, {}}, // 176
    MindNode {106, 172, 1, {178}}, // 177
    MindNode {107, 177, 0, {}}, // 178
    MindNode {108, 172, 2, {180, 181}}, // 179
    MindNode {109, 179, 0, {}}, // 180
    MindNode {107, 179, 0, {}}, // 181
    MindNode {110, 172, 2, {183, 184}}, // 182
    MindNode {111, 182, 0, {}}, // 183
    MindNode {112, 182, 0, {}}, // 184
    MindNode {115, 171, 2, {186, 188}}, // 185
    MindNode {116, 185, 1, {187}}, // 186
    MindNode {117, 186, 0, {}}, // 187
    MindNode {118, 185, 1, {189}}, // 188
    MindNode {53, 188, 0, {}}, // 189
    MindNode {119, 171, 1, {191}}, // 190
    MindNode {120, 190, 2, {192, 193}}, // 191
    MindNode {121, 191, 0, {}}, // 192
    MindNode {87, 191, 0, {}}, // 193
    MindNode {122, 171, 1, {195}}, // 194
    MindNode {123, 194, 1, {196}}, // 195
    MindNode {124, 195, 0, {}}, // 196
    MindNode {125, 0, 0, {}}, // 197
};
// clang-format on
} // namespace detail

static_assert(detail::k_BuiltNodes.back().row == k_MindRows.size() - 1, "the last node is the last aim's");

/// Every node of the tree, the root first and each node before its children, as the game looks for a name
[[nodiscard]] inline const std::array<MindNode, k_MindNodeCount>& MindNodes()
{
	return detail::k_BuiltNodes;
}

/// The row of a node
[[nodiscard]] inline const MindRow& RowOf(uint16_t node)
{
	return k_MindRows.at(MindNodes().at(node).row);
}

/// The children of a node, in their order
[[nodiscard]] inline std::span<const uint16_t> ChildrenOf(uint16_t node)
{
	const auto& entry = MindNodes().at(node);
	return std::span<const uint16_t>(entry.children.data(), entry.childCount);
}

/// The six aims under the root
inline constexpr size_t k_TopNodeCount = 6;

/// A node's name up to its last ':', or the whole name when it has none
[[nodiscard]] constexpr std::string_view NamePrefix(std::string_view name)
{
	const auto colon = name.rfind(':');
	return colon == std::string_view::npos ? name : name.substr(0, colon);
}

/// The name a script gives begins with the node's name up to its ':'
[[nodiscard]] inline bool NameMatches(uint16_t node, std::string_view name)
{
	const auto prefix = NamePrefix(RowOf(node).name);
	return name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix;
}

/// The rows' names as the game reads them when it looks for a name. They start as the rows' own names; a personality
/// file that is read writes over them, for every mind and for the rest of the game
class MindRowNames
{
public:
	/// The rows' own names
	MindRowNames();

	/// The row's name now, up to its first nul
	[[nodiscard]] std::string_view Name(size_t row) const;
	/// The room the row's name is kept in: as many bytes as its own name and the nul after it
	[[nodiscard]] std::span<char> Room(size_t row);

private:
	std::array<std::string, k_MindRows.size()> _rooms;
};

/// The rows' own names, which nothing writes over
[[nodiscard]] const MindRowNames& OwnRowNames();

/// Every node that the name names, in the order of the tree: the order in which the game finds the first, the second
/// and so on. A node is known by its row's name as it is now
[[nodiscard]] std::vector<uint16_t> FindNodes(std::string_view name, const MindRowNames& names = OwnRowNames());

/// The first node that the name names, none when no node does
[[nodiscard]] std::optional<uint16_t> FindNode(std::string_view name, const MindRowNames& names = OwnRowNames());

/// The names of the nodes up to their ':', in the order of the tree, each only where it is first met: a name met again
/// later is always found first where it was met first
[[nodiscard]] std::span<const std::string_view> MindNodeNames();

} // namespace openblack::computer_player
