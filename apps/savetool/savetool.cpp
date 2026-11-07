/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <WorldReader.h>

// Reads the world files of the game's saved games (<slot>.sav) as far as openblack knows their classes, and prints what
// it found: the start of the file, the records of each class, whether every stored running sum equals the reader's,
// where the read stopped and why, and whether the records read write back to the file's own bytes.

using namespace openblack::gamesave;

namespace
{
std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool IsWorldFile(const std::filesystem::path& path)
{
	return path.extension() == ".sav" && !path.filename().string().starts_with("Script_");
}

/// Prints one file and gives whether it read with every sum equal and wrote back the same
bool Show(const std::filesystem::path& path, bool records, std::map<uint32_t, uint32_t>& total)
{
	const auto bytes = ReadAll(path);
	const auto world = ReadWorld(bytes);
	std::printf("== %s (%zu bytes)\n", path.string().c_str(), bytes.size());
	if (world.objects.empty())
	{
		std::printf("  %s\n", world.stop.c_str());
		return false;
	}
	std::printf("  load attempts %u, %s, %s\n", world.framing.attempts, world.framing.revision.c_str(),
	            world.framing.landscape.c_str());
	if (const auto* game = std::get_if<GameHead>(&*world.objects.front()); game != nullptr)
	{
		std::printf("  game flags 0x%08X, land %u\n", game->gameFlags, game->landNumber);
	}
	if (records)
	{
		for (size_t i = 0; i < world.objects.size(); ++i)
		{
			if (world.objects[i].has_value())
			{
				std::printf("  object %zu at byte 0x%zX: %s\n", i + 1, world.positions[i],
				            TypeName(TypeOf(*world.objects[i])).c_str());
			}
		}
	}
	const auto census = world.Census();
	uint32_t read = 0;
	for (const auto& [type, count] : census)
	{
		std::printf("  %5u %s\n", count, TypeName(type).c_str());
		total[type] += count;
		read += count;
	}
	const bool sumsEqual = !world.firstWrongSum.has_value();
	if (sumsEqual)
	{
		std::printf("  %u records read, %u stored sums, every one equal to the reader's\n", read, world.sumsRead);
	}
	else
	{
		std::printf("  %u records read; the stored sum at byte 0x%zX is not the reader's: a layout before it is wrong\n", read,
		            *world.firstWrongSum);
	}
	std::printf("  stopped at byte 0x%zX (%.1f%% of the file): %s\n", world.stoppedAt,
	            100.0 * static_cast<double>(world.stoppedAt) / static_cast<double>(std::max<size_t>(bytes.size(), 1)),
	            world.stop.c_str());
	const auto written = WriteWorld(world);
	const bool same = written.size() == world.stoppedAt && std::equal(written.begin(), written.end(), bytes.begin());
	std::printf("  written again up to there: %s\n", same ? "identical" : "DIFFERENT");
	return sumsEqual && same;
}
} // namespace

int main(int argc, char* argv[]) noexcept
{
	if (argc < 2)
	{
		std::printf("usage: savetool [--records] FILE|FOLDER...\n"
		            "Reads the world files of saved games (<slot>.sav; a folder: every one in it) as far as their classes\n"
		            "are known, counts the records of each class, checks every stored sum and writes the records back.\n");
		return EXIT_FAILURE;
	}
	bool records = false;
	std::vector<std::filesystem::path> files;
	for (const std::string_view arg : std::span(argv + 1, static_cast<size_t>(argc - 1)))
	{
		if (arg == "--records")
		{
			records = true;
			continue;
		}
		const std::filesystem::path path(arg);
		std::error_code error;
		if (std::filesystem::is_directory(path, error))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(path, error))
			{
				if (entry.is_regular_file() && IsWorldFile(entry.path()))
				{
					files.push_back(entry.path());
				}
			}
		}
		else
		{
			files.push_back(path);
		}
	}
	std::ranges::sort(files);
	int failures = 0;
	std::map<uint32_t, uint32_t> total;
	for (const auto& file : files)
	{
		failures += Show(file, records, total) ? 0 : 1;
	}
	if (files.size() > 1)
	{
		std::printf("== %zu files, %d with a wrong sum or different bytes\n", files.size(), failures);
		for (const auto& [type, count] : total)
		{
			std::printf("  %5u %s\n", count, TypeName(type).c_str());
		}
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
