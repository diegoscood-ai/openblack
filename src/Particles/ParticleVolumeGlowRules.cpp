/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_VolFXOnObject: the glowing shell laid over the models of the objects its effect is given, as the magic over a
// worthy artefact. Each object given becomes an atom kept on it; every step each atom's shell is set again: it reaches
// further out the stronger the effect is, in its player's colour when the rule takes one, fading with its collection.
// An atom whose object has gone goes with it. The shell is drawn by particles::draw::AddShells and the renderer. Wiki:
// docs/wiki/physics/particle-rules.md#the-glowing-shell-over-an-object.

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/GlintTargetsInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/SurfRevol.h"
#include "Particles/VolumeGlow.h"

using namespace openblack;
using namespace openblack::particles;
namespace volume_glow = openblack::particles::volume_glow;

namespace
{
/// Where an object stands while it is there, as the glints on a target read it (Locator::glintTargets); none without
/// them
std::optional<glm::vec3> ObjectPosition(entt::entity object)
{
	if (object == entt::null || !Locator::glintTargets::has_value())
	{
		return std::nullopt;
	}
	return Locator::glintTargets::value().ObjectPosition(object);
}

/// The game's milliseconds now, by the turn and its fraction; 0 without a clock
uint64_t GameMilliseconds()
{
	if (!Locator::time::has_value())
	{
		return 0;
	}
	const auto& time = Locator::time::value();
	return volume_glow::GameMilliseconds(time.GetTurn(), time.GetTurnFraction());
}

/// An atom's data: the object it is kept on
struct GlowData
{
	entt::entity object {entt::null};
};

/// UR_VolFXOnObject. A creator
class VolumeGlowOnObject final: public Modifier
{
public:
	explicit VolumeGlowOnObject(const Object& object)
	    : _creator(object.String("PCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	{
		const volume_glow::Settings defaults;
		_settings = {
		    .texture =
		        std::clamp(object.Int("MatInd", defaults.texture), 0, static_cast<int>(volume_glow::k_Textures.size()) - 1),
		    .animSpeed = {object.Float("AnimSpeedU", defaults.animSpeed.x), object.Float("AnimSpeedV", defaults.animSpeed.y)},
		    .zoomMin = object.Float("ZoomProportionMin", defaults.zoomMin),
		    .zoomMax = object.Float("ZoomProportionMax", defaults.zoomMax),
		    .textureFraction = {object.Float("TextFracU", defaults.textureFraction.x),
		                        object.Float("TextFracV", defaults.textureFraction.y)},
		    .alpha = object.Int("Alpha", defaults.alpha),
		    .usePlayerColour = object.Bool("UsePlayerColor", defaults.usePlayerColour),
		    .alphaFromStrength = object.Bool("SetAlphaFromStrength", defaults.alphaFromStrength),
		};
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* creator = effect.FindCreator(_creator);
		// An atom on each object given; anything given that isn't an object in the world ends the rule's step
		while (!effect.GetTargets().empty())
		{
			const auto target = effect.TakeTarget();
			if (target == entt::null || !ObjectPosition(target).has_value() || creator == nullptr)
			{
				return true;
			}
			auto& atom = effect.NewAtom(collection, creator, _nextGroups);
			AtomDataOf<GlowData>(atom, this).object = target;
			// Its texture slides from now, by the game's time
			atom.objectGlow = volume_glow::ObjectGlow {.object = target, .startedAt = GameMilliseconds()};
		}

		// The effect's strength is its magnitude: a glowing artefact's is its worth
		const float power = effect.GetMagnitude();
		const int player = effect.GetPlayer();
		const auto playerRgb = player >= 0 ? std::optional<uint32_t>(surf_revol::PlayerColour(player)) : std::nullopt;
		const auto alpha = volume_glow::Alpha(_settings, power);
		const auto collectionAlpha = static_cast<uint8_t>(std::clamp(collection.alpha, 0.0f, 255.0f));
		const auto argb = volume_glow::Argb(_settings, alpha, collectionAlpha, playerRgb);
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const auto object = AtomDataOf<GlowData>(atom, this).object;
			const auto standing = ObjectPosition(object);
			// An atom whose object has gone (or that the rule did not make) goes with it
			if (!standing.has_value())
			{
				effect.NotifyAtomRemoved(atom);
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			// It stands where its object does, which places the shell among what blends; no file of the rule has a
			// hierarchy, so the point is in the world
			atom.position = *standing;
			const auto startedAt = atom.objectGlow.has_value() ? atom.objectGlow->startedAt : GameMilliseconds();
			atom.objectGlow = volume_glow::ObjectGlow {
			    .object = object,
			    .zoom = volume_glow::Zoom(_settings, power, atom.baseScale * atom.ruleScale),
			    .argb = argb,
			    .texture = _settings.texture,
			    .animSpeed = _settings.animSpeed,
			    .textureFraction = _settings.textureFraction,
			    .startedAt = startedAt,
			};
			++i;
		}
		return true;
	}

private:
	std::string _creator;
	std::vector<int> _nextGroups;
	volume_glow::Settings _settings;
};
} // namespace

void openblack::particles::RegisterVolumeGlowRules()
{
	RegisterModifier("UR_VolFXOnObject", MakeModifierOf<VolumeGlowOnObject>);
}
