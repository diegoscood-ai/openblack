/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Semver.h"

#include <charconv>

#include <fmt/format.h>

namespace openblack::mods
{

std::optional<Version> Version::Parse(std::string_view text)
{
	// "-pre" and "+build" are allowed and do not count
	if (const auto cut = text.find_first_of("-+"); cut != std::string_view::npos)
	{
		text = text.substr(0, cut);
	}
	if (text.empty())
	{
		return std::nullopt;
	}
	Version version;
	uint32_t* parts[] = {&version.major, &version.minor, &version.patch};
	size_t index = 0;
	const char* it = text.data();
	const char* end = text.data() + text.size();
	while (it < end)
	{
		if (index >= 3)
		{
			return std::nullopt;
		}
		const auto [next, error] = std::from_chars(it, end, *parts[index]);
		if (error != std::errc() || next == it)
		{
			return std::nullopt;
		}
		++index;
		it = next;
		if (it < end)
		{
			if (*it != '.' || it + 1 == end)
			{
				return std::nullopt;
			}
			++it;
		}
	}
	return version;
}

std::string Version::ToString() const
{
	return fmt::format("{}.{}.{}", major, minor, patch);
}

std::optional<VersionRange> VersionRange::Parse(std::string_view text)
{
	VersionRange range;
	range._text = std::string(text);
	size_t start = 0;
	while (start < text.size())
	{
		const auto space = text.find(' ', start);
		auto token = text.substr(start, space == std::string_view::npos ? std::string_view::npos : space - start);
		start = space == std::string_view::npos ? text.size() : space + 1;
		if (token.empty() || token == "*")
		{
			continue;
		}

		auto take = [&token](std::string_view prefix) {
			if (token.starts_with(prefix))
			{
				token.remove_prefix(prefix.size());
				return true;
			}
			return false;
		};
		enum class Kind : uint8_t
		{
			Plain,
			Caret,
			Tilde
		};
		Kind kind = Kind::Plain;
		Op op = Op::Equal;
		if (take(">="))
		{
			op = Op::GreaterEqual;
		}
		else if (take("<="))
		{
			op = Op::LessEqual;
		}
		else if (take(">"))
		{
			op = Op::Greater;
		}
		else if (take("<"))
		{
			op = Op::Less;
		}
		else if (take("="))
		{
			op = Op::Equal;
		}
		else if (take("^"))
		{
			kind = Kind::Caret;
		}
		else if (take("~"))
		{
			kind = Kind::Tilde;
		}

		const auto version = Version::Parse(token);
		if (!version)
		{
			return std::nullopt;
		}
		switch (kind)
		{
		case Kind::Plain:
			range._conditions.push_back({op, *version});
			break;
		case Kind::Caret: // ^1.2.3 -> >=1.2.3 <2.0.0 (^0.x keeps the minor, as npm/Cargo do)
			range._conditions.push_back({Op::GreaterEqual, *version});
			range._conditions.push_back({Op::Less, version->major > 0 ? Version {version->major + 1, 0, 0}
			                                                           : Version {0, version->minor + 1, 0}});
			break;
		case Kind::Tilde: // ~1.2.3 -> >=1.2.3 <1.3.0
			range._conditions.push_back({Op::GreaterEqual, *version});
			range._conditions.push_back({Op::Less, Version {version->major, version->minor + 1, 0}});
			break;
		}
	}
	return range;
}

bool VersionRange::Contains(const Version& version) const
{
	for (const auto& [op, bound] : _conditions)
	{
		bool ok = false;
		switch (op)
		{
		case Op::Equal:
			ok = version == bound;
			break;
		case Op::Greater:
			ok = version > bound;
			break;
		case Op::GreaterEqual:
			ok = version >= bound;
			break;
		case Op::Less:
			ok = version < bound;
			break;
		case Op::LessEqual:
			ok = version <= bound;
			break;
		}
		if (!ok)
		{
			return false;
		}
	}
	return true;
}

} // namespace openblack::mods
