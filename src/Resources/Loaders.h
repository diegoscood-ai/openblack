/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <queue>

#include <PackFile.h>

#include "3D/L3DAnim.h"
#include "3D/L3DSubMesh.h"
#include "3D/Light.h"
#include "Audio/Sound.h"
#include "Creature/CreatureMind.h"
#include "Level.h"

namespace openblack::graphics
{
class L3DMesh;
class Texture2D;
} // namespace openblack::graphics

namespace openblack::pack
{
struct AudioBankSampleHeader;
struct G3DTexture;
} // namespace openblack::pack

namespace openblack::resources
{

template <typename Resource>
struct BaseLoader
{
	using result_type = std::shared_ptr<Resource>;
	using ResourceType = Resource;
	struct FromBufferTag
	{
	};
	struct FromDiskTag
	{
	};
};

struct L3DLoader final: BaseLoader<graphics::L3DMesh>
{
	struct FromGeneratedTag
	{
	};

	[[nodiscard]] result_type operator()(FromGeneratedTag, const std::string& debugName,
	                                     const std::vector<graphics::L3DSubMesh::GeneratedPrimitive>& primitives) const;
	[[nodiscard]] result_type operator()(FromBufferTag, const std::string& debugName, const std::vector<uint8_t>& data) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

struct Texture2DLoader final: BaseLoader<graphics::Texture2D>
{
	struct FromPackTag
	{
	};
	/// A PNG image (mod graphics.hd-people), RGBA8 with a full mip chain
	struct FromImageTag
	{
	};

	[[nodiscard]] result_type operator()(FromPackTag, const std::string& name, const pack::G3DTexture& g3dTexture) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& rawTexturePath) const;
	[[nodiscard]] result_type operator()(FromImageTag, const std::string& name, const std::filesystem::path& imagePath) const;
};

struct L3DAnimLoader final: BaseLoader<L3DAnim>
{
	[[nodiscard]] result_type operator()(FromBufferTag, const std::vector<uint8_t>& data) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

struct LevelLoader final: BaseLoader<Level>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path, Level::LandType landType) const;
};

struct CreatureMindLoader final: BaseLoader<creature::CreatureMind>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& creatureMindPath) const;
};

struct SoundLoader final: BaseLoader<audio::Sound>
{
	[[nodiscard]] result_type operator()(FromBufferTag, const pack::AudioBankSampleHeader& header,
	                                     const std::vector<std::vector<uint8_t>>& buffer) const;
};

struct LightLoader final: BaseLoader<Lights>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};
} // namespace openblack::resources
