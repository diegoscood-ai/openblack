/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include <WorldReader.h>

// Reads the world files of the game's saved games (<slot>.sav) as far as openblack knows their classes, and prints what
// it found: the start of the file, the records of each class, whether every stored running sum equals the reader's,
// where the read stopped and why, and whether the records read write back to the file's own bytes. With --fields,
// the values of the records asked for, in their class's field order, for reading what each field holds.

using namespace openblack::gamesave;

namespace
{
/// --full: a creature's fight fields and every word and step argument of the plan it keeps, after the short form
bool g_full = false;

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

// Where the game's tail keeps the help level and the help switch (among the help system's last words) and whether the
// leashes and the highlights are drawn (among the script's eight words), as the game's load reads them
constexpr size_t k_HelpLevelWord = 8;
constexpr size_t k_HelpOnWord = 13;
constexpr size_t k_DrawLeashWord = 0;
constexpr size_t k_DrawHighlightWord = 2;

/// --set-help and --set-camera-switches: the values a repair writes into a saved game's tail
struct Repair
{
	std::optional<std::array<uint32_t, 2>> help;     ///< the help level, then the help switch
	std::optional<std::array<uint32_t, 2>> switches; ///< whether the leashes, then the highlights, are drawn

	[[nodiscard]] bool Any() const { return help.has_value() || switches.has_value(); }
};

/// The help level and switch and the two drawing switches a game's tail holds, as one line
[[nodiscard]] std::string SwitchesOf(const GameTail& tail)
{
	return "help level " + std::to_string(tail.helpLastWords.at(k_HelpLevelWord)) + ", help on " +
	       std::to_string(tail.helpLastWords.at(k_HelpOnWord)) + "; leashes drawn " +
	       std::to_string(tail.scriptSecondWords.at(k_DrawLeashWord)) + ", highlights drawn " +
	       std::to_string(tail.scriptSecondWords.at(k_DrawHighlightWord));
}

/// The creation number of a record's object, for a record with an object part
std::optional<uint32_t> CreationIndexOf(const SavedRecord& record)
{
	return std::visit(
	    [](const auto& saved) -> std::optional<uint32_t> {
		    if constexpr (requires { saved.object.creationIndex; })
		    {
			    return saved.object.creationIndex;
		    }
		    else if constexpr (requires { saved.base.object.creationIndex; })
		    {
			    return saved.base.object.creationIndex;
		    }
		    else if constexpr (requires { saved.base.base.object.creationIndex; })
		    {
			    return saved.base.base.object.creationIndex;
		    }
		    else
		    {
			    return std::nullopt;
		    }
	    },
	    record);
}

template <typename T>
struct IsArray: std::false_type
{
};
template <typename T, size_t N>
struct IsArray<std::array<T, N>>: std::true_type
{
};
template <typename T>
struct IsByteArray: std::false_type
{
};
template <size_t N>
struct IsByteArray<std::array<uint8_t, N>>: std::true_type
{
};
template <typename T>
struct IsVector: std::false_type
{
};
template <typename T>
struct IsVector<std::vector<T>>: std::true_type
{
};
template <typename T>
struct IsOptional: std::false_type
{
};
template <typename T>
struct IsOptional<std::optional<T>>: std::true_type
{
};
template <typename T>
struct IsCounted: std::false_type
{
};
template <typename T>
struct IsCounted<Counted<T>>: std::true_type
{
};
template <typename T>
struct IsListed: std::false_type
{
};
template <typename T>
struct IsListed<Listed<T>>: std::true_type
{
};
template <typename T>
struct IsEach: std::false_type
{
};
template <typename T, size_t N>
struct IsEach<Each<T, N>>: std::true_type
{
};

void Append(std::string& out, const char* format, auto... values)
{
	std::array<char, 64> text {};
	const int written = std::snprintf(text.data(), text.size(), format, values...);
	out.append(text.data(), static_cast<size_t>(std::clamp(written, 0, static_cast<int>(text.size()) - 1)));
}

/// A value of a record as text: numbers as they are (floats also by their bits), references as #n, bytes in hex,
/// lists and parts in brackets, in their fields' order
template <typename T>
void Put(std::string& out, const T& value)
{
	if constexpr (std::is_same_v<T, bool>)
	{
		out += value ? "true" : "false";
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		Append(out, "%.9g", static_cast<double>(value));
	}
	else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
	{
		Append(out, "%lld", static_cast<long long>(value));
	}
	else if constexpr (std::is_integral_v<T>)
	{
		Append(out, "%llu", static_cast<unsigned long long>(value));
	}
	else if constexpr (std::is_same_v<T, Ref>)
	{
		Append(out, "#%u", value.number);
	}
	else if constexpr (std::is_same_v<T, RefList>)
	{
		out += "[";
		for (const auto number : value.numbers)
		{
			Append(out, " #%u", number);
		}
		out += " ]";
	}
	else if constexpr (IsByteArray<T>::value)
	{
		out += "x";
		for (const auto byte : value)
		{
			Append(out, "%02X", byte);
		}
	}
	else if constexpr (IsArray<T>::value || IsVector<T>::value)
	{
		out += "[";
		for (const auto& element : value)
		{
			out += " ";
			Put(out, element);
		}
		out += " ]";
	}
	else if constexpr (IsOptional<T>::value)
	{
		if (value.has_value())
		{
			Put(out, *value);
		}
		else
		{
			out += "none";
		}
	}
	else if constexpr (IsCounted<T>::value || IsEach<T>::value)
	{
		Put(out, value.values);
	}
	else if constexpr (IsListed<T>::value)
	{
		Put(out, value.elements);
	}
	else if constexpr (std::is_same_v<T, MapCoords>)
	{
		Append(out, "(%d %d ", value.x, value.z);
		Put(out, value.altitude);
		out += ")";
	}
	else if constexpr (std::is_same_v<T, ThingPart>)
	{
		Append(out, "thing(extra %u, destroyed %u, flags 0x%02X)", value.extra, value.destroyed, value.flags);
	}
	else if constexpr (std::is_same_v<T, PositionPart>)
	{
		out += "position(";
		Put(out, value.position);
		Append(out, " flags 0x%04X)", value.flags);
	}
	else if constexpr (std::is_same_v<T, PhysicsPart>)
	{
		out += "physics(";
		Put(out, value.matrix);
		Put(out, value.velocity);
		Put(out, value.spin);
		out += ")";
	}
	else if constexpr (std::is_same_v<T, ObjectPart>)
	{
		Append(out, "object(info %u ", value.info);
		Put(out, value.coords);
		Append(out, " creation %u life ", value.creationIndex);
		Put(out, value.life);
		out += " scale ";
		Put(out, value.scale);
		out += " heading ";
		Put(out, value.heading);
		Append(out, " fire #%u ", value.fire);
		Put(out, value.physics);
		out += ")";
	}
	else if constexpr (std::is_same_v<T, FixedPart>)
	{
		Append(out, "fixed(#%u)", value.townArtifact);
	}
	else if constexpr (std::is_same_v<T, RuleDataRecord>)
	{
		Append(out, "{ rule(%u %u) values x", value.persistent.first, value.persistent.second);
		for (const auto byte : value.values)
		{
			Append(out, "%02X", byte);
		}
		out += " references [";
		for (const auto number : value.references)
		{
			Append(out, " #%u", number);
		}
		out += " ] }";
	}
	else if constexpr (std::is_same_v<T, KeptCreaturePlan>)
	{
		// the plan a creature keeps: what it wants and does, the step it is at of the agenda's count, its state, and the
		// sub-action of each step from the current one on
		Append(out, "plan(desire %u action %u current %u/%u state %u steps [", value.desire, value.action, value.current,
		       value.count, value.state);
		for (const auto& step : value.steps)
		{
			Append(out, " 0x%X", step.action);
		}
		out += " ])";
		if (g_full)
		{
			Append(out, " full(objects #%u #%u #%u first %u words %u %u %u creature #%u other #%u steps [",
			       value.objects[0].number, value.objects[1].number, value.objects[2].number, value.firstWord, value.words[0],
			       value.words[1], value.words[2], value.creature.number, value.other.number);
			for (const auto& step : value.steps)
			{
				Append(out, " {0x%X #%u int 0x%X pt (%.9g %.9g %.9g) real %.9g spell %u word %u other #%u kind 0x%X}",
				       step.action, step.object.number, step.integer, static_cast<double>(step.point[0]),
				       static_cast<double>(step.point[1]), static_cast<double>(step.point[2]), static_cast<double>(step.real),
				       step.spell, step.word, step.other.number, step.orderKind);
			}
			out += " ])";
		}
	}
	else if constexpr (std::is_same_v<T, CreatureFields>)
	{
		// a creature's own fields: whether it keeps a plan, and that plan
		Append(out, "creature(index %u owner %u keepsPlan %u", value.index, value.owner, value.keepsPlan);
		if (value.keepsPlan != 0)
		{
			out += " ";
			Put(out, value.plan);
		}
		if (g_full)
		{
			Append(out, " fight(arena #%u health %.9g auto %u)", value.arena.number, static_cast<double>(value.fightHealth),
			       value.autoFight);
		}
		out += ")";
	}
	else if constexpr (requires(T& part) { T::FieldsOf(part); })
	{
		out += "{";
		std::apply(
		    [&out](const auto&... fields) {
			    size_t index = 0;
			    ((Append(out, " %zu:", index++), Put(out, fields)), ...);
			    static_cast<void>(index);
		    },
		    T::FieldsOf(value));
		out += " }";
	}
	else
	{
		out += "?";
	}
}

/// Prints the records of the numbers asked for: their class, their thing part and their fields
void ShowFields(const WorldRead& world, const std::vector<uint32_t>& numbers)
{
	for (const auto number : numbers)
	{
		if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			std::printf("  object %u: not read\n", number);
			continue;
		}
		const auto& record = *world.objects[number - 1];
		std::string text;
		std::visit(
		    [&text](const auto& saved) {
			    if constexpr (requires { saved.thing; })
			    {
				    Put(text, saved.thing);
			    }
			    Put(text, saved);
		    },
		    record);
		std::printf("  object %u (%s): %s\n", number, TypeName(TypeOf(record)).c_str(), text.c_str());
	}
}

/// The numbers of a comma separated list, none for a wrong one
std::vector<uint32_t> NumbersOf(std::string_view list)
{
	std::vector<uint32_t> numbers;
	while (!list.empty())
	{
		const auto comma = list.find(',');
		const auto item = list.substr(0, comma);
		uint32_t number = 0;
		const auto [end, error] = std::from_chars(item.data(), item.data() + item.size(), number);
		if (error != std::errc {} || end != item.data() + item.size())
		{
			return {};
		}
		numbers.push_back(number);
		list = comma == std::string_view::npos ? std::string_view {} : list.substr(comma + 1);
	}
	return numbers;
}

bool IsWorldFile(const std::filesystem::path& path)
{
	return path.extension() == ".sav" && !path.filename().string().starts_with("Script_");
}

/// Prints one file and gives whether it read with every sum equal and wrote back the same
bool Show(const std::filesystem::path& path, bool records, bool lists, const std::vector<uint32_t>& fields,
          std::map<uint32_t, uint32_t>& total)
{
	const auto bytes = ReadAll(path);
	const auto world = ReadWorld(bytes);
	std::printf("== %s (%zu bytes)\n", path.string().c_str(), bytes.size());
	if (world.objects.empty())
	{
		std::printf("  %s\n", world.stop.c_str());
		return false;
	}
	ShowFields(world, fields);
	std::printf("  load attempts %u, %s, %s\n", world.framing.attempts, world.framing.revision.c_str(),
	            world.framing.landscape.c_str());
	if (const auto* game = std::get_if<GameHead>(&*world.objects.front()); game != nullptr)
	{
		std::printf("  game flags 0x%08X, land %u\n", game->gameFlags, game->landNumber);
		std::printf("  %s\n", SwitchesOf(game->tail).c_str());
		if (lists)
		{
			// each global list in the game's order: its count, then its objects' classes as runs, in the list's order
			for (size_t list = 0; list < game->lists.size(); ++list)
			{
				const auto& numbers = game->lists.at(list).numbers;
				std::printf("  list %2zu: %zu", list, numbers.size());
				std::string last;
				size_t run = 0;
				const auto flush = [&last, &run]() {
					if (run != 0)
					{
						std::printf(" | %zu %s", run, last.c_str());
					}
				};
				for (const auto number : numbers)
				{
					const bool known = number != 0 && number <= world.objects.size() && world.objects[number - 1].has_value();
					const auto name = known ? TypeName(TypeOf(*world.objects[number - 1])) : std::string("?");
					if (name != last)
					{
						flush();
						last = name;
						run = 0;
					}
					++run;
				}
				flush();
				// whether the list runs by its objects' creation numbers, the newest first
				size_t numbered = 0;
				size_t outOfOrder = 0;
				std::optional<uint32_t> previous;
				for (const auto number : numbers)
				{
					if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
					{
						continue;
					}
					const auto index = CreationIndexOf(*world.objects[number - 1]);
					if (!index.has_value())
					{
						continue;
					}
					++numbered;
					if (previous.has_value() && *index >= *previous)
					{
						++outOfOrder;
					}
					previous = index;
				}
				if (numbered > 1)
				{
					std::printf(" || newest first: %s (%zu of %zu out)", outOfOrder == 0 ? "yes" : "no", outOfOrder, numbered);
				}
				std::printf("\n");
			}
		}
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
/// Writes a repair into one world file. Only a file read to its end, with every sum equal and written back the same, is
/// changed; its first form is kept beside it as <file>.orig, and the result is read again to check it.
bool RepairFile(const std::filesystem::path& path, const Repair& repair)
{
	const auto bytes = ReadAll(path);
	auto world = ReadWorld(bytes);
	auto* game =
	    world.objects.empty() || !world.objects.front().has_value() ? nullptr : std::get_if<GameHead>(&*world.objects.front());
	if (game == nullptr || world.firstWrongSum.has_value() || world.stoppedAt != bytes.size() || WriteWorld(world) != bytes)
	{
		std::printf("== %s: not repaired: it does not read to its end and write back the same\n", path.string().c_str());
		return false;
	}
	auto& tail = game->tail;
	const auto before = SwitchesOf(tail);
	if (repair.help.has_value())
	{
		tail.helpLastWords.at(k_HelpLevelWord) = repair.help->at(0);
		tail.helpLastWords.at(k_HelpOnWord) = repair.help->at(1);
	}
	if (repair.switches.has_value())
	{
		tail.scriptSecondWords.at(k_DrawLeashWord) = repair.switches->at(0);
		tail.scriptSecondWords.at(k_DrawHighlightWord) = repair.switches->at(1);
	}
	std::printf("== %s: %s -> %s\n", path.string().c_str(), before.c_str(), SwitchesOf(tail).c_str());
	const auto written = WriteWorld(world);
	if (written == bytes)
	{
		std::printf("  already so, left as it is\n");
		return true;
	}
	if (const auto again = ReadWorld(written); again.firstWrongSum.has_value() || again.stoppedAt != written.size())
	{
		std::printf("  not repaired: the repaired file does not read back whole\n");
		return false;
	}
	auto kept = path;
	kept += ".orig";
	std::error_code error;
	if (!std::filesystem::exists(kept, error) &&
	    !std::filesystem::copy_file(path, kept, std::filesystem::copy_options::none, error))
	{
		std::printf("  not repaired: %s could not be kept\n", kept.string().c_str());
		return false;
	}
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(written.data()), static_cast<std::streamsize>(written.size()));
	std::printf("  repaired, the first form kept as %s\n", kept.string().c_str());
	return stream.good();
}

/// Two numbers separated by a comma, for --set-help and --set-camera-switches
std::optional<std::array<uint32_t, 2>> PairOf(std::string_view text)
{
	const auto numbers = NumbersOf(text);
	if (numbers.size() != 2)
	{
		return std::nullopt;
	}
	return std::array {numbers.at(0), numbers.at(1)};
}
} // namespace

int main(int argc, char* argv[]) noexcept
{
	if (argc < 2)
	{
		std::printf("usage: savetool [--records] [--lists] [--fields N[,N...]] FILE|FOLDER...\n"
		            "Reads the world files of saved games (<slot>.sav; a folder: every one in it) as far as their classes\n"
		            "are known, counts the records of each class, checks every stored sum and writes the records back.\n"
		            "--fields prints the values of the records of those numbers, in their class's field order.\n"
		            "--set-help LEVEL,ON and --set-camera-switches LEASHES,HIGHLIGHTS write those words into each file's\n"
		            "tail instead, keeping its first form as <file>.orig (the repair of a save an older writer made).\n");
		return EXIT_FAILURE;
	}
	bool records = false;
	bool lists = false;
	bool fieldsNext = false;
	Repair repair;
	std::optional<std::array<uint32_t, 2>>* pairNext = nullptr;
	std::vector<uint32_t> fields;
	std::vector<std::filesystem::path> files;
	for (const std::string_view arg : std::span(argv + 1, static_cast<size_t>(argc - 1)))
	{
		if (fieldsNext)
		{
			fieldsNext = false;
			fields = NumbersOf(arg);
			if (fields.empty())
			{
				std::printf("--fields takes numbers separated by commas, not %.*s\n", static_cast<int>(arg.size()), arg.data());
				return EXIT_FAILURE;
			}
			continue;
		}
		if (pairNext != nullptr)
		{
			*pairNext = PairOf(arg);
			if (!pairNext->has_value())
			{
				std::printf("--set-help and --set-camera-switches take two numbers separated by a comma, not %.*s\n",
				            static_cast<int>(arg.size()), arg.data());
				return EXIT_FAILURE;
			}
			pairNext = nullptr;
			continue;
		}
		if (arg == "--set-help")
		{
			pairNext = &repair.help;
			continue;
		}
		if (arg == "--set-camera-switches")
		{
			pairNext = &repair.switches;
			continue;
		}
		if (arg == "--fields")
		{
			fieldsNext = true;
			continue;
		}
		if (arg == "--full")
		{
			g_full = true;
			continue;
		}
		if (arg == "--lists")
		{
			lists = true;
			continue;
		}
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
	if (repair.Any())
	{
		for (const auto& file : files)
		{
			failures += RepairFile(file, repair) ? 0 : 1;
		}
		return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	for (const auto& file : files)
	{
		failures += Show(file, records, lists, fields, total) ? 0 : 1;
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
