/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::mods
{

/// A mod's version, "major.minor.patch" (missing parts are 0: "1.2" is 1.2.0). Pre-release and build suffixes
/// ("-beta", "+abc") are accepted and ignored when comparing
struct Version
{
	uint32_t major {0};
	uint32_t minor {0};
	uint32_t patch {0};

	[[nodiscard]] static std::optional<Version> Parse(std::string_view text);
	[[nodiscard]] std::string ToString() const;

	auto operator<=>(const Version&) const = default;
};

/// What a dependency accepts: one or more space-separated conditions that must all hold, e.g. ">=1.2 <2.0".
/// Each condition is "*" (anything), "1.2.3" or "=1.2.3" (exactly), ">", ">=", "<", "<=", "^1.2" (same major, at least
/// 1.2) or "~1.2" (same major and minor, at least 1.2)
class VersionRange
{
public:
	/// nullopt if the text is not a valid range
	[[nodiscard]] static std::optional<VersionRange> Parse(std::string_view text);

	[[nodiscard]] bool Contains(const Version& version) const;
	[[nodiscard]] const std::string& GetText() const noexcept { return _text; }

private:
	enum class Op : uint8_t
	{
		Equal,
		Greater,
		GreaterEqual,
		Less,
		LessEqual,
	};
	struct Condition
	{
		Op op;
		Version version;
	};

	std::vector<Condition> _conditions; // all must hold; none = anything
	std::string _text;
};

} // namespace openblack::mods
