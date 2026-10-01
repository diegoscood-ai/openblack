/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <string_view>
#include <type_traits>

#include "PSys.h"

// Class name -> factory for the spell files' modifier (rule, emitter) and creator classes. The original resolves the
// names with its persistence registry (PSysFileData::OnLoaded 0x672620); here every topic file registers its own
// classes from one explicit list, RegisterAll() in PSysRegistry.cpp (no static initialisers). A class nobody registered
// is "not ported yet": it is attached with no effect, so the group logic stays the same.

namespace openblack::psys
{
using ModifierFactory = std::unique_ptr<Modifier> (*)(const Object& object);
using CreatorFactory = std::unique_ptr<Creator> (*)(const Object& object);

/// A later registration of the same name replaces the earlier one (and logs it)
void RegisterModifier(std::string_view className, ModifierFactory factory);
/// For Particle*Creator classes that need more than the common properties; the others use the plain Creator
void RegisterCreator(std::string_view className, CreatorFactory factory);

/// An EventCondition class (TEventCondition: ConditionTrueForAtom / ConditionTrueForCollection): the raw answer, before
/// the object's InvertResponse. `atom` is null when it is asked for a collection.
using ConditionTest = bool (*)(const Effect& effect, const Object& object, const Atom* atom, const Collection& collection);
void RegisterCondition(std::string_view className, ConditionTest test);
[[nodiscard]] ConditionTest FindCondition(std::string_view className);

/// nullptr when the class is not registered
[[nodiscard]] ModifierFactory FindModifierFactory(std::string_view className);
[[nodiscard]] CreatorFactory FindCreatorFactory(std::string_view className);

/// The common ParticleCreator properties (fn_006A85E0) and the sprite ones (ParticleSpriteCreator 0x6AA0D0). A
/// registered creator calls it first on its own Creator subclass.
void ReadCreatorProperties(const Object& object, Creator& creator);

/// The base name of a TextureFileName property (".\Data\Textures\S_Lightning.raw" -> "S_lightning") with the case the
/// file really has in Data\Textures: "raw/<that>" is the id Game.cpp loaded it under, and the spell files do not always
/// spell it the same way (S_Lightning.raw for the file S_lightning.raw).
[[nodiscard]] std::string TextureBaseName(std::string path);

/// The usual factory: T(object), or T() for the classes without properties
template <class T>
std::unique_ptr<Modifier> MakeModifierOf(const Object& object)
{
	if constexpr (std::is_constructible_v<T, const Object&>)
	{
		return std::make_unique<T>(object);
	}
	else
	{
		return std::make_unique<T>();
	}
}

// The registration lists, one per topic file (called once, in this order, by PSysRegistry.cpp)
void RegisterCoreModifiers(); ///< PSys.cpp: the create rules, emitters and rules ported before the miracles
void RegisterSoundRules();    ///< Rules/Sound.cpp: StartStopSoundOnCondition, AddSoundToAtom, RemoveSoundFromAtom
void RegisterFireballRules(); ///< Rules/Fireball.cpp: the throw, bounce, MagicFireBall, deflection, spin, trail (M5)
void RegisterSprinkleRules(); ///< Rules/Sprinkle.cpp: UR_HandSprinkle, AppearanceRuleTumble
void RegisterMeshCreators();  ///< Creators/Mesh.cpp: ParticleMeshCreator, ParticleMeshCreatorAnimTextured
void RegisterHandFollowRules(); ///< Rules/HandFollow.cpp: UR_FollowLocalHand, UR_FollowCastPosn
void RegisterGestureRules();    ///< Rules/Gesture.cpp: UR_GesturingRecognised, ZR_ChainGesture, CreateRuleMakeChain
void RegisterLightningRules();  ///< Rules/Lightning.cpp: UR_Lightning, UR_LightningStrike (M5)
void RegisterChainCreator();    ///< Creators/Chain.cpp: ParticleChainCreator (M5)
void RegisterLightMapCreator(); ///< Creators/LightMap.cpp: ParticleLightMapCreator (M5)
void RegisterHealRules();       ///< Rules/Heal.cpp: UR_HealSpellChakra, the fused explode, UR_HealInHand (M4)
void RegisterShieldRules();     ///< Rules/Shield.cpp: the defensive spheres, shield sparks, spin, vapour, EP atoms (M6)
void RegisterSurfRevolRules();  ///< Rules/SurfRevol.cpp: ZR_SurfRevol, the teleport pool and dispenser discs (M6)
void RegisterExplosionRules(); ///< Rules/Explosion.cpp: UR_Explosion, SetPSysCloseDown, UR_MoveAtom, UR_ChangeScaleXYZ (M6b)
void RegisterKeyPointRules();  ///< Rules/KeyPoints.cpp: UR_KPStretchHeight, UR_KPMoveAtoms (heal PU mushroom, M6b)
void RegisterOrientRules();    ///< Rules/Orient.cpp: UR_OrientSpriteWithVelocity (fireball in hand, M6b)
void RegisterForestRules();    ///< Rules/Forest.cpp: UR_ForestPath, ParticleGoodEvilCreator (forest butterflies, M6b)
void RegisterMistCreator();     ///< Creators/Mist.cpp: ParticleMistCreator (M4a water; the storm uses it too)
void RegisterFlockRules();      ///< Rules/Flock.cpp: UR_FollowTargets, EventConditionAtomNearVillagers (M4c)
void RegisterStormRules();      ///< Rules/Storm.cpp: UR_CloudMoverNew, UR_CloudGather, UR_Tornado, UR_StormCast (M6)
} // namespace openblack::psys
