/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mist.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <memory>

#include "Common/GameRandom.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Graphics/Mists.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
std::unique_ptr<Creator> MakeMistCreator(const Object& object)
{
	auto creator = std::make_unique<MistCreator>();
	ReadCreatorProperties(object, *creator);
	creator->kind = Creator::Kind::Other; // drawn by mist_atoms::SubmitFrame, not by the sprite or mesh passes
	// ctor 0x6AA380: RandomiseScale 0, IsShadowMap 1, LoadLightMap 1, TakeRatioFromMatrix 0, Pitch 12, 1 frame in the
	// file and in use, InitialScaleMin 1.0, Ratio 0, TextureFileName ""
	creator->takeRatioFromMatrix = object.Bool("TakeRatioFromMatrix", false);
	creator->isShadowMap = object.Bool("IsShadowMap", true);
	creator->loadLightMap = object.Bool("LoadLightMap", true);
	creator->lightMap = object.String("TextureFileName");
	if (creator->lightMap == "NULL_STRING")
	{
		creator->lightMap.clear();
	}
	creator->pitch = std::clamp(object.Int("Pitch", 12), 1, 12);                    // range [1, 12]
	creator->numFramesInFile = std::clamp(object.Int("NumFramesInFile", 1), 1, 32); // range [1, 32]
	creator->numFramesInUse = std::clamp(object.Int("NumFramesInUse", 1), 1, 32);
	creator->initialScaleMin = object.Float("InitialScaleMin", 1.0f);
	creator->ratio = object.Float("Ratio", 0.0f);
	// GetBitmap 0x6AA540 (0x6AA54B..0x6AA586): only with LoadLightMap (+0x7A); bpp 1 with IsShadowMap (+0x79), else 3.
	// (inferido) the file's Pitch as written, not the [1, 12] property range: SF_LightningStormPush's S_SMClouds16 is
	// Pitch 16 (256 bytes, 16 x 16 x 1)
	if (creator->loadLightMap && !creator->lightMap.empty())
	{
		creator->landBitmap = land_light::LoadBitmapFile(creator->lightMap, std::max(1, object.Int("Pitch", 12)),
		                                                 creator->isShadowMap ? 1 : 3, creator->numFramesInFile,
		                                                 creator->numFramesInUse);
	}
	return creator;
}
} // namespace

void MistCreator::InitAtom(Effect& effect, Atom& atom) const
{
	// CreateParticleMist 0x6AA610, after fn_006A85E0 (0x6AA61D, NewAtom's common part): CreateLH3DMist 0x6AA5A0
	// (0x6AA649) first. LH3DObject::Create(7) 0x6AA5A9 runs the LH3DMist ctor 0x7F9560, which starts +0x84 at
	// ftol(Random(0, 16)) & 15 (0x7F95DC..0x7F95FB) on the CRT rand (game_random::crt), not the PSys stream
	atom.mist = {graphics::frame_anim::MistStartCounter(game_random::crt::Random(0.0f, 16.0f)), 0.0f};
	// 0x6AA5C0..0x6AA5EE: k (+0x8C) = Ratio (+0x74), or LocalFloatRand(2.5) + 2.5 ([0x8C581C]) when it is 0 (or NaN:
	// fcomp 0; test ah,0x40)
	if (ratio == 0.0f || std::isnan(ratio))
	{
		const float r = game_random::LocalFloatRand(2.5f);
		atom.mistK = r + 2.5f;
	}
	else
	{
		atom.mistK = ratio;
	}
	// then (0x6AA65E..0x6AA680): RandomiseScale (+0x78) ? PSysFloatRand(InitialScaleMin (+0x70), InitialScale (+0x30))
	// (0x6AA66D) : InitialScale -> atom +0x74 (it replaces fn_006A85E0's)
	atom.baseScale = randomiseScale ? effect.Random(initialScaleMin, initialScale) : initialScale;
	// 0x6AA683..0x6AA6AF: atom +0x110 = 1.0, +0x114 = 1, +0x118 PlayAnim = 0, +0x119 LoopAnim = the creator's +0x0C:
	// one frame that never steps
	atom.frame = 0.0f;
	atom.frameRate = 1.0f;
	atom.playAnim = false;
}

uint32_t mist_atoms::MistColour(uint32_t atomArgb, uint32_t baseArgb)
{
	// 0x67A6C1..0x67A741: the base's alpha byte is set to 0xFF ([esp+0x17] = 0xFF), then each channel is (c x g) >> 8
	const auto channel = [&](int shift, uint32_t g) { return ((((atomArgb >> shift) & 0xFFu) * g) >> 8) << shift; };
	return channel(24, 0xFFu) | channel(16, (baseArgb >> 16) & 0xFFu) | channel(8, (baseArgb >> 8) & 0xFFu) |
	       channel(0, baseArgb & 0xFFu);
}

bool mist_atoms::Describe(const Effect::DrawAtom& atom, mists::MistDesc& mist)
{
	const auto* creator = dynamic_cast<const MistCreator*>(atom.creator);
	if (creator == nullptr)
	{
		return false;
	}
	mist = {};
	mist.position = atom.position; // vt 0x24 SetPos(PSR +0x24)
	mist.size = atom.scale;        // mist +0x88 = PSR +0x30
	// CreateLH3DMist 0x6AA5A0: +0x80 |= 2 (the effect branch, 0x6AA5F4) and the k it drew for this mist (+0x8C)
	mist.edgeShrink = true;
	mist.k = atom.atom != nullptr ? atom.atom->mistK : creator->ratio;
	// DrawAt: TakeRatioFromMatrix -> k = M[1][1] / M[0][0] when |M[0][0]| > 0.0001 (0x8BF518)
	// (the PSR matrix's Y axis carries the stretch, fn_00673DB0: the storm clouds' cloud ratio, UR_CloudGather)
	const glm::mat3 matrix = atom.rotation * glm::mat3(atom.scale, 0.0f, 0.0f, 0.0f, atom.scale * atom.stretch, 0.0f, 0.0f,
	                                                   0.0f, atom.scale);
	if (creator->takeRatioFromMatrix && std::abs(matrix[0][0]) > 0.0001f)
	{
		mist.k = matrix[1][1] / matrix[0][0];
	}
	// DrawData colour (the atom's, its alpha with the collection's) x [0xFA26A4], the land light table's base
	// colour (the renderer's last Build, LandLightTable::Current().GetRawBase(): (aproximado) the previous frame's)
	const auto alpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
	const uint32_t argb = (alpha << 24) | (static_cast<uint32_t>(atom.colour[0]) << 16) |
	                      (static_cast<uint32_t>(atom.colour[1]) << 8) | static_cast<uint32_t>(atom.colour[2]);
	mist.colour = MistColour(argb, LandLightTable::Current().GetRawBase());
	if (atom.atom != nullptr)
	{
		mist.counter = atom.atom->mist.counter;
	}
	// DrawData +0xC, the atom's +0x90 (0x679BF4), to SetColour 0x7F9770 as the specular (0x67A6C4/0x67A6D6)
	mist.specular = atom.specular;
	return true;
}

void mist_atoms::SubmitFrame(float milliseconds)
{
	for (const auto& drawable : manager::Collect(Creator::Kind::Other))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const MistCreator*>(atom.creator);
			if (creator == nullptr)
			{
				continue;
			}
			// fn_007FA300: every LH3DMist its own counter += ftol(g_game_time_inc x 0.255), modulo 900 once past it, run
			// only for a mist on screen (AddDrawing 0x7FA7F0, mists::InView); the fraction kept as the map mists do
			// (frame_anim::MistAdvance)
			if (atom.atom != nullptr && mists::InView(atom.position, atom.scale))
			{
				graphics::frame_anim::MistAdvance(atom.atom->mist, milliseconds);
			}
			mists::MistDesc mist {};
			Describe(atom, mist);
			// RenderParticleMist::DrawAt 0x67A774: with [0xC0215D] set (the effect drawn with Draw_(t, 1), DrawPath::Sorted)
			// vt 0x100 = fn_007FA7F0, the mist's own Z object at mist +0x38 (NewZObject 0x7FA87B): mists::Submit; else
			// vt 0x104 = fn_007FA790 (0x67A78C), drawn at once inside its effect's draw (manager::CollectQueued /
			// HandEffects, mist_atoms::Describe). Until the renderer draws by path (manager::k_DrawByPath) every mist
			// still goes to mists::Submit
			if (!manager::k_DrawByPath || drawable.path == DrawPath::Sorted)
			{
				mists::Submit(mist);
			}
			// 0x67A792..0x67A8C1: with the creator's bitmap a record in the list 0xD4EDB8 at (x, 0, z), frame 0, alpha =
			// DrawData alpha / 255 ([0x9357AC]) clamped to [0, 1]; PSysLightMaps fn_006CA280: + (10, 0, 10), centred,
			// mode 1 for bpp 3 / 2 for bpp 1 (the storm's shadow: fn_00878C70, land_light::AddStamp)
			if (creator->landBitmap)
			{
				const auto alpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
				const int mode = creator->landBitmap->channels == 3 ? 1 : 2;
				land_light::AddStamp(glm::vec3(mist.position.x + 10.0f, 0.0f, mist.position.z + 10.0f),
				                     graphics::frame_anim::FrameTexels(*creator->landBitmap, 0), creator->landBitmap->pitch, true,
				                     static_cast<float>(alpha) * 0.00392157f, mode);
			}
		}
	}
}

void openblack::psys::RegisterMistCreator()
{
	RegisterCreator("ParticleMistCreator", MakeMistCreator);
}
