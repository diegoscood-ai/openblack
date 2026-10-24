/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spiritual shield against the other miracles, pinned to the original: a lightning fork ends where it enters a
// shield that holds, a blast centred inside one is cancelled, a blocked attack drains the shield by the attacker's own
// cost until the shield runs dry and lets it through, and an area effect that only meets a shield costs its own caster.
// Then the dome's sounds: its looping hum, sized by the dome, cut at once when the dome goes, and a spark sound for each
// hit. Wiki: docs/wiki/magic/shields.md#the-particles.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/Banks.h"
#include "Audio/Services/SpellSounds.h"
#include "ECS/Components/Spell.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "FileSystem/DefaultFileSystem.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/Spells/SpellShield.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SoundAction.h"
#include "Particles/SpellLink.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using EventType = particles::SpellEventInfo::Type;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellCreator;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
/// The dome's blocking sphere, as SF_DefenseSphere makes it: UR_AddDefensiveSphere on the root, its radius the
/// magnitude x 1.11062
constexpr std::string_view k_Dome = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider0
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1000
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.11062
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_Root
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_AddDefensiveSphere UR_AddDefensiveSphere0
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY IsMagical BOOL 1
PROPERTY SphereRadius PERSIS_PNTR MagnitudeFloatProvider0
ENDPROPERTIES
ENDCLASS
)";

/// A bolt cast from the hand with one target (a ground point within 12 m of the hand, 2 m up: no map here) and five
/// joints per fork; group 0 holds the rule, the forks are group 1
constexpr std::string_view k_Bolt = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS UR_Lightning UR_Lightning0
BEGINPROPERTIES
PROPERTY CastingFromHand BOOL 1
PROPERTY CommonGlowGroup INTEGER 2
PROPERTY ForkGroup INTEGER 1
PROPERTY ForkScale FLOAT 2
PROPERTY Group INTEGER 0
PROPERTY LightMapGroup INTEGER -1
PROPERTY MaxJointsPerFork INTEGER 5
PROPERTY MaxLightningObjects INTEGER 1
PROPERTY MaxLightningObjectsAtOnce INTEGER 1
PROPERTY MinLightningObjects INTEGER 1
PROPERTY DefaultSearchRadius FLOAT 20
PROPERTY RandomFrac FLOAT 0.05
PROPERTY SplitAngle FLOAT 0.2
PROPERTY RenewSearchEvery FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticleChainCreator0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticleChainCreator ParticleChainCreator0
BEGINPROPERTIES
PROPERTY NumTexturesForWholeChain INTEGER 4
PROPERTY TextureFileName STRING .\Data\Textures\S_Lightning.raw
PROPERTY UseAdditiveAlpha BOOL 1
ENDPROPERTIES
ENDCLASS
)";

/// The control / parent / blast structure of SF_BeamExplosionSingle (group 8 -> 7 -> 6), with its UR_Explosion values
constexpr std::string_view k_Blast = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT 25
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_Control
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 8
PROPERTY NextGroups ARRAY SIZE 1 7
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS SetPSysCloseDown SetPSysCloseDown_Control
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR EventConditionCollectionDelay_CloseDown
PROPERTY Group INTEGER 8
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionCollectionDelay EventConditionCollectionDelay_CloseDown
BEGINPROPERTIES
PROPERTY DelayTime FLOAT 6
PROPERTY InvertResponse BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_Explosion UR_Explosion0
BEGINPROPERTIES
PROPERTY BlastSpeed FLOAT 50
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 6
PROPERTY InitialDelay FLOAT 0.4
PROPERTY MaxDistance FLOAT 15
PROPERTY MaxObjectsToDelete INTEGER 15
PROPERTY MaxObjectsToExplode INTEGER 15
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR NULL_STRING
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SmokeDelay FLOAT 1.2
PROPERTY SpreadSpeed FLOAT 20
PROPERTY TimeToDoEventsFor FLOAT 5
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_ExplosionParent
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 7
PROPERTY NextGroups ARRAY SIZE 1 6
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
)";

/// The sounds of SF_DefenseSphere: its root's looping hum, sized by the dome's radius (the magnitude x 1.11062) and not
/// released softly, and the spark rule's hit sound, released softly, one per spark (up to four of them, three seconds
/// each). Each atom has its own creator so the test can tell them apart
constexpr std::string_view k_DomeSounds = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider0
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1000
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.11062
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Hum
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_SparkRoot
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Spark
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_RootSurface
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Hum
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION SOUND_SPELL_SHIELD_ON LOOPING 1 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
PROPERTY SoundRadiusFP PERSIS_PNTR MagnitudeFloatProvider0
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_RootSpark
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 3
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_SparkRoot
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UpdateRuleShieldSpark UpdateRuleShieldSpark1
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 3
PROPERTY MaxNumAtomsForCollection INTEGER 4
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Spark
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundSpark SOUND_ACTION SOUND_SPELL_SHIELD_HIT LOOPING 0 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0
PROPERTY SparkLife FLOAT 3
PROPERTY SphereRadius PERSIS_PNTR MagnitudeFloatProvider0
ENDPROPERTIES
ENDCLASS
)";

/// The dome's spark rule as its file has it, but with neither the spark's life nor its wiggle given, so that the rule's
/// own defaults apply; each spark's arc of ten atoms is made in group 4 under it, as in the dome's file
constexpr std::string_view k_DomeSparks = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider0
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1000
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.11062
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_SparkRoot
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Spark
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Arc
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_RootSpark
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 3
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_SparkRoot
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UpdateRuleShieldSpark UpdateRuleShieldSpark1
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 3
PROPERTY MaxNumAtomsForCollection INTEGER 4
PROPERTY NextGroups ARRAY SIZE 1 4
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Spark
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundSpark SOUND_ACTION SOUND_SPELL_SHIELD_HIT LOOPING 0 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0
PROPERTY SphereRadius PERSIS_PNTR MagnitudeFloatProvider0
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleSphere CreateRuleSphere_Sparks
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 4
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY NumAtoms INTEGER 10
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Arc
PROPERTY Radius FLOAT 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SetInitFrameFromIndex BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
)";

/// The look of SF_DefenseSphere: its root grows from 0.1 to 1 over 2 s while the dome is open, spins with the cast's
/// curl (x 1, at most 5 rad/s, fading over 5 s), shrinks over 2 s once it closes and goes 2.2 s after; the surface
/// patch under it fades in at 30 a second, in a collection whose alpha is the strength x 255 between 40 and 255. The
/// patch hangs from the root here, not from the 15 tracers of the sphere's surface
constexpr std::string_view k_DomeLook = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider0
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1000
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.11062
ENDPROPERTIES
ENDCLASS
BEGINCLASS StrengthFloatProvider StrengthFloatProvider_ForAlpha
BEGINPROPERTIES
PROPERTY ScaleBy FLOAT 255
PROPERTY Maximum FLOAT 255
PROPERTY Minimum FLOAT 40
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Root
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_Patch
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_RootSurface
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 2
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Root
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_InitialSpin UR_InitialSpin0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY MaxAngularVelocity FLOAT 5
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY ScaleAngularVelocity FLOAT 1
PROPERTY TimeToFade FLOAT 5
ENDPROPERTIES
ENDCLASS
BEGINCLASS SetCollectionAlpha SetCollectionAlpha0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 2
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY Alpha PERSIS_PNTR StrengthFloatProvider_ForAlpha
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_VapourEndEffect UR_VapourEndEffectGiles
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 2
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY ScaleFactor PERSIS_PNTR MagnitudeFloatProvider0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_ChangeScale UR_ChangeScale0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR EventConditionTrueOnCloseDown_Invert
PROPERTY Group INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY StartScale FLOAT 0.1
PROPERTY StartTime FLOAT 0
PROPERTY StopScale FLOAT 1.0
PROPERTY StopTime FLOAT 2.0
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionTrueOnCloseDown EventConditionTrueOnCloseDown0
BEGINPROPERTIES
PROPERTY InvertResponse BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionTrueOnCloseDown EventConditionTrueOnCloseDown_Invert
BEGINPROPERTIES
PROPERTY InvertResponse BOOL 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS AR_FadeOutOnceConditionTrue AR_FadeOutOnceConditionTrue0
BEGINPROPERTIES
PROPERTY ConditionStartFadeOut PERSIS_PNTR EventConditionTrueOnCloseDown0
PROPERTY Group INTEGER 0
PROPERTY TimeToFadeOut FLOAT 2
PROPERTY FadeAlpha BOOL 0
PROPERTY ShrinkScale BOOL 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS RemoveRuleAfterCloseDown RemoveRuleAfterCloseDown0
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY Delay FLOAT 2.2
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 2
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_Patch
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
)";

/// A tiny SoundAction.h with the dome's two sounds, and the values it gives them
constexpr std::string_view k_SyntheticSoundActions = "enum LHSoundAction\n{\n\tNO_SOUND_ACTION = 0,\n"
                                                     "\tSOUND_SPELL_SHIELD_ON = 61,\n\tSOUND_SPELL_SHIELD_HIT = 62,\n};\n";
constexpr int32_t k_HumSound = 61;
constexpr int32_t k_SparkSound = 62;

/// The shield spell the dome's effect belongs to, as the rules that hit it name it
constexpr auto k_ShieldSpell = static_cast<entt::entity>(42);

/// The dome's sphere radius per unit of magnitude (SF_DefenseSphere's MagnitudeFloatProvider0)
constexpr float k_SphereScale = 1.11062f;

std::shared_ptr<const particles::File> Parse(std::string_view text, const char* name)
{
	auto file = particles::File::Parse(text, name);
	return file.has_value() ? std::make_shared<const particles::File>(*file) : nullptr;
}

/// The spell behind an effect, as its rules see it: it keeps every event, answers a hit on a shield with `hitAnswer`
/// (0: the shield held, 1: it gave way) and every other event with 1, and names `spell` as its spell
class AnsweringSink final: public particles::SpellSink
{
public:
	AnsweringSink(int hitAnswer, entt::entity spell)
	    : _hitAnswer(hitAnswer)
	    , _spell(spell)
	{
	}
	int SpellEvent(const particles::SpellEventInfo& event) override
	{
		events.push_back(event);
		return event.type == EventType::HitSpell ? _hitAnswer : 1;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	[[nodiscard]] entt::entity SpellEntity() const override { return _spell; }
	/// The events of that type it was sent, in order
	[[nodiscard]] std::vector<particles::SpellEventInfo> Of(EventType type) const
	{
		std::vector<particles::SpellEventInfo> found;
		std::ranges::copy_if(events, std::back_inserter(found),
		                     [type](const particles::SpellEventInfo& event) { return event.type == type; });
		return found;
	}
	std::vector<particles::SpellEventInfo> events;

private:
	int _hitAnswer;
	entt::entity _spell;
};

/// One step of a bolt cast from `hand` towards +x
void StepBolt(particles::Effect& effect, const glm::vec3& hand)
{
	particles::ProcessInfo info;
	info.handPos = hand;
	info.cameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
	info.enabled = true;
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
}

/// Puts the synthetic SoundAction.h into a fresh resource cache, under the path the sound actions are read from, so
/// no file is opened. The previous file system and resources come back when it goes
class SyntheticSoundActions
{
public:
	SyntheticSoundActions()
	{
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::resources::emplace<resources::Resources>();
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "SoundAction.h";
		Locator::resources::value().GetBlobs().Load(
		    resources::BlobId(path), resources::BlobLoader::FromBufferTag {},
		    std::vector<uint8_t>(k_SyntheticSoundActions.begin(), k_SyntheticSoundActions.end()));
	}

private:
	// built before the constructor's body, so they keep the services that were there before
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;
	const test::RestoreService<Locator::resources> _restoreResources;
};

/// An audio service that hears every animation effect asked of it, and has a channel still playing for every owner,
/// so a particle sound whose atom has gone is released rather than forgotten
class HeardAnimEffects final: public audio::AudioManagerNoOp
{
public:
	struct Request
	{
		audio::AnimKey key;
		audio::AnimAction action;
	};
	[[nodiscard]] audio::Channel PlayingChannel(audio::Owner /*owner*/, audio::BankId /*bank*/) override { return 7; }
	audio::Channel PlayAnimationEffect(audio::Owner /*owner*/, float /*distance*/, const audio::AnimKey& key,
	                                   audio::AnimAction action, audio::BankId /*bank*/, bool /*track*/, float /*minDistance*/,
	                                   float /*maxDistance*/) override
	{
		requests.push_back({key, action});
		return audio::k_NoChannel;
	}
	/// The actions asked for the sounds of that action (the key's last attribute), in order
	[[nodiscard]] std::vector<audio::AnimAction> ActionsOf(int32_t sound) const
	{
		std::vector<audio::AnimAction> actions;
		for (const auto& request : requests)
		{
			if (request.key[4] == sound)
			{
				actions.push_back(request.action);
			}
		}
		return actions;
	}
	std::vector<Request> requests;
};

/// The drawn atoms of an effect made by its creator of that name
std::vector<const particles::Atom*> AtomsOf(const particles::Effect& effect, std::string_view creator)
{
	std::vector<particles::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, particles::Creator::Kind::Point);
	const auto* wanted = effect.FindCreator(std::string(creator));
	std::vector<const particles::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		if (atom.creator == wanted && atom.atom != nullptr)
		{
			atoms.push_back(atom.atom);
		}
	}
	return atoms;
}

/// The particle tests set the game turn: it goes back to 0 after each one, also when an ASSERT stops it
class ShieldMiracleParticles: public ::testing::Test
{
protected:
	void TearDown() override { game_clock::SetTurn(0); }
};

/// The miracles' rules over a fresh world: a registry with the world's lists (the reactions the shield makes reach
/// them), zeroed tables but the shield's upkeep rows, and the inert miracles' service, whose world is a
/// FakeMagicWorld; each put back when the test ends
class ShieldMiracleSpells: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		reactions::Clear();
		_magic = &static_cast<test::InertMagicSystem&>(Locator::magicSystem::emplace<test::InertMagicSystem>());
		auto info = std::make_unique<InfoConstants>();
		_info = info.get();
		Locator::infoConstants::reset(info.release());
		// the shield's own rows, as info.dat gives them: 20 a turn at 30 m, divided by the tribal power
		auto& shield = Effect(MagicType::Shield);
		shield.costPerGameTurn = 20.0f;
		shield.divideCostsByTribalPower = 1;
		_info->magicShield.at(0).radiusForNormalCost = 30.0f;
	}
	void TearDown() override
	{
		reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	[[nodiscard]] test::FakeMagicWorld& World() const { return _magic->FakeWorld(); }
	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] GMagicEffectInfo& Effect(MagicType type) const { return _info->magicEffect.at(static_cast<size_t>(type)); }

	/// A miracle the first player cast at `position`, with its prayer power and a magnitude of 40
	[[nodiscard]] static entt::entity MakeSpell(MagicType type, float prayerPower, glm::vec3 position)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = type;
		component.spellClass = magic::ClassOf(type);
		component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
		magic::chants::SetChants(component, prayerPower);
		component.position = position;
		component.castPos = position;
		component.magnitude = 40.0f;
		return spell;
	}

	[[nodiscard]] static float ChantsOf(entt::entity spell) { return Registry().Get<const Spell>(spell).chants; }

private:
	// put back in reverse order: the tables, then the miracles' service with its stores; the registry and the world's
	// lists go in TearDown, in the game's order
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	InfoConstants* _info {nullptr};
	test::InertMagicSystem* _magic {nullptr};
};

particles::SpellEventInfo EventAt(EventType type, glm::vec3 position, entt::entity target = entt::null,
                                  bool checkShields = false)
{
	return {.type = type,
	        .position = position,
	        .velocity = glm::vec3(1.0f, 0.0f, 0.0f),
	        .strength = 1.0f,
	        .checkShields = checkShields,
	        .target = target};
}
} // namespace

// A fork whose split point lies inside a shield (with a 2.5 m margin) sends the shield's spell a hit where the fork
// enters the sphere: no movement, strength 1, no shield test of its own. When the shield holds (it answers 0) the fork
// ends there and strikes nothing; when it gives way the fork goes on to its target. Either way the shield sparks at
// that point (docs/wiki/magic/fireball-and-lightning.md, the shield on the fork's segment)
TEST_F(ShieldMiracleParticles, ALightningForkEndsWhereItEntersAShieldThatHolds)
{
	// It reaches the map cells and the dead list
	const openblack::test::ScopedWorldSystems worldSystems;
	const auto bolt = Parse(k_Bolt, "SF_LightningBoltShieldTest");
	const auto dome = Parse(k_Dome, "SF_DefenseSphereShieldTest");
	ASSERT_NE(bolt, nullptr);
	ASSERT_NE(dome, nullptr);
	constexpr float k_ForkShieldMargin = 2.5f;
	// the hand 30 m above (1000, 1000); the bolt's one target is a ground point within 12 m of the hand's spot, 2 m up.
	// The dome (18 x 1.11062 = 19.99 m) stands on the ground under the hand: the target is inside it, the hand outside
	const glm::vec3 hand(1000.0f, 30.0f, 1000.0f);
	const glm::vec3 centre(1000.0f, 0.0f, 1000.0f);
	constexpr float k_Magnitude = 18.0f;
	const float entry = k_Magnitude * k_SphereScale + k_ForkShieldMargin;
	for (const int answer : {0, 1})
	{
		game_clock::SetTurn(40);
		AnsweringSink domeSink(1, k_ShieldSpell);
		particles::Effect shield(dome, centre, k_Magnitude);
		shield.SetSink(&domeSink);
		shield.Step(0.1f);
		AnsweringSink boltSink(answer, entt::null);
		particles::Effect effect(bolt, glm::vec3(0.0f), 1.0f);
		effect.SetSink(&boltSink);
		for (int i = 0; i < 3; ++i)
		{
			StepBolt(effect, hand);
		}
		const auto hits = boltSink.Of(EventType::HitSpell);
		ASSERT_FALSE(hits.empty()) << "answer " << answer;
		for (const auto& hit : hits)
		{
			EXPECT_EQ(hit.target, k_ShieldSpell);
			EXPECT_FLOAT_EQ(hit.strength, 1.0f);
			EXPECT_FALSE(hit.checkShields);
			EXPECT_EQ(hit.velocity, glm::vec3(0.0f));
			// where the fork from the hand enters the sphere of the shield's radius plus the margin
			EXPECT_NEAR(glm::distance(hit.position, centre), entry, 1e-2f);
		}
		// a spark on the shield for every hit, whatever the answer
		EXPECT_EQ(shield.TargetPointCount(), hits.size());
		std::vector<particles::Effect::DrawChain> chains;
		effect.CollectChains(1.0f, chains);
		ASSERT_FALSE(chains.empty());
		const auto end = chains.front().joints.back().position;
		if (answer == 0)
		{
			// the shield held: the fork ends at the point it entered and strikes nothing
			EXPECT_TRUE(boltSink.Of(EventType::Landed).empty());
			EXPECT_NEAR(glm::distance(end, hits.back().position), 0.0f, 1e-3f);
		}
		else
		{
			// it gave way: the fork goes on to its target, inside the dome, and strikes it
			EXPECT_FALSE(boltSink.Of(EventType::Landed).empty());
			EXPECT_LT(glm::distance(end, centre), entry - 1.0f);
		}
	}
}

// A blast whose centre lies inside a shield (with the spell's effect radius as the margin, 5 m without a spell) hits
// the shield: a spark where a ray from 200 m above the centre meets the sphere, and a hit sent to the shield's spell at
// the centre. When the shield holds the blast is cancelled for good: none of its blast events follow; when it gives
// way the blast goes on (docs/wiki/magic/lightning-explosion.md, InitCollection)
TEST_F(ShieldMiracleParticles, ABlastCentredInsideAShieldThatHoldsIsCancelled)
{
	// It reaches the map cells and the dead list
	const openblack::test::ScopedWorldSystems worldSystems;
	const auto blast = Parse(k_Blast, "SF_BeamExplosionShieldTest");
	const auto dome = Parse(k_Dome, "SF_DefenseSphereShieldTest");
	ASSERT_NE(blast, nullptr);
	ASSERT_NE(dome, nullptr);
	constexpr float k_NoSpellMargin = 5.0f;
	constexpr float k_Magnitude = 9.0f;
	const glm::vec3 centre(100.0f, 0.0f, 200.0f);
	for (const int answer : {0, 1})
	{
		AnsweringSink domeSink(1, k_ShieldSpell);
		particles::Effect shield(dome, centre, k_Magnitude);
		shield.SetSink(&domeSink);
		shield.Step(0.1f);
		AnsweringSink blastSink(answer, entt::null);
		particles::Effect effect(blast, centre, 1.0f);
		effect.SetSink(&blastSink);
		for (int i = 0; i < 70; ++i)
		{
			effect.Step(0.1f);
		}
		const auto hits = blastSink.Of(EventType::HitSpell);
		ASSERT_EQ(hits.size(), 1u) << "answer " << answer;
		EXPECT_EQ(hits[0].target, k_ShieldSpell);
		EXPECT_FLOAT_EQ(hits[0].position.x, centre.x);
		EXPECT_FLOAT_EQ(hits[0].position.z, centre.z);
		EXPECT_FLOAT_EQ(hits[0].strength, 1.0f);
		EXPECT_FALSE(hits[0].checkShields);
		// the spark: the top of the sphere of the shield's radius plus the margin, straight above the centre
		ASSERT_EQ(shield.TargetPointCount(), 1u);
		glm::vec3 spark(0.0f);
		ASSERT_TRUE(shield.TakeTargetPoint(spark));
		EXPECT_NEAR(spark.x, centre.x, 1e-3f);
		EXPECT_NEAR(spark.y, k_Magnitude * k_SphereScale + k_NoSpellMargin, 1e-3f);
		EXPECT_NEAR(spark.z, centre.z, 1e-3f);
		if (answer == 0)
		{
			EXPECT_TRUE(blastSink.Of(EventType::Point).empty());
		}
		else
		{
			EXPECT_FALSE(blastSink.Of(EventType::Point).empty());
		}
	}
}

// A hit on a shield makes the shield pay the attacker's strength times the attacker's own costPerShieldCollide (the
// fireball's 600), forced, and the attacker one event. While the shield keeps some strength the attack is blocked (the
// hit answers 0) and the shield gives its "struck" reaction, made once and then only stamped with the turn; the hit
// that leaves it with none gets through (1): the shield's own REACT_TO_MAGIC_SHIELD reactions go and the "destroyed"
// one is made. A shield with no strength left lets every attack through at no cost
// (docs/wiki/magic/shields.md#the-spell and #the-particles)
TEST_F(ShieldMiracleSpells, EachBlockedAttackDrainsTheShieldByTheAttackersCostUntilItRunsDryAndBreaks)
{
	auto& fire = Effect(MagicType::Fireball);
	fire.costPerShieldCollide = 600.0f;
	const glm::vec3 where(100.0f, 0.0f, 100.0f);
	const auto shield = MakeSpell(MagicType::Shield, 1500.0f, where);
	const auto fireball = MakeSpell(MagicType::Fireball, 1000.0f, glm::vec3(160.0f, 0.0f, 100.0f));
	const auto watched = reactions::CreateReaction(shield, Reaction::ReactToMagicShield, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(watched, 0u);
	const auto hit = EventAt(EventType::HitSpell, glm::vec3(120.0f, 0.0f, 100.0f), shield);

	// the first hit: 1500 - 600 leaves the shield 0.6 of its strength; blocked, and struck
	game_clock::SetTurn(5);
	EXPECT_EQ(magic::spell_event::SpellEvent(fireball, hit), 0);
	EXPECT_FLOAT_EQ(ChantsOf(shield), 900.0f);
	EXPECT_FLOAT_EQ(ChantsOf(fireball), 1000.0f); // the fireball's event costs nothing
	const auto* data = Registry().TryGet<const magic::SpellShieldData>(shield);
	ASSERT_NE(data, nullptr);
	const auto struck = data->struckReaction;
	ASSERT_NE(struck, 0u);
	ASSERT_NE(reactions::Find(struck), nullptr);
	EXPECT_EQ(reactions::Find(struck)->type, Reaction::ReactToMagicShieldStruck);

	// the second: 300 left; blocked again, the same struck reaction stamped with the turn
	game_clock::SetTurn(9);
	EXPECT_EQ(magic::spell_event::SpellEvent(fireball, hit), 0);
	EXPECT_FLOAT_EQ(ChantsOf(shield), 300.0f);
	EXPECT_EQ(Registry().Get<const magic::SpellShieldData>(shield).struckReaction, struck);
	ASSERT_NE(reactions::Find(struck), nullptr);
	EXPECT_EQ(reactions::Find(struck)->turnCreated, 9u);
	EXPECT_NE(reactions::GetReactionOfTypeInitiatedBy(shield, Reaction::ReactToMagicShield), 0u);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(shield, Reaction::ReactToMagicShieldDestroyed), 0u);

	// the third runs it dry: the fireball gets through and the shield breaks
	EXPECT_EQ(magic::spell_event::SpellEvent(fireball, hit), 1);
	EXPECT_FLOAT_EQ(ChantsOf(shield), -300.0f);
	EXPECT_FLOAT_EQ(magic::GetSpellStrength(shield), 0.0f);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(shield, Reaction::ReactToMagicShield), 0u);
	EXPECT_NE(reactions::GetReactionOfTypeInitiatedBy(shield, Reaction::ReactToMagicShieldDestroyed), 0u);

	// a shield with no strength stops nothing and pays nothing
	EXPECT_EQ(magic::spell_event::SpellEvent(fireball, hit), 1);
	EXPECT_FLOAT_EQ(ChantsOf(shield), -300.0f);
}

// An event that checks shields, landing inside a shield (with the attacker's effect radius as the margin), sends the
// attacker itself a hit with itself as the target: the attacker pays its own strength times its costPerShieldCollide
// and its events, the event is cut and nothing round the point takes it, and the shield is not drained. Outside the
// margin the event acts as usual. With the beam explosion's row: radius 5, 10 an event, 3000 a collision
// (docs/wiki/magic/shields.md#the-particles, the default spell effect with checkShields)
TEST_F(ShieldMiracleSpells, AnAreaEffectThatOnlyMeetsAShieldCostsItsCasterAndNotTheShield)
{
	auto& beam = Effect(MagicType::ExplosionOne);
	beam.radius = 5.0f;
	beam.costPerEvent = 10.0f;
	beam.costPerShieldCollide = 3000.0f;
	const glm::vec3 centre(100.0f, 0.0f, 100.0f);
	const auto shield = MakeSpell(MagicType::Shield, 1000.0f, centre);
	const auto blast = MakeSpell(MagicType::ExplosionOne, 10000.0f, glm::vec3(124.0f, 0.0f, 100.0f));
	const auto dome = Parse(k_Dome, "SF_DefenseSphereShieldTest");
	ASSERT_NE(dome, nullptr);
	AnsweringSink domeSink(1, shield);
	// a dome of 18 x 1.11062 = 19.99 m round the shield's centre
	particles::Effect sphere(dome, centre, 18.0f);
	sphere.SetSink(&domeSink);
	sphere.Step(0.1f);

	// 24 m from the centre: within the sphere and the 5 m margin. Three events (the event, the hit it sends itself and
	// that hit's own) and the collision, all paid by the blast; nothing reached, no spark
	EXPECT_EQ(
	    magic::spell_event::SpellEvent(blast, EventAt(EventType::Point, glm::vec3(124.0f, 0.0f, 100.0f), entt::null, true)), 0);
	EXPECT_FLOAT_EQ(ChantsOf(blast), 10000.0f - 3.0f * 10.0f - 3000.0f);
	EXPECT_FLOAT_EQ(ChantsOf(shield), 1000.0f);
	EXPECT_TRUE(World().appliedAt.empty());
	EXPECT_EQ(sphere.TargetPointCount(), 0u);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(shield, Reaction::ReactToMagicShieldStruck), 0u);

	// 26 m: beyond the margin, the event acts round its point and costs one event
	EXPECT_EQ(
	    magic::spell_event::SpellEvent(blast, EventAt(EventType::Point, glm::vec3(126.0f, 0.0f, 100.0f), entt::null, true)), 1);
	EXPECT_FLOAT_EQ(ChantsOf(blast), 10000.0f - 4.0f * 10.0f - 3000.0f);
	EXPECT_EQ(World().appliedAt.size(), 1u);
	EXPECT_FLOAT_EQ(ChantsOf(shield), 1000.0f);
}

// The dome's hum starts with its root atom, looping, its size class from the dome's radius (the magnitude x 1.11062):
// under 200 m small (3), under 500 m medium (2), else large (1). It is not released softly: when the dome goes the hum
// is stopped at once (the release asks the sound's row to stop), while a spark's hit sound, released softly, ends its
// pass (docs/wiki/audio/magic-and-weather-sounds.md, the lifetime of the particle sound; the sounds of
// SF_DefenseSphere)
TEST_F(ShieldMiracleParticles, TheHumLoopsSizedByTheDomeAndStopsAtOnceWhenTheDomeGoes)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_SHIELD_ON"), k_HumSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAnimEffects&>(Locator::audio::emplace<HeardAnimEffects>());
	const auto file = Parse(k_DomeSounds, "SF_DefenseSphereSoundTest");
	ASSERT_NE(file, nullptr);
	struct Case
	{
		float magnitude;
		int32_t size;
	};
	// 44.4 m, 333.2 m and 666.4 m
	for (const auto& [magnitude, size] : {Case {40.0f, 3}, Case {300.0f, 2}, Case {600.0f, 1}})
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), magnitude);
		effect.Step(0.1f);
		const auto hums = AtomsOf(effect, "ParticlePointCreator_Hum");
		ASSERT_EQ(hums.size(), 1u) << "magnitude " << magnitude;
		ASSERT_EQ(hums[0]->sounds.size(), 1u);
		const auto& action = hums[0]->sounds.front()->action;
		EXPECT_EQ(action.action, k_HumSound);
		EXPECT_EQ(action.size, size) << "magnitude " << magnitude;
		EXPECT_EQ(action.flags, particles::SoundAction::k_Looping);
	}
	// the dome goes (its atoms with it): the next sound turn stops the hum, and releases a spark's sound softly
	heard.requests.clear();
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 40.0f);
		effect.Step(0.1f);
		effect.AddTargetPoint(glm::vec3(100.0f, 30.0f, 100.0f));
		effect.Step(0.1f);
		ASSERT_EQ(AtomsOf(effect, "ParticlePointCreator_Spark").size(), 1u);
	}
	audio::spell_sounds::ProcessTurn(0.1f);
	const auto hum = heard.ActionsOf(k_HumSound);
	ASSERT_FALSE(hum.empty());
	EXPECT_EQ(hum.back(), audio::AnimAction::Stop);
	const auto spark = heard.ActionsOf(k_SparkSound);
	ASSERT_FALSE(spark.empty());
	EXPECT_EQ(spark.back(), audio::AnimAction::Release);
	// released once only
	const auto asked = heard.requests.size();
	audio::spell_sounds::ProcessTurn(0.1f);
	EXPECT_EQ(heard.requests.size(), asked);
	audio::spell_sounds::Clear();
}

// Every hit on the dome is an impact point of its effect, and each makes a spark atom that plays the spark's hit sound
// once, released softly, of the default medium size (docs/wiki/magic/shields.md#the-particles, UpdateRuleShieldSpark;
// the sounds of SF_DefenseSphere)
TEST_F(ShieldMiracleParticles, EachHitOnTheDomePlaysASparkSound)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_SHIELD_HIT"), k_SparkSound);
	const auto file = Parse(k_DomeSounds, "SF_DefenseSphereSoundTest");
	ASSERT_NE(file, nullptr);
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 40.0f);
		effect.Step(0.1f);
		EXPECT_TRUE(AtomsOf(effect, "ParticlePointCreator_Spark").empty());
		// two hits in one turn
		effect.AddTargetPoint(glm::vec3(100.0f, 40.0f, 100.0f));
		effect.AddTargetPoint(glm::vec3(140.0f, 10.0f, 100.0f));
		effect.Step(0.1f);
		const auto sparks = AtomsOf(effect, "ParticlePointCreator_Spark");
		ASSERT_EQ(sparks.size(), 2u);
		for (const auto* spark : sparks)
		{
			ASSERT_EQ(spark->sounds.size(), 1u);
			const auto& action = spark->sounds.front()->action;
			EXPECT_EQ(action.action, k_SparkSound);
			EXPECT_EQ(action.flags, particles::SoundAction::k_SoftRelease);
			EXPECT_EQ(action.size, 2);
		}
		// a later hit plays its own
		effect.AddTargetPoint(glm::vec3(60.0f, 10.0f, 100.0f));
		effect.Step(0.1f);
		EXPECT_EQ(AtomsOf(effect, "ParticlePointCreator_Spark").size(), 3u);
	}
	audio::spell_sounds::Clear();
}

// A spark the file gives neither a life nor a wiggle lives the rule's own 3 s and its arc wiggles by the rule's own
// 0.08 of the radius, as the original's rule makes them (docs/wiki/magic/shields.md#the-particles, UpdateRuleShieldSpark:
// each arc atom i of N at u = i / N, R WiggleAmpl sin(k u pi) sin(4 t + a3) and R WiggleAmpl sin((k + 1) u pi)
// sin(2 t + a3) across the line from the centre to the hit)
TEST_F(ShieldMiracleParticles, ASparkLivesThreeSecondsAndItsArcWigglesByAnEighthOfATenthOfTheRadius)
{
	constexpr float k_Life = 3.0f;
	constexpr float k_Wiggle = 0.08f;
	constexpr float k_Step = 0.1f;
	constexpr float k_Magnitude = 40.0f;
	const SyntheticSoundActions soundActions;
	const auto file = Parse(k_DomeSparks, "SF_DefenseSphereSparkTest");
	ASSERT_NE(file, nullptr);
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), k_Magnitude);
		effect.Step(k_Step);
		const float radius = k_Magnitude * k_SphereScale;
		effect.AddTargetPoint(glm::vec3(100.0f + 0.6f * radius, 0.8f * radius, 100.0f));
		effect.Step(k_Step);
		auto sparks = AtomsOf(effect, "ParticlePointCreator_Spark");
		ASSERT_EQ(sparks.size(), 1u);
		const auto* spark = sparks.front();
		// its three angles and its wiggle count (2 to 5), as the rule drew them
		glm::vec4 drawn(0.0f);
		for (const auto& [rule, data] : spark->data)
		{
			if (data.w >= 2.0f && data.w <= 5.0f)
			{
				drawn = data;
			}
		}
		ASSERT_GE(drawn.w, 2.0f);
		const float a3 = drawn.z;
		const auto wiggles = static_cast<float>(static_cast<int>(drawn.w));
		const glm::vec3 axis = glm::normalize(spark->position);
		const float pi = std::numbers::pi_v<float>;
		float widest = 0.0f;
		float lastSeen = 0.0f;
		int steps = 0;
		while (!AtomsOf(effect, "ParticlePointCreator_Spark").empty() && steps < 50)
		{
			const float t = effect.AtomAge(*spark) - k_Step; // the age the rule saw in the step just made
			EXPECT_LE(t, k_Life + 1e-4f);
			lastSeen = std::max(lastSeen, t);
			ASSERT_FALSE(spark->subCollections.empty());
			const auto& arc = spark->subCollections.front()->atoms;
			if (!arc.empty() && t > 0.0f)
			{
				ASSERT_EQ(arc.size(), 10u);
				const auto n = static_cast<float>(arc.size());
				float index = n;
				for (const auto& atom : arc)
				{
					const float u = index / n;
					const float across = std::sin(wiggles * u * pi) * std::sin(4.0f * t + a3);
					const float other = std::sin((wiggles + 1.0f) * u * pi) * std::sin(2.0f * t + a3);
					const glm::vec3 off = atom->position - glm::dot(atom->position, axis) * axis;
					EXPECT_NEAR(glm::length(off), radius * k_Wiggle * std::sqrt(across * across + other * other), 1e-3f)
					    << "atom " << index << " at " << t << " s";
					widest = std::max(widest, glm::length(off));
					index -= 1.0f;
				}
			}
			effect.Step(k_Step);
			++steps;
		}
		// kept while it is no older than its 3 s, then gone
		EXPECT_TRUE(AtomsOf(effect, "ParticlePointCreator_Spark").empty());
		EXPECT_GT(lastSeen, k_Life - 2.0f * k_Step);
		EXPECT_GT(widest, 0.01f * radius);
	}
	audio::spell_sounds::Clear();
}

namespace
{
/// One step of the dome's look with the cast's curl and the shield's strength in its process info
void StepDome(particles::Effect& effect, float curl, float strength)
{
	particles::ProcessInfo info;
	info.curl = curl;
	info.power = strength;
	info.enabled = true;
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
}

/// The dome's root atom, nullptr once it has gone
const particles::Atom* RootOf(const particles::Effect& effect)
{
	const auto roots = AtomsOf(effect, "ParticlePointCreator_Root");
	return roots.empty() ? nullptr : roots.front();
}

/// The angle the root has turned about its own Y axis, from its first row
float SpinOf(const particles::Atom& root)
{
	return std::atan2(root.rotation[0].z, root.rotation[0].x);
}
} // namespace

// While the dome is open its root grows from 0.1 to full size over its first 2 s and then stays at 1. A dome cast by a
// player spins at the cast's curl (x 1, never beyond 5 rad/s either way), the spin fading out over 5 s; a dome with
// no player never spins (docs/wiki/magic/shields.md#the-particles, UR_InitialSpin; SF_DefenseSphere's UR_ChangeScale0
// and UR_InitialSpin0)
TEST_F(ShieldMiracleParticles, TheDomeGrowsFromATenthToFullSizeOverTwoSecondsAndKeepsTheCastsSpin)
{
	const auto file = Parse(k_DomeLook, "SF_DefenseSphereLookTest");
	ASSERT_NE(file, nullptr);
	struct Case
	{
		float curl;
		float spin; ///< the turn of the first step, 0.1 s at the full spin
	};
	for (const auto& [curl, spin] : {Case {2.0f, 0.2f}, Case {-2.0f, -0.2f}, Case {8.0f, 0.5f}, Case {-9.0f, -0.5f}})
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 40.0f);
		effect.SetPlayer(0);
		StepDome(effect, curl, 1.0f);
		const auto* root = RootOf(effect);
		ASSERT_NE(root, nullptr);
		// a tenth of its size at the start (at most one step of the growth, 0.045, past it)
		EXPECT_GE(root->ruleScale, 0.1f);
		EXPECT_LE(root->ruleScale, 0.1f + 0.045f + 1e-5f);
		// the first step's turn: the full spin less at most one step of its fade
		EXPECT_LE(std::abs(SpinOf(*root)), std::abs(spin) + 1e-5f) << "curl " << curl;
		EXPECT_GE(std::abs(SpinOf(*root)), std::abs(spin) * 0.98f - 1e-5f) << "curl " << curl;
		EXPECT_EQ(SpinOf(*root) > 0.0f, spin > 0.0f) << "curl " << curl;
		float previous = root->ruleScale;
		for (int i = 1; i < 25; ++i)
		{
			StepDome(effect, curl, 1.0f);
			EXPECT_GE(RootOf(effect)->ruleScale, previous);
			previous = RootOf(effect)->ruleScale;
		}
		// full size from 2 s, and it stays so
		EXPECT_FLOAT_EQ(RootOf(effect)->ruleScale, 1.0f);
		// the spin has faded out by 5 s: the root no longer turns
		for (int i = 25; i < 60; ++i)
		{
			StepDome(effect, curl, 1.0f);
		}
		const glm::mat3 still = RootOf(effect)->rotation;
		for (int i = 0; i < 10; ++i)
		{
			StepDome(effect, curl, 1.0f);
		}
		EXPECT_EQ(RootOf(effect)->rotation, still) << "curl " << curl;
		EXPECT_FLOAT_EQ(RootOf(effect)->ruleScale, 1.0f);
	}
	// no player: no spin
	particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 40.0f);
	for (int i = 0; i < 5; ++i)
	{
		StepDome(effect, 3.0f, 1.0f);
	}
	ASSERT_NE(RootOf(effect), nullptr);
	EXPECT_EQ(RootOf(effect)->rotation, glm::mat3(1.0f));
}

// A surface patch fades in at 30 alpha a second, full after 8.5 s; its collection's alpha is the shield's strength x
// 255, never below 40 nor above 255, so a weakening shield fades down to a floor (docs/wiki/magic/shields.md
// #the-particles, UR_VapourEndEffect and SetCollectionAlpha; SF_DefenseSphere's StrengthFloatProvider_ForAlpha)
TEST_F(ShieldMiracleParticles, ThePatchesFadeInOverEightAndAHalfSecondsAndWithTheStrengthDownToAFloor)
{
	const auto file = Parse(k_DomeLook, "SF_DefenseSphereLookTest");
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 40.0f);
	const auto patch = [&effect]() -> const particles::Atom* {
		const auto* root = RootOf(effect);
		if (root == nullptr || root->subCollections.empty() || root->subCollections.front()->atoms.empty())
		{
			return nullptr;
		}
		return root->subCollections.front()->atoms.front().get();
	};
	for (int i = 0; i < 30; ++i)
	{
		StepDome(effect, 0.0f, 1.0f);
	}
	ASSERT_NE(patch(), nullptr);
	// about 3 s old: 3 x 30, give or take a step
	EXPECT_GE(patch()->colour[3], 84);
	EXPECT_LE(patch()->colour[3], 90);
	EXPECT_FLOAT_EQ(patch()->collection->alpha, 255.0f);
	for (int i = 30; i < 80; ++i)
	{
		StepDome(effect, 0.0f, 1.0f);
	}
	// under 8 s old: not full yet
	EXPECT_LT(patch()->colour[3], 255);
	for (int i = 80; i < 90; ++i)
	{
		StepDome(effect, 0.0f, 1.0f);
	}
	// past 8.5 s: full
	EXPECT_EQ(patch()->colour[3], 255);
	// the collection's alpha follows the strength, 40 at the least
	struct Case
	{
		float strength;
		float alpha;
	};
	for (const auto& [strength, alpha] : {Case {0.4f, 102.0f}, Case {0.1f, 40.0f}, Case {0.0f, 40.0f}, Case {1.0f, 255.0f}})
	{
		StepDome(effect, 0.0f, strength);
		ASSERT_NE(patch(), nullptr);
		EXPECT_FLOAT_EQ(patch()->collection->alpha, alpha) << "strength " << strength;
	}
}

// When the shield closes its dome stops growing and shrinks to nothing over 2 s from the size it had, and its root goes
// 2.2 s after the close-down (docs/wiki/physics/particles.md, AR_FadeOutOnceConditionTrue; SF_DefenseSphere's
// AR_FadeOutOnceConditionTrue0 and RemoveRuleAfterCloseDown0)
TEST_F(ShieldMiracleParticles, OnCloseDownTheDomeShrinksOverTwoSecondsAndGoesAtTwoPointTwo)
{
	const auto file = Parse(k_DomeLook, "SF_DefenseSphereLookTest");
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 40.0f);
	for (int i = 0; i < 25; ++i)
	{
		StepDome(effect, 0.0f, 1.0f);
	}
	ASSERT_NE(RootOf(effect), nullptr);
	ASSERT_FLOAT_EQ(RootOf(effect)->ruleScale, 1.0f);
	effect.CloseDown();
	StepDome(effect, 0.0f, 1.0f);
	ASSERT_NE(RootOf(effect), nullptr);
	float previous = RootOf(effect)->ruleScale;
	EXPECT_GE(previous, 0.95f - 1e-5f);
	for (int i = 1; i < 11; ++i)
	{
		StepDome(effect, 0.0f, 1.0f);
		ASSERT_NE(RootOf(effect), nullptr);
		EXPECT_LE(RootOf(effect)->ruleScale, previous);
		previous = RootOf(effect)->ruleScale;
	}
	// a second after the close-down: about half its size, give or take a step
	EXPECT_NEAR(RootOf(effect)->ruleScale, 0.5f, 0.05f + 1e-5f);
	for (int i = 11; i < 22; ++i)
	{
		StepDome(effect, 0.0f, 1.0f);
	}
	// 2.1 s after it: nothing left of it, but still there
	ASSERT_NE(RootOf(effect), nullptr);
	EXPECT_FLOAT_EQ(RootOf(effect)->ruleScale, 0.0f);
	for (int i = 22; i < 25; ++i)
	{
		StepDome(effect, 0.0f, 1.0f);
	}
	// 2.4 s after it, past its 2.2 s: gone
	EXPECT_EQ(RootOf(effect), nullptr);
}
