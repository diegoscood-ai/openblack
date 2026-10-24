/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureIconSeeds.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Creature/LocalPlayer.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "GestureIconModels.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/ParticleManager.h"
#include "Particles/ParticleTypes.h"
#include "Particles/Rules/SurfRevol.h"
#include "Particles/SpellLink.h"
#include "Resources/SharedAssets.h"
#include "Worship/SpellSeedGraphic.h"

namespace openblack::magic::gestures
{
namespace
{
// The game's FPU rounds every step to a float: each one is its own statement here, so that none is fused. A sine it
// keeps wider than a float until it is used is kept in double
constexpr float k_MsToSeconds = 0.001f;
constexpr float k_TwoPi = 6.2831854820251465f;
constexpr float k_SpinRate = 2.0f;
constexpr float k_FreezePulseRate = 0.35f;
constexpr float k_PulseRate = 0.5f;
constexpr float k_FatPulse = 1.5f;
constexpr float k_ThinPulseRow0 = 0.8f;
constexpr float k_ThinPulseRow2 = 0.7f;
constexpr float k_MaxAlpha = 255.0f;
/// The small phial's second draw: its alpha times 80 / 256
constexpr uint32_t k_SmallShadowAlpha = 5;
constexpr float k_BandAngleRate = 10.3f;
constexpr float k_BandSpinRate = 1.0f;
constexpr float k_BandSize = 0.2f;
/// The object's draw leaves a model out beyond this many times its half diagonal as it is scaled
constexpr float k_FarCut = 300.0f;
/// Where an icon's graphic is made: the map's corner, at a scale of 1
constexpr glm::vec3 k_MadeAt {0.0f, 0.0f, 0.0f};
constexpr float k_MadeAtScale = 1.0f;

/// The graphics (the miracles' service's hand magic state), kept until the map is cleared
struct IconSeedState
{
	IconSeedGraphics graphics;
};

IconSeedState& SeedState()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("magic::gestures: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value().HandMagic().Get<IconSeedState>();
}

/// A colour's rgb with a new alpha byte
uint32_t WithAlpha(uint32_t colour, uint32_t alpha)
{
	return (colour & 0xFFFFFFu) | ((alpha & 0xFFu) << 24u);
}

/// Every cell of the matrix's columns `first` to `last` times k
void ScaleColumns(glm::mat4& matrix, int first, int last, float k)
{
	for (int c = first; c <= last; ++c)
	{
		for (int j = 0; j < 3; ++j)
		{
			matrix[c][j] = k * matrix[c][j];
		}
	}
}

/// An angle turned on by rate x seconds, its turn kept within 2 pi
float TurnedOn(float angle, float rate, float seconds)
{
	float turned = rate * seconds;
	turned = turned + angle;
	return std::fmod(turned, k_TwoPi);
}

/// The land's height under a point, as its map position finds it: none without a land
float GroundUnder(glm::vec3 point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	const glm::vec2 cell(map_coords::ToMetres(map_coords::ToFixed(point.x)),
	                     map_coords::ToMetres(map_coords::ToFixed(point.z)));
	return Locator::terrainSystem::value().GetHeightAt(cell);
}

/// The seed's row of the tables as its graphic draws it
IconSeedLook LookOf(const IconSeedGraphic& graphic)
{
	const auto& tables = Locator::infoConstants::value();
	const auto& info = GetSpellSeedInfo(tables, graphic.seed);
	IconSeedLook look {
	    .drawScale = info.scale,
	    .meshHeight = info.meshHeight,
	    .holderHeight = info.holderHeight,
	    .useMesh = info.useMesh != 0,
	};
	// a creature spell phial: the seed's spell (the magic of its level -1) is a creature spell
	const auto magicType = info.magicTypes[0];
	if (static_cast<size_t>(magicType) < k_MagicTypeCount)
	{
		if (const auto* spell = GetMagicInfoAs<GMagicCreatureSpellInfo>(tables, magicType); spell != nullptr)
		{
			look.phial = spell->creatureReceiveSpellType;
		}
	}
	look.meshHalfDiagonal = ecs::object::MeshHalfDiagonal(graphic.mesh);
	if (graphic.powerUp != -1 &&
	    resources::shared_assets::LoadPowerUpBand("Gesture icons") != resources::shared_assets::LoadResult::Failed)
	{
		look.bandHalfDiagonal = ecs::object::MeshHalfDiagonal(resources::shared_assets::k_PowerUpBandMesh.value());
	}
	return look;
}

/// A new graphic: the local player's, its holder effect started where it is made, stepped by its draws only and never
/// in the world's draw
IconSeedGraphic MakeGraphic(SpellSeedType seed, int powerUp)
{
	IconSeedGraphic graphic {.seed = seed, .powerUp = powerUp, .player = creature::LocalPlayer()};
	graphic.mesh = worship::seed_graphic::MeshIdOf(seed, graphic.player);
	const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	if (info.holderParticle == ParticleType::None || !Locator::particleSystem::has_value())
	{
		return graphic;
	}
	const auto file = particles::ParticleTypeFile(info.holderParticle);
	if (file.empty())
	{
		return graphic;
	}
	const auto made = IconSeedPlacesAt(k_MadeAt, k_MadeAtScale, GroundUnder(k_MadeAt), info.meshHeight, info.holderHeight);
	auto& particles = Locator::particleSystem::value();
	graphic.effect = particles.StartForSpell(file, made.effect, glm::vec3(0.0f), k_MadeAtScale, nullptr, false);
	if (graphic.effect != ecs::systems::ParticleSystemInterface::k_NoEffect)
	{
		particles.SetPlayer(graphic.effect, static_cast<int>(graphic.player));
		particles.SetDrawn(graphic.effect, false);
		particles::manager::SetPerFrame(graphic.effect);
	}
	return graphic;
}
} // namespace

IconSeedPlaces IconSeedPlacesAt(glm::vec3 point, float scale, float ground, float meshHeight, float holderHeight)
{
	// the map position: x and z in fixed point, the height above the ground
	const float x = map_coords::ToMetres(map_coords::ToFixed(point.x));
	const float z = map_coords::ToMetres(map_coords::ToFixed(point.z));
	const float above = point.y - ground;
	IconSeedPlaces places;
	places.point = {x, ground + above, z};
	float meshAbove = meshHeight * scale;
	meshAbove = meshAbove + above;
	places.mesh = {x, ground + meshAbove, z};
	const float effectHeight = holderHeight * scale;
	places.effect = {x, effectHeight + places.point.y, z};
	return places;
}

bool IconModelInReach(glm::vec3 position, float halfDiagonal, float scale)
{
	// the squared distance from the camera: (dx dx + dy dy) + dz dz
	const glm::vec3 d = position - k_IconModelEye;
	const float xx = d.x * d.x;
	const float yy = d.y * d.y;
	float distanceSquared = xx + yy;
	const float zz = d.z * d.z;
	distanceSquared = distanceSquared + zz;
	float reach = halfDiagonal * scale;
	reach = reach * k_FarCut;
	// drawn unless short of it (a reach that is not a number is short of it too)
	const float reachSquared = reach * reach;
	return reachSquared >= distanceSquared;
}

IconSeedDraw StepIconSeed(IconSeedGraphic& graphic, const IconSeedLook& look, const IconSeedPlaces& places, uint32_t frameMs,
                          uint8_t alpha, uint32_t playerColour)
{
	IconSeedDraw draw {.effectOrigin = places.effect, .effectMagnitude = k_IconSeedScale, .effectAlpha = alpha};
	const float seconds = static_cast<float>(frameMs) * k_MsToSeconds;
	const bool fadingAlpha = alpha != 0xFF;
	if (look.useMesh)
	{
		const float scale = look.drawScale * k_IconSeedScale;
		float spin = k_SpinRate * seconds;
		spin = spin + graphic.spin;
		spin = std::fmod(spin, k_TwoPi);
		if (spin < 0.0f)
		{
			spin = spin + k_TwoPi;
		}
		graphic.spin = spin;
		const auto meshDraw = [&](const glm::mat4& matrix, float objectScale, bool fading) {
			IconSeedMeshDraw mesh {
			    .matrix = matrix, .scale = objectScale, .colour = graphic.colour, .specular = 0u, .fading = fading};
			mesh.inReach = IconModelInReach(glm::vec3(matrix[3]), look.meshHalfDiagonal, objectScale);
			return mesh;
		};
		if (!look.phial.has_value())
		{
			// a player's seed: its colour's alpha is the icons', drawn with it unless that is opaque
			graphic.colour = WithAlpha(graphic.colour, alpha);
			draw.meshes.push_back(meshDraw(affine::PlacementMatrix(places.mesh, spin, scale), scale, fadingAlpha));
		}
		else
		{
			const auto receive = *look.phial;
			// its frames, 8 by 4 of its sheet, and its pulse, 0.35 a second frozen, else 0.5
			const glm::vec2 uv = graphics::frame_anim::SpellIconFrame(graphic.uvFrame, seconds);
			float phase =
			    (receive == CreatureReceiveSpellType::CreatureReceiveSpellFreeze ? k_FreezePulseRate : k_PulseRate) * seconds;
			phase = phase + graphic.pulsePhase;
			phase = std::fmod(phase, 1.0f);
			graphic.pulsePhase = phase;
			const float pulseAngle = phase * k_TwoPi;
			float pulse = static_cast<float>(std::sin(static_cast<double>(pulseAngle)) + 1.0);
			pulse = pulse * 0.5f;
			auto matrix = affine::PlacementMatrix(places.mesh, spin, scale);
			if (receive == CreatureReceiveSpellType::CreatureReceiveSpellFat)
			{
				float k = pulse * k_FatPulse;
				k = k + 1.0f;
				ScaleColumns(matrix, 0, 0, k);
				ScaleColumns(matrix, 2, 2, k);
			}
			else if (receive == CreatureReceiveSpellType::CreatureReceiveSpellThin)
			{
				float row2 = pulse * k_ThinPulseRow2;
				row2 = 1.0f - row2;
				float row0 = pulse * k_ThinPulseRow0;
				row0 = 1.0f - row0;
				ScaleColumns(matrix, 0, 0, row0);
				ScaleColumns(matrix, 2, 2, row2);
			}
			switch (receive)
			{
			case CreatureReceiveSpellType::CreatureReceiveSpellFreeze:
				// the land's light, then its frozen look with the pulse instead of a draw (not drawn: Pending)
				draw.frozen = true;
				break;
			case CreatureReceiveSpellType::CreatureReceiveSpellBig:
			{
				// drawn with the icons' alpha, then again growing with the pulse's phase and fading as it grows
				graphic.colour = WithAlpha(graphic.colour, alpha);
				draw.meshes.push_back(meshDraw(matrix, scale, fadingAlpha));
				float fade = -k_MaxAlpha * phase;
				fade = fade + k_MaxAlpha;
				const auto fadeByte = static_cast<uint32_t>(static_cast<int32_t>(fade)) & 0xFFu;
				graphic.colour = (graphic.colour & 0xFFFFFFu) | (((fadeByte * alpha) & 0xFFFFFF00u) << 16u);
				const float grow = phase + 1.0f;
				ScaleColumns(matrix, 0, 2, grow);
				float grownScale = pulse + 1.0f;
				grownScale = grownScale * scale;
				draw.meshes.push_back(meshDraw(matrix, grownScale, true));
				break;
			}
			case CreatureReceiveSpellType::CreatureReceiveSpellSmall:
			{
				// drawn shrinking with the pulse's phase, then at its size with less of the icons' alpha
				graphic.colour = WithAlpha(graphic.colour, alpha);
				float small = 1.0f - phase;
				small = small * scale;
				draw.meshes.push_back(meshDraw(affine::PlacementMatrix(places.mesh, spin, small), small, fadingAlpha));
				graphic.colour =
				    (graphic.colour & 0xFFFFFFu) | ((static_cast<uint32_t>(alpha) * k_SmallShadowAlpha << 20u) & 0xFF000000u);
				draw.meshes.push_back(meshDraw(affine::PlacementMatrix(places.mesh, spin, scale), scale, true));
				break;
			}
			case CreatureReceiveSpellType::CreatureReceiveSpellInvisible:
			{
				// the land's light where it stands, its alpha the icons' times the pulse
				const float faded = static_cast<float>(alpha) * pulse;
				graphic.colour = WithAlpha(graphic.colour, static_cast<uint32_t>(static_cast<int32_t>(faded)));
				auto mesh = meshDraw(matrix, scale, true);
				mesh.landLight = true;
				draw.meshes.push_back(mesh);
				break;
			}
			default:
				// its colour as it is, drawn with its own alpha unless the icons' is opaque
				draw.meshes.push_back(meshDraw(matrix, scale, fadingAlpha));
				break;
			}
			for (auto& mesh : draw.meshes)
			{
				mesh.uv = uv;
			}
		}
	}
	// the bands, when it was made with a power-up: their angles turn only then
	if (graphic.powerUp != -1)
	{
		graphic.bandAngle = TurnedOn(graphic.bandAngle, k_BandAngleRate, seconds);
		graphic.bandSpin = TurnedOn(graphic.bandSpin, k_BandSpinRate, seconds);
		const uint32_t bandColour = (playerColour & 0xFFFFFFu) | (((k_IconSeedBandAlpha * alpha) & 0xFFFFFF00u) << 16u);
		float size = k_BandSize * k_IconSeedBandScale;
		size = size * k_IconSeedScale;
		const auto toEye = graphics::billboard::BandToEye(places.point, k_IconModelEye);
		const bool inReach = IconModelInReach(places.point, look.bandHalfDiagonal, size);
		for (int level = 0; level <= graphic.powerUp; ++level)
		{
			const auto rotation = toEye * worship::seed_graphic::BandRotation(graphic.bandAngle, static_cast<size_t>(level));
			const IconSeedBandDraw band {.matrix = affine::Model(places.point, rotation, glm::vec3(size)),
			                             .scale = size,
			                             .colour = bandColour,
			                             .inReach = inReach};
			// each level drawn twice
			draw.bands.push_back(band);
			draw.bands.push_back(band);
		}
	}
	return draw;
}

std::optional<IconSeedFrame> DrawIconSeed(const GestureIcon& icon, uint8_t alpha, uint32_t frameMs)
{
	const int seedIndex = IconModelSeed(icon);
	// (openblack guard) the tables' seeds and power-up levels -1 to 2
	if (seedIndex < 0 || static_cast<size_t>(seedIndex) >= k_SpellSeedCount || icon.powerUp < -1 || icon.powerUp > 2 ||
	    !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto seed = static_cast<SpellSeedType>(seedIndex);
	const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	// one graphic for each magic type: that of the seed at the icon's power-up level
	const auto magic = static_cast<size_t>(MagicTypeForPowerUpLevel(info, icon.powerUp));
	if (magic >= k_MagicTypeCount)
	{
		return std::nullopt;
	}
	auto& slot = SeedState().graphics.at(magic);
	if (!slot.has_value())
	{
		slot = MakeGraphic(seed, icon.powerUp);
	}
	auto& graphic = *slot;
	IconSeedFrame frame;
	frame.mesh = graphic.mesh;
	// moved to the models' camera's focus at the icons' scale, its holder effect there stepped by the frame
	const auto look = LookOf(graphic);
	const auto places =
	    IconSeedPlacesAt(k_IconModelFocus, k_IconSeedScale, GroundUnder(k_IconModelFocus), look.meshHeight, look.holderHeight);
	particles::Effect* effect = nullptr;
	if (graphic.effect != 0 && Locator::particleSystem::has_value())
	{
		auto& particles = Locator::particleSystem::value();
		if (auto* found = particles.Find(graphic.effect); found != nullptr)
		{
			found->SetOrigin(places.effect);
			found->SetMagnitude(k_IconSeedScale);
			const particles::ProcessInfo step {.power = 1.0f, .enabled = true};
			particles.ProcessForSpell(graphic.effect, step, static_cast<float>(frameMs) * k_MsToSeconds);
			effect = particles.Find(graphic.effect);
		}
		if (effect == nullptr)
		{
			graphic.effect = 0;
		}
	}
	frame.draw = StepIconSeed(graphic, look, places, frameMs, alpha,
	                          particles::surf_revol::PlayerColour(static_cast<int>(graphic.player)));
	// the holder effect drawn now with the icons' alpha, as its step left it
	if (effect != nullptr)
	{
		effect->SetGlobalAlpha(static_cast<float>(alpha));
		effect->Collect(1.0f, frame.sprites, particles::Creator::Kind::Sprite);
	}
	return frame;
}

void RefreshIconSeedMeshes()
{
	if (!Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	for (auto& slot : SeedState().graphics)
	{
		if (slot.has_value())
		{
			slot->mesh = worship::seed_graphic::MeshIdOf(slot->seed, slot->player);
		}
	}
}

void ResetIconSeeds()
{
	for (auto& slot : SeedState().graphics)
	{
		if (slot.has_value() && slot->effect != 0 && Locator::particleSystem::has_value() &&
		    Locator::particleSystem::value().IsRunning(slot->effect))
		{
			Locator::particleSystem::value().Delete(slot->effect);
		}
		slot.reset();
	}
}

const IconSeedGraphics& IconSeeds()
{
	return SeedState().graphics;
}
} // namespace openblack::magic::gestures
