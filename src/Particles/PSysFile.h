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
#include <optional>
#include <string>
#include <string_view>

#include <ParticleFile.h>

// The spell files (Data\Spells\ZSpellFiles\SF_*): the format and its parser are the psys component's
// (components/psys/ParticleFile.h). What stays here is what the game adds: the file's name, for the logs, and loading
// it through the resource caches.

namespace openblack::psys
{

/// One class instance of a spell file and its properties, the component's type under the name the effects use
using Object = ParticleObject;

/// A parsed spell file and the name it was loaded by
struct File: ParticleFile
{
	std::string name;

	/// Parses the text of a spell file; nullopt when it isn't one
	[[nodiscard]] static std::optional<File> Parse(std::string_view text, std::string name);
	/// Data\Spells\ZSpellFiles\<name>.txt if it exists (the original reads a loose .txt first),
	/// else <name>_txt.zzz (u32 size, then zlib)
	static std::shared_ptr<const File> Load(const std::string& name);
};

} // namespace openblack::psys
