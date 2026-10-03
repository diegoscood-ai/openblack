/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalAnimations.h"

#include <algorithm>
#include <vector>

#include "Common/GameRandom.h"
#include "ECS/AnimalAI.h"
#include "ECS/Animations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs
{
using animal_ai::AnimalState;
using components::Animal;
using components::AnimalBrain;
using components::SkeletalAnimation;

namespace
{
/// The animation virtuals of the species (vtable slots of AnimalAnimation.cpp)
enum class Slot
{
	Move,         // +0xBE4
	Stand,        // +0x854 (and Decide +0xC20, which is Stand)
	Dying,        // +0xBE8
	Dead,         // +0xBEC
	Eat,          // +0xBF0
	StartToEat,   // +0xBF4
	FinishEating, // +0xBF8
	Sleep,        // +0xBFC (SLEEPS, HIDE_IN_LAIR)
	Pounce,       // +0xC04
	Standard,     // +0xC10
	InHand,       // +0xC14
	Landed,       // +0xC18
	Thrown,       // +0xC1C
};

/// g_AnimalStateTable (0xD12108) slot 0x60 of each state (dev\tmp_dis\anim\animal_table.txt)
Slot SlotOf(AnimalState state)
{
	switch (state)
	{
	case AnimalState::InScript:
	case AnimalState::InteractDecideWhatToDo:
	case AnimalState::GivesBirth:
	case AnimalState::DecideWhatToDo:
	case static_cast<AnimalState>(7):  // LOOKING_AT_OBJECT_REACTION
	case static_cast<AnimalState>(30): // FLEEING_AND_LOOKING_AT_OBJECT_REACTION
		return Slot::Stand;
	case AnimalState::Flying:
		return Slot::Thrown;
	case AnimalState::Landed:
		return Slot::Landed;
	case AnimalState::Dying:
	case AnimalState::Drowning:
	case AnimalState::Downed:
		return Slot::Dying;
	case AnimalState::Dead:
	case AnimalState::BeingEaten:
		return Slot::Dead;
	case AnimalState::InHand:
		return Slot::InHand;
	case AnimalState::Eat:
		return Slot::Eat;
	case AnimalState::Sleeps:
	case AnimalState::HideInLair:
		return Slot::Sleep;
	case AnimalState::StandardAction:
		return Slot::Standard;
	case AnimalState::StartToEat:
		return Slot::StartToEat;
	case AnimalState::FinishEating:
		return Slot::FinishEating;
	case AnimalState::TargetPounce:
		return Slot::Pounce;
	default:
		return Slot::Move;
	}
}

/// GSpeedThreshold::Infos (info.dat speedThreshold) entry, in the speed's units (SpeedState = the u16 speed)
uint32_t MaxWalk(size_t entry)
{
	return static_cast<uint32_t>(Locator::infoConstants::value().speedThreshold.at(entry).speedMaxWalk);
}
uint32_t MaxRun(size_t entry)
{
	return static_cast<uint32_t>(Locator::infoConstants::value().speedThreshold.at(entry).speedMaxRun);
}

/// The clips of one species (AnimalAnimation.cpp 0x41C0E0..0x41CF10; research dev\tmp_dis\animals\hand_death.md §7)
struct SpeciesClips
{
	int32_t stand, dying, eat0, eat1, startToEat, finishEating, sleep, pounce, inHand, thrown;
	int32_t landed[3]; ///< landType 0, 1, 2-3
	int32_t dead[2];   ///< landType 1, other
};

/// Cow::MoveAnimation 0x41C720 (Sheep and Pig use the cow's threshold, entry 2): run over its walk speed
int32_t GrazerMove(uint32_t speed, int32_t run, int32_t walk)
{
	return speed > MaxWalk(2) ? run : walk;
}

/// Lion::MoveAnimation 0x41C0E0 and the other predators: prowl under speedDefault, run over the entry's run speed
int32_t PredatorMove(uint32_t speed, const GAnimalInfo& info, size_t entry, int32_t prowl, int32_t run, int32_t walk)
{
	if (speed < static_cast<uint32_t>(info.speedGroup.speedDefault))
	{
		return prowl;
	}
	return speed > MaxRun(entry) ? run : walk;
}

AnimalInfo BaseSpecies(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::PuzzleCow:
		return AnimalInfo::Cow;
	case AnimalInfo::PuzzleSheep:
		return AnimalInfo::Sheep;
	case AnimalInfo::PuzzlePig:
		return AnimalInfo::Pig;
	case AnimalInfo::PuzzleHorse:
		return AnimalInfo::Horse;
	case AnimalInfo::PuzzleLion:
		return AnimalInfo::Lion;
	case AnimalInfo::PuzzleWolf:
	case AnimalInfo::SpellWolf:
		return AnimalInfo::Wolf;
	case AnimalInfo::PuzzleTortoise:
		return AnimalInfo::Tortoise;
	default:
		return type;
	}
}

const SpeciesClips* ClipsOf(AnimalInfo species)
{
	// stand dying eat(0) eat(1) startToEat finishEating sleep pounce inHand thrown {landed} {dead}
	static constexpr SpeciesClips k_Cow {42, 31, 36, 35, 37, 44, 42, -1, 38, 43, {42, 40, 39}, {30, 29}};
	static constexpr SpeciesClips k_Sheep {142, 134, 133, 132, 136, 140, 142, -1, 137, 143, {142, 139, 138}, {131, 130}};
	static constexpr SpeciesClips k_Pig {126, 118, 117, 116, 120, 124, 126, -1, 121, 127, {126, 123, 122}, {115, 114}};
	static constexpr SpeciesClips k_Horse {57, 46, 49, 48, 52, 60, 57, -1, 53, 58, {57, 55, 54}, {47, 46}};
	// the predators' landing is the get-up from sleep and their corpse the sleep loop, whatever the landType
	static constexpr SpeciesClips k_Lion {106, 89, 92, 92, 95, 110, 105, 100, 98, 106, {112, 112, 112}, {105, 105}};
	static constexpr SpeciesClips k_Tiger {164, 149, 150, 150, 153, 166, 163, 158, 164, 164, {168, 168, 168}, {163, 163}};
	static constexpr SpeciesClips k_Leopard {80, 65, 66, 66, 69, 82, 79, 74, 72, 80, {84, 84, 84}, {79, 79}};
	static constexpr SpeciesClips k_Wolf {184, 173, 174, 174, 175, 185, 183, 179, 184, 184, {187, 187, 187}, {183, 183}};
	static constexpr SpeciesClips k_Tortoise {171, 171, 171, 171, 171, 171, 171, -1, 171, 171, {171, 171, 171}, {171, 171}};
	switch (species)
	{
	case AnimalInfo::Cow:
		return &k_Cow;
	case AnimalInfo::Sheep:
		return &k_Sheep;
	case AnimalInfo::Pig:
		return &k_Pig;
	case AnimalInfo::Horse:
		return &k_Horse;
	case AnimalInfo::Lion:
		return &k_Lion;
	case AnimalInfo::Tiger:
		return &k_Tiger;
	case AnimalInfo::Leopard:
		return &k_Leopard;
	case AnimalInfo::Wolf:
		return &k_Wolf;
	case AnimalInfo::Tortoise:
		return &k_Tortoise;
	default:
		// Goat and Zebra return -1 everywhere (and are never made); the birds fly, not done yet
		return nullptr;
	}
}

int32_t MoveClip(AnimalInfo species, const GAnimalInfo& info, uint32_t speed)
{
	switch (species)
	{
	case AnimalInfo::Cow:
		return GrazerMove(speed, 41, 45);
	case AnimalInfo::Sheep:
		return GrazerMove(speed, 141, 145);
	case AnimalInfo::Pig:
		return GrazerMove(speed, 125, 128);
	case AnimalInfo::Horse:
		// Horse::MoveAnimation 0x41CA70, entry 3: walk, trot, run
		return speed <= MaxWalk(3) ? 61 : (speed > MaxRun(3) ? 56 : 59);
	case AnimalInfo::Lion:
		return PredatorMove(speed, info, 6, 102, 103, 113);
	case AnimalInfo::Tiger:
		return PredatorMove(speed, info, 7, 160, 161, 169);
	case AnimalInfo::Leopard:
		return PredatorMove(speed, info, 8, 76, 77, 85);
	case AnimalInfo::Wolf:
		return PredatorMove(speed, info, 9, 180, 181, 188);
	case AnimalInfo::Tortoise:
		return 172;
	default:
		return -1;
	}
}

bool IsBird(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Crow:
	case AnimalInfo::Dove:
	case AnimalInfo::Swallow:
	case AnimalInfo::Pigeon:
	case AnimalInfo::Seagull:
	case AnimalInfo::Bat:
	case AnimalInfo::SpellDove:
	case AnimalInfo::SpellBat:
		return true;
	default:
		return false;
	}
}

/// The birds' slots (AnimalAnimation.cpp 0x41BD20..0x41C080; dev\tmp_dis\animals\birds_draw.md §1): Move (and Decide)
/// is a coin between flap and glide (the swallow three ways, the bat always flap); Stand, Dead, Eat, Sleep, Thrown are
/// fixed ids (the odd ones, e.g. the crow's stand = TAKEOFF, are what the code returns); every other slot -1
int32_t BirdClip(AnimalInfo type, Slot slot, AnimalState state)
{
	if (state == AnimalState::DecideWhatToDo)
	{
		slot = Slot::Move; // Animal::DecideAnimation 0x41BD20 = the Move function
	}
	// GameRand(2) == 0 ? first : second (Dove 0x41BD39, Crow 0x41BEC9, Pigeon / Seagull 0x41BF39 / 0x41BFA9)
	const auto coin = [](int32_t heads, int32_t tails) { return game_random::GameRand(2) == 0 ? heads : tails; };
	struct Fixed
	{
		int32_t stand, dead, eat, sleep, thrown;
	};
	Fixed fixed {};
	switch (type)
	{
	case AnimalInfo::Crow:
		if (slot == Slot::Move)
		{
			return coin(3, 4);
		}
		fixed = {7, 4, 5, 6, 7};
		break;
	case AnimalInfo::Dove:
		if (slot == Slot::Move)
		{
			return coin(8, 9);
		}
		fixed = {8, 8, 8, 8, 8};
		break;
	case AnimalInfo::Pigeon:
		if (slot == Slot::Move)
		{
			return coin(13, 14);
		}
		fixed = {17, 14, 15, 16, 17};
		break;
	case AnimalInfo::Seagull:
		if (slot == Slot::Move)
		{
			return coin(18, 20);
		}
		fixed = {22, 19, 20, 21, 22};
		break;
	case AnimalInfo::Swallow:
		if (slot == Slot::Move)
		{
			const auto third = game_random::GameRand(3); // 0x41C019
			return third == 0 ? 27 : (third == 1 ? 26 : 25);
		}
		fixed = {27, 26, 27, 27, 27};
		break;
	case AnimalInfo::SpellDove:
		return 23;
	case AnimalInfo::Bat:
	case AnimalInfo::SpellBat:
	default:
		if (slot == Slot::Move)
		{
			return 1;
		}
		fixed = {2, 2, 2, 2, 2};
		break;
	}
	switch (slot)
	{
	case Slot::Stand:
		return fixed.stand;
	case Slot::Dead:
		return fixed.dead;
	case Slot::Eat:
		return fixed.eat;
	case Slot::Sleep:
		return fixed.sleep;
	case Slot::Thrown:
		return fixed.thrown;
	default:
		return -1;
	}
}

SkeletalAnimation& AnimationOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animation = registry.TryGet<SkeletalAnimation>(entity); animation != nullptr)
	{
		return *animation;
	}
	return registry.Assign<SkeletalAnimation>(entity);
}
} // namespace

int32_t AnimalAnimId(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* animal = registry.TryGet<const Animal>(entity);
	if (animal == nullptr || !Locator::infoConstants::has_value())
	{
		return -1;
	}
	const auto* brain = registry.TryGet<const AnimalBrain>(entity);
	const auto state = brain != nullptr ? static_cast<AnimalState>(brain->topState) : AnimalState::DecideWhatToDo;
	if (IsBird(animal->type))
	{
		return BirdClip(animal->type, SlotOf(state), state);
	}
	const auto species = BaseSpecies(animal->type);
	const auto* clips = ClipsOf(species);
	if (clips == nullptr)
	{
		return -1;
	}
	const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type));
	const uint32_t speed = brain != nullptr ? brain->speed : static_cast<uint32_t>(info.speedGroup.speedDefault);
	const uint16_t landType = brain != nullptr ? (brain->status >> 4) & 3 : 3;
	switch (SlotOf(state))
	{
	case Slot::Move:
		return MoveClip(species, info, speed);
	case Slot::Stand:
		return clips->stand;
	case Slot::Dying:
		return clips->dying;
	case Slot::Dead:
		// Cow::DeadAnimation 0x41C790: (status & 0x30) == 0x10 ? DEAD_ON_RHS : DEAD_ON_LHS
		return landType == 1 ? clips->dead[0] : clips->dead[1];
	case Slot::Eat:
		// Cow::EatAnimation 0x41C740: GameRand(2) picks one of the two (0x41C74C)
		if (clips->eat0 == clips->eat1)
		{
			return clips->eat0;
		}
		return game_random::GameRand(2) == 0 ? clips->eat0 : clips->eat1;
	case Slot::StartToEat:
		return clips->startToEat;
	case Slot::FinishEating:
		return clips->finishEating;
	case Slot::Sleep:
		return clips->sleep;
	case Slot::Pounce:
		return clips->pounce;
	case Slot::Standard:
		return -1; // Animal::StandardAnimation 0x4177D0
	case Slot::InHand:
		return clips->inHand;
	case Slot::Landed:
		// Cow::LandedAnimation 0x41C760: lt 0 ? STAND : lt 1 ? LANDED_RIGHT_SIDE : LANDED_LEFT_SIDE
		return clips->landed[std::min<uint16_t>(landType, 2)];
	case Slot::Thrown:
		return clips->thrown;
	}
	return -1;
}

void SetAnimalAnim(entt::entity entity, int32_t clip, bool reset)
{
	// Living::SetAnim (0x5ECBA0): switches at once (no blending), nothing if that clip already plays; -1 keeps the clip
	if (clip < 0)
	{
		return;
	}
	auto& animation = AnimationOf(entity);
	if (animation.locked || (animation.hasClip && animation.clipIndex == clip))
	{
		return;
	}
	animation.clip = ClipId(static_cast<uint32_t>(clip));
	animation.clipIndex = clip;
	animation.hasClip = true;
	if (reset)
	{
		animation.time = 0.0f;
	}
}

void SetAnimalStateAnim(entt::entity entity)
{
	// Living::SetStateAnim (0x5ECB10): the state's clip from the start; the birds' SetAnim (fn_0041E7E0) never restarts it
	const auto* animal = Locator::entitiesRegistry::value().TryGet<const Animal>(entity);
	SetAnimalAnim(entity, AnimalAnimId(entity), animal == nullptr || !IsBird(animal->type));
}

void UpdateAnimalAnimations()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fresh;
	registry.Each<const Animal>([&registry, &fresh](entt::entity entity, const Animal&) {
		if (!registry.AllOf<SkeletalAnimation>(entity))
		{
			fresh.push_back(entity);
		}
	});
	for (const auto entity : fresh)
	{
		// Living::CallVirtualFunctionsForCreation (0x5EC9B0): SetAnim(reset) with the first state's clip
		registry.Assign<SkeletalAnimation>(entity);
		SetAnimalStateAnim(entity);
	}
	// Animal::Draw 0x51C310 -> fn_0051AF00: while it moved in the last turn (Object::IsMoving) the clip advances with the
	// ground covered, otherwise with time
	registry.Each<const AnimalBrain, SkeletalAnimation>([](entt::entity, const AnimalBrain& brain, SkeletalAnimation& animation) {
		animation.distanceSpeed = brain.movedLastTurn > 0.0f ? brain.movedLastTurn * 10.0f : 0.0f;
	});
}

} // namespace openblack::ecs
