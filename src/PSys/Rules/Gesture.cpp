/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The gesture effects' rules: UR_GesturingRecognised (SF_Gesture, the sparkles of a recognised gesture), ZR_ChainGesture
// and CreateRuleMakeChain (SF_GestureChain, the trail at the hand, drawn by ParticleChainCreator).
// Wiki: docs/bw1-notes/magic.md, "Efectos de utilidad".

#include <cmath>

#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "Locator.h"
#include "Magic/Gestures/GestureShapes.h"
#include "PSys/Noise.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"
#include "PSys/Utility.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// GPlayer::GetPlayerColour 0x64D800 of this interface's player: 0xBFF0B8[GetRemapedPlayer(player number)] (the remap is
/// the identity here, as in PSys/TownBelief.cpp; the local player is number 0). (inf) every record is drawn in this
/// colour: the record's own player and GetRemapedPlayer 0x64D790 are not used
constexpr uint32_t k_LocalPlayerColour = 0xFF4646;
/// fn_0068DE90 / fn_0068DEA0: when the collection's pulse starts, and how long it lasts
constexpr float k_PulseStart = 2.4f;
constexpr float k_PulseLength = 2.1f;
/// the light sheet's points (fn_0083E710)
constexpr int k_LightSheetPoints = 50;

/// UR_GesturingRecognised (ModifyAtomCollection 0x6884F0, ModifySubCollection 0x688910, DefineProperties 0x6B0560): one
/// atom per recognised gesture record, whose sub-collection holds NumAtoms sprites that rise from the drawn stroke onto
/// the gesture's ideal shape lifted towards the camera, wiggle and fade
class GesturingRecognised final: public Modifier
{
public:
	explicit GesturingRecognised(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , lightSheetHeightScale(object.Float("LightSheetHeightScale", 0.0f))       // +0x2C
	    , timeToIdeal(object.Float("TimeToIdeal", 0.0f))                           // +0x30
	    , interpGain(object.Float("InterpGain", 0.0f))                             // +0x38
	    , dieAge(object.Float("DieAge", 0.0f))                                     // +0x3C
	    , lightSheetDieAge(object.Float("LightSheetDieAge", 0.0f))                 // +0x40
	    , maxAlpha(object.Float("MaxAlpha", 0.0f))                                 // +0x44
	    , numAtoms(object.Int("NumAtoms", 10))                                     // +0x48
	    , spriteCreator(object.String("SpriteCreator"))                            // +0x4C
	    , wiggleFreq(object.Float("WiggleFreq", 0.0f))                             // +0x50
	    , wiggleMag(object.Float("WiggleMag", 0.0f))                               // +0x54
	    , wiggleMagY(object.Float("WiggleMagY", 0.0f))                             // +0x58
	    , wiggleSpeed(object.Float("WiggleSpeed", 0.0f))                           // +0x5C
	    , wigglePhaseSpeed(object.Float("WigglePhaseSpeed", 0.0f))                 // +0x60
	    , dispersalTime(object.Float("DispersalTime", 0.0f))                       // +0x64
	    , collectionAlphaPulse(object.Int("CollectionAlphaPulse", 0))              // +0x70
	    , collectionAlphaInit(object.Int("CollectionAlphaInit", 0))                // +0x74
	    , shrinkTimeAfterDispersal(object.Float("ShrinkTimeAfterDispersal", 0.0f)) // +0x78
	    , sparkleGroup(object.Int("SparkleGroup", -1))                             // +0x8C
	    , goToIdeal(object.Bool("GoToIdeal", false))                               // +0x90
	    , doTransition(object.Bool("DoTransition", false))                         // +0x91
	{
		// (inferido) the defaults above: the ctor was not read (SF_Gesture sets DieAge, TimeToIdeal and the rest)
		// HeightOffset (+0x34), ExplodeFactor (+0x68), ExplodePause (+0x6C) and HandPulseDuration (+0x7C) are properties
		// too; the first three are not used by the rule, the last one only by the hand pulse (not ported)
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& pending = utility::PendingRecognised();
		// a new record: one atom of PCreator (its NextGroups sub-collection gets the sprites) and the recognition sound
		if (!pending.empty())
		{
			const auto* pointCreator = effect.FindCreator(creator);
			if (pointCreator == nullptr)
			{
				return false;
			}
			auto& atom = effect.NewAtom(collection, pointCreator, nextGroups);
			auto& data = _data[&atom];
			data.record = std::move(pending.back());
			pending.pop_back();
			PlayRecognisedSound(data);
		}
		// the atoms older than DieAge go (with their sprites)
		std::erase_if(collection.atoms, [&](const std::unique_ptr<Atom>& atom) {
			if (effect.AtomAge(*atom) > dieAge)
			{
				_data.erase(atom.get());
				return true;
			}
			return false;
		});
		for (auto& atom : collection.atoms)
		{
			auto found = _data.find(atom.get());
			if (found == _data.end())
			{
				continue;
			}
			auto& data = found->second;
			for (auto& sub : atom->subCollections)
			{
				if (sub->group == sparkleGroup && data.initialised)
				{
					// the sparkle group (none in SF_Gesture): the atom runs along the ideal once every 2 s and fades out in
					// its last 2 s (age / 2 [0xC02648] at 0x68872D, < 2 [0x8AB478] at 0x68877E, x 127.5 [0x92B6EC] at
					// 0x68878B)
					atom->position = data.record.ideal.At(std::fmod(effect.AtomAge(*atom) / 2.0f, 1.0f));
					const float left = dieAge - effect.AtomAge(*atom);
					if (left < 2.0f)
					{
						const auto alpha = static_cast<uint8_t>(std::clamp(left * 127.5f, 0.0f, 255.0f));
						atom->colour[3] = alpha;
						sub->alpha = static_cast<float>(alpha);
					}
				}
				else
				{
					ModifySubCollection(effect, *sub, data);
				}
			}
			// the LightSheet (+0x2C, fn_0083E710): 50 points along the ideal, the player's colour, height = scale x
			// LightSheetHeightScale, +0x18 = 0.03, and its alpha 1 - (2f - 1)^2 over LightSheetDieAge. The data is kept;
			// LH3D's LightSheet draw is not ported.
			if (!data.lightSheetMade)
			{
				data.lightSheetMade = true;
				data.lightSheet.clear();
				for (int i = 0; i < k_LightSheetPoints; ++i)
				{
					data.lightSheet.push_back(data.record.ideal.At(static_cast<float>(i) * (1.0f / 49.0f)));
				}
				data.lightSheetHeight = data.scale * lightSheetHeightScale;
			}
			const float f = std::clamp(effect.AtomAge(*atom) / lightSheetDieAge, 0.0f, 1.0f);
			const float g = 2.0f * f - 1.0f;
			data.lightSheetAlpha = 1.0f - g * g;
		}
		return true;
	}

private:
	/// UR_GesturingRecognised::AtomData (0xC8 bytes, fn_00688140)
	struct AtomData
	{
		magic::gestures::RecognisedGesture record; ///< +0x94
		bool initialised {false};                  ///< +0xC4
		float start {0.0f};                        ///< +0xA0 the collection's age when it began
		float scale {1.0f};                        ///< +0xA4 the ideal's length x 0.01
		glm::vec3 min {0.0f};                      ///< +0xA8 the ideal's box
		glm::vec3 max {0.0f};                      ///< +0xB4
		std::vector<int> order;                    ///< +0x98 / +0x9C the atoms' shuffled wiggle phases
		bool lightSheetMade {false};               ///< +0x28
		std::vector<glm::vec3> lightSheet;         ///< +0x2C
		float lightSheetHeight {0.0f};
		float lightSheetAlpha {0.0f};
		/// the atom data as a channel owner (0x688643: owner +0x20 = this), for another interface's recognition sound
		uint32_t soundOwner {0};
	};

	/// 0x6885BB..0x68865B: fn_006882F0 (the record's status +0x38 is MyInterface()'s +0x39C: this computer's interface;
	/// openblack has only that one, RecognisedGesture::fromInterface) plays LH_SAMPLE_G_SPELLGESTURERECOGNISE (0x24) in
	/// 2D: GAudio::PlaySoundEffect 0x429D60(NULL, 0x24, mode 3, loops 0, +0x10 0, is3D 0, IN_GAME); another
	/// interface's one plays at its hand: LH_SamplePlayOptions with bank +0x04 InGame, owner +0x20 the atom data, sample
	/// +0x24 0x24, is3D +0x08 1, track +0x0C 0, the point +0x30 the record's +0x3C, then 0x429E30
	static void PlayRecognisedSound(AtomData& data)
	{
		if (data.record.fromInterface)
		{
			audio::PlaySoundEffect(audio::Owner::None(), 0x24, 3, 0, false, false, audio::SfxBank::InGame);
			return;
		}
		if (data.soundOwner == 0)
		{
			data.soundOwner = audio::NewObjectId();
		}
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), 0x24};
		options.owner = audio::Owner::Object(data.soundOwner);
		options.is3D = true;
		options.track = false;
		options.position = data.record.handPosition;
		audio::PlaySoundEffect(options);
	}

	/// ModifySubCollection 0x688910
	void ModifySubCollection(Effect& effect, Collection& sub, AtomData& data) const
	{
		auto& stroke = data.record.stroke;
		auto& ideal = data.record.ideal;
		if (!data.initialised)
		{
			data.start = effect.CollectionAge(sub);
			data.initialised = true;
			ideal.Measure();
			sub.alpha = static_cast<float>(static_cast<uint8_t>(collectionAlphaInit));
			// [0xD4EB60] is never written: the scale is always the ideal's length / 100
			data.scale = ideal.Length() * 0.01f;
			// every ideal point comes towards the camera until it is `scale` higher (at most half-way)
			const glm::vec3 camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
			for (size_t k = 0; k < ideal.Size(); ++k)
			{
				glm::vec3 d = camera - ideal[k];
				float length = 0.0f;
				if (d != glm::vec3(0.0f))
				{
					length = glm::length(d);
					d /= length;
				}
				if (d.y > 0.0f)
				{
					const float s = std::min(data.scale / d.y, length * 0.5f);
					ideal[k] += d * s;
				}
			}
			stroke.Measure();
			ideal.Measure();
			// the sprites: SpriteCreator's atoms in the player's colour (the alpha is set below), their scale x `scale`
			const auto* sprites = effect.FindCreator(spriteCreator);
			for (int i = 0; sprites != nullptr && i < numAtoms; ++i)
			{
				auto& atom = effect.NewAtom(sub, sprites, {});
				atom.colour[0] = static_cast<uint8_t>(k_LocalPlayerColour >> 16);
				atom.colour[1] = static_cast<uint8_t>(k_LocalPlayerColour >> 8);
				atom.colour[2] = static_cast<uint8_t>(k_LocalPlayerColour);
				atom.baseScale *= data.scale;
			}
			if (sub.parent != nullptr)
			{
				sub.parent->ruleScale *= data.scale;
			}
			data.min = ideal.Size() > 0 ? ideal[0] : glm::vec3(0.0f);
			data.max = data.min;
			for (size_t k = 0; k < ideal.Size(); ++k)
			{
				data.min = glm::min(data.min, ideal[k]);
				data.max = glm::max(data.max, ideal[k]);
			}
			// a shuffled order: identity, then 2 x NumAtoms swaps of two PSysRand(NumAtoms) slots
			data.order.resize(static_cast<size_t>(std::max(numAtoms, 0)));
			std::iota(data.order.begin(), data.order.end(), 0);
			for (int i = 0; !data.order.empty() && i < 2 * numAtoms; ++i)
			{
				// UR_GesturingRecognised 0x688E01 / 0x688E0C
				const auto a = static_cast<size_t>(effect.Rand(numAtoms));
				const auto b = static_cast<size_t>(effect.Rand(numAtoms));
				std::swap(data.order[a], data.order[b]);
			}
		}
		const float age = effect.CollectionAge(sub);
		// t: how far towards the ideal (0..1 over TimeToIdeal), e: t pulled towards smoothstep by InterpGain
		const float t = std::min(age - data.start, timeToIdeal) / timeToIdeal;
		const float e = t + ((3.0f - 2.0f * t) * t * t - t) * interpGain;
		const float width = data.max.x - data.min.x;
		const float depth = data.max.z - data.min.z;
		float wiggle = (std::cos(age * wigglePhaseSpeed) + 1.0f) * 0.5f;
		const float peak = t * maxAlpha;
		if (age - dispersalTime >= 0.0f)
		{
			wiggle *= std::clamp(1.0f - (age - dispersalTime) / shrinkTimeAfterDispersal, 0.0f, 1.0f);
		}
		// after 2.4 s the collection pulses from CollectionAlphaPulse to 0 over 2.1 s; the hand of this interface pulses in
		// the player's colour over HandPulseDuration (the hand object's vt 0x2C: not ported)
		if (const float h = age - k_PulseStart; h > 0.0f)
		{
			const float f = std::clamp(1.0f - h / k_PulseLength, 0.0f, 1.0f);
			sub.alpha = static_cast<float>(static_cast<uint8_t>(static_cast<int>(static_cast<float>(collectionAlphaPulse) * f)));
		}
		const float step = sub.atoms.size() > 1 ? 1.0f / static_cast<float>(sub.atoms.size() - 1) : 0.0f;
		// the max(dt, eps) is a port guard: the original multiplies by [0xD4E0F0] = 1/dt directly
		const float perSecond = 1.0f / std::max(effect.GetDt(), 1e-4f); // [0xD4E0F0]
		for (size_t i = 0; i < sub.atoms.size(); ++i)
		{
			auto& atom = *sub.atoms[i];
			const float u = static_cast<float>(i) * step;
			// alpha: the two ends of the line appear first and meet in the middle as t grows
			float alpha = 0.0f;
			if (u < t)
			{
				alpha += peak - peak * (u / t);
			}
			if (1.0f - u < t)
			{
				alpha += peak - peak * ((1.0f - u) / t);
			}
			atom.colour[3] = static_cast<uint8_t>(static_cast<int>(std::clamp(alpha, 0.0f, 255.0f)));
			glm::vec3 position;
			if (doTransition)
			{
				const auto from = stroke.At(u);
				position = from + (ideal.At(u) - from) * e;
			}
			else
			{
				position = goToIdeal ? ideal.At(u) : stroke.At(u);
			}
			// the wiggle: value noise at the atom's shuffled phase, x and z by the ideal's box, y upwards only (the
			// phase offsets +0.3 [0x8AB23C] at 0x6892D3 and +0.7 [0x8AB238] at 0x689308)
			const float phase = static_cast<float>(i < data.order.size() ? data.order[i] : 0) * step;
			const float nx = noise::VSNoise1To1(phase * wiggleFreq + age * wiggleSpeed) * width * wiggleMag;
			const float nz = noise::VSNoise1To1((phase + 0.3f) * wiggleFreq + age * wiggleSpeed) * depth * wiggleMag;
			const float ny =
			    (noise::VSNoise1To1((phase + 0.7f) * wiggleFreq + age * wiggleSpeed) + 1.0f) * 0.5f * wiggleMagY * depth;
			position += glm::vec3(nx, ny, nz) * wiggle;
			atom.velocity = (position - atom.position) * perSecond;
			atom.position = position;
		}
	}

	std::string creator;
	std::vector<int> nextGroups;
	float lightSheetHeightScale;
	float timeToIdeal;
	float interpGain;
	float dieAge;
	float lightSheetDieAge;
	float maxAlpha;
	int numAtoms;
	std::string spriteCreator;
	float wiggleFreq;
	float wiggleMag;
	float wiggleMagY;
	float wiggleSpeed;
	float wigglePhaseSpeed;
	float dispersalTime;
	int collectionAlphaPulse;
	int collectionAlphaInit;
	float shrinkTimeAfterDispersal;
	int sparkleGroup;
	bool goToIdeal;
	bool doTransition;
	/// the atoms' AtomData (the original keeps it in the atom's modifier-data list +0x24)
	mutable std::unordered_map<const Atom*, AtomData> _data;
};

/// ZR_ChainGesture (ModifyAtomCollection 0x68A080, emission fn_0068A330, DefineProperties 0x6B0170): while the effect is
/// enabled (PSysManager::IsInState(2), PSysProcessInfo +0x38) one head atom at the gesture position trails a chain: its
/// NextGroups sub-collection's joints are a shift register, a new joint at the head every MinEmitDist of movement (at
/// most count / DieAge joints per second). When the effect stops being enabled the head stops emitting and goes 5 s
/// later.
class ChainGesture final: public Modifier
{
public:
	explicit ChainGesture(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , dieAge(object.Float("DieAge", 0.0f))                        // +0x2C
	    , minEmitDist(object.Float("MinEmitDist", 0.0f))              // +0x30
	    , adjustInitialScale(object.String("AdjustInitialScale"))     // +0x34
	    , inTestMode(object.Bool("InTestMode", false))                // +0x3A
	{
		// (inferido) the defaults above: the ctor was not read
		// PredictNextPosition (+0x38) and DrawOnFirstUpdate (+0x39) are properties too; this rule does not read them
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* head = effect.FindCreator(creator);
		if (head == nullptr)
		{
			return false;
		}
		// ParentCollectionData +0x20: emitting last time; a start of emission begins a new chain
		const bool emitting = inTestMode || effect.GetProcessInfo().enabled;
		const bool was = slot.state.x != 0.0f;
		slot.state.x = emitting ? 1.0f : 0.0f;
		if (emitting != was && !was)
		{
			for (const auto& atom : collection.atoms)
			{
				_data.erase(atom.get());
			}
			collection.atoms.clear();
			effect.NewAtom(collection, head, nextGroups);
		}
		for (auto it = collection.atoms.begin(); it != collection.atoms.end();)
		{
			auto& atom = **it;
			auto& data = _data[&atom];
			if (!data.emitting && effect.AtomAge(atom) - data.stopTime > 5.0f) // (inferido) the 0x68A080 offset of 5 s
			{
				_data.erase(&atom);
				it = collection.atoms.erase(it);
				continue;
			}
			if (data.emitting)
			{
				if (!emitting)
				{
					data.emitting = false;
					data.stopTime = effect.AtomAge(atom);
				}
				else
				{
					if (atom.subCollections.empty())
					{
						return false;
					}
					Emit(effect, data, *atom.subCollections.front(), atom.position);
				}
			}
			++it;
		}
		return true;
	}

private:
	/// ZR_ChainGesture::AtomData (0x48 bytes, fn_0068A290)
	struct AtomData
	{
		glm::vec3 previousHead {0.0f}; ///< +0x20 the head's position last step
		glm::vec3 lastEmitted {0.0f};  ///< +0x2C where the last joint came out
		float wanted {0.0f};           ///< +0x38 joints owed so far
		float emitted {0.0f};          ///< +0x3C joints emitted
		float stopTime {0.0f};         ///< +0x40 the head's age when the emission stopped
		bool emitting {true};          ///< +0x44
		bool first {true};             ///< +0x45
	};

	/// fn_0068A330
	void Emit(Effect& effect, AtomData& data, Collection& chain, const glm::vec3& head) const
	{
		const size_t count = chain.atoms.size();
		if (count == 0)
		{
			return;
		}
		if (data.first)
		{
			data.lastEmitted = head;
			data.previousHead = head;
			for (auto& joint : chain.atoms)
			{
				joint->position = head;
				joint->ruleScale = 0.0f;
			}
		}
		const float initialScale = adjustInitialScale.empty() ? 1.0f : effect.FloatProvider(adjustInitialScale, 1.0f);
		const float before = data.wanted;
		const float dt = effect.GetDt(); // [0xD4E0EC]
		const float maxStep = static_cast<float>(count) / dieAge * dt;
		float step = data.emitted == 0.0f ? 1.0f : glm::distance(data.lastEmitted, head) / minEmitDist;
		if (!(maxStep > step))
		{
			step = maxStep;
		}
		if (!data.emitting)
		{
			step = 0.0f;
		}
		data.wanted += step;
		const float after = data.wanted;
		while (data.wanted - 1.0f > data.emitted)
		{
			data.emitted += 1.0f;
			// every joint takes the state of the one before it (position, age, initial scale)
			glm::vec3 position(0.0f);
			float age = 0.0f;
			float scale = 1.0f;
			for (size_t i = 0; i < count; ++i)
			{
				auto& joint = *chain.atoms[i];
				const glm::vec3 p = joint.position;
				const float a = effect.AtomAge(joint);
				const float s = joint.baseScale;
				if (i != 0)
				{
					joint.position = position;
					joint.birth = effect.GetAge() - age; // AtomCore fn_00673CE0 (SetAtomAge)
					joint.baseScale = scale;
				}
				position = p;
				age = a;
				scale = s;
			}
			// the new joint at the head, between last step's head and this one's
			auto& first = *chain.atoms.front();
			glm::vec3 at = head;
			if (after != before)
			{
				const float fraction = (data.emitted - before) / (after - before);
				at = data.previousHead + (head - data.previousHead) * fraction;
				first.birth = effect.GetAge() - dt * fraction;
			}
			first.position = at;
			data.lastEmitted = at;
			first.baseScale = initialScale;
			first.ruleScale = 1.0f;
		}
		data.first = false;
		data.previousHead = head;
	}

	std::string creator;
	std::vector<int> nextGroups;
	float dieAge;
	float minEmitDist;
	std::string adjustInitialScale;
	bool inTestMode;
	/// the head atoms' AtomData (the original keeps it in the atom's modifier-data list +0x24)
	mutable std::unordered_map<const Atom*, AtomData> _data;
};

/// CreateRuleMakeChain::ModifyAtomCollection 0x69FD10 (a OnceOnlyCreateRule, NumAtoms 0x6B0B60): NumAtoms joints of
/// PCreator (the chain creator numbers them), once
class MakeChain final: public Modifier
{
public:
	explicit MakeChain(const Object& object)
	    : creator(object.String("PCreator"))
	    , numAtoms(object.Int("NumAtoms", 0)) // (inferido) default: the ctor was not read
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* joints = effect.FindCreator(creator);
		for (int i = 0; joints != nullptr && i < numAtoms; ++i)
		{
			effect.NewAtom(collection, joints, {});
		}
		return false; // once
	}
	std::string creator;
	int numAtoms;
};
} // namespace

void openblack::psys::RegisterGestureRules()
{
	RegisterModifier("UR_GesturingRecognised", MakeModifierOf<GesturingRecognised>);
	RegisterModifier("ZR_ChainGesture", MakeModifierOf<ChainGesture>);
	RegisterModifier("CreateRuleMakeChain", MakeModifierOf<MakeChain>);
}
