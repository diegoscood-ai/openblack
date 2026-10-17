/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Materials.h"

#include <algorithm>
#include <sstream>
#include <string>

using namespace openblack::physics;

MaterialsFile openblack::physics::ParseMaterials(std::span<const uint8_t> bytes, const MaterialTable& base)
{
	MaterialsFile file {.table = base};
	std::istringstream in(std::string(bytes.begin(), bytes.end()));
	in >> file.version >> file.rows;
	// every column clamped to its own range
	constexpr std::array<float, 6> k_Min = {0.05f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f};
	constexpr std::array<float, 6> k_Max = {3.0f, 240.0f, 10.0f, 3.0f, 1.0f, 4.0f};
	for (size_t i = 0; i < k_MaterialRows; ++i)
	{
		if (static_cast<int>(i) >= file.rows)
		{
			file.table.at(i) = file.table.at(0); // missing rows are copies of row 0
			continue;
		}
		std::array<float, 6> v {};
		for (auto& f : v)
		{
			in >> f;
		}
		for (size_t c = 0; c < v.size(); ++c)
		{
			v.at(c) = std::clamp(v.at(c), k_Min.at(c), k_Max.at(c));
		}
		file.table.at(i) = {v[0], v[1], v[2], v[3], v[4], v[5]};
	}
	return file;
}
