/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMFile.h>
#include <gtest/gtest.h>

#include "ECS/SavedScriptObjects.h"

using namespace openblack;
using namespace openblack::lhvm;
namespace table = openblack::ecs::saved_script_objects;

namespace
{

VMStack StackOf(std::initializer_list<std::pair<uint32_t, DataType>> values)
{
	VMStack stack;
	for (const auto& [value, type] : values)
	{
		stack.values.at(stack.count) = VMValue(value);
		stack.types.at(stack.count) = type;
		++stack.count;
	}
	return stack;
}

// A running program whose globals, task locals, task stack and main stack name objects, among values of other types
LHVMFile ProgramNaming(uint32_t global, uint32_t local, uint32_t onStack, uint32_t onMainStack)
{
	const std::vector<VMInstruction> code {
	    VMInstruction(Opcode::End, VMMode::Immediate, DataType::None, VMValue(), 1),
	};
	const std::vector<VMScript> scripts {VMScript("First", "first.txt", ScriptType::Script, 0, {"local"}, 0, 0, 1)};
	const std::vector<VMVar> globals {
	    VMVar(DataType::Object, VMValue(global), "Thing"),
	    VMVar(DataType::Int, VMValue(global), "Count"),
	    VMVar(DataType::Object, VMValue(0u), "Nothing"),
	};
	VMTask task({VMVar(DataType::Object, VMValue(local), "local")}, 1, 3, 0, 0,
	            StackOf({{onStack, DataType::Int}, {onStack, DataType::Object}}), "First", "first.txt", ScriptType::Script);
	return {LHVMVersion::BlackAndWhite,
	        {"Thing", "Count", "Nothing"},
	        code,
	        {},
	        scripts,
	        {'\0'},
	        StackOf({{onMainStack, DataType::Object}}),
	        globals,
	        {task},
	        1,
	        1,
	        3,
	        1,
	        1};
}

std::filesystem::path TempFile(const std::string& name)
{
	return std::filesystem::temp_directory_path() / ("openblack_" + name + ".sav");
}

} // namespace

TEST(SavedScriptObjects, ThePlacesAreTheNamedAndHeldObjectsOnceEachInEntityOrderWithoutTheNull)
{
	const std::vector<uint32_t> named {70, 0, 42, 70, 9};
	const std::vector<uint32_t> held {42, 5};
	const auto places = table::PlacesOf(named, held);
	ASSERT_TRUE(places.has_value());
	EXPECT_EQ(*places, (std::vector<uint32_t> {5, 9, 42, 70}));
	EXPECT_EQ(table::PlaceOf(*places, 5), 1u);
	EXPECT_EQ(table::PlaceOf(*places, 70), 4u);
	EXPECT_EQ(table::PlaceOf(*places, 0), 0u);
	EXPECT_EQ(table::PlaceOf(*places, 6), 0u);
}

TEST(SavedScriptObjects, MoreObjectsThanTheTableHoldsHaveNoPlaces)
{
	std::vector<uint32_t> named;
	for (uint32_t id = 1; id <= table::k_Places; ++id)
	{
		named.push_back(id);
	}
	EXPECT_TRUE(table::PlacesOf(named, {}).has_value());
	named.push_back(table::k_Places + 1);
	EXPECT_FALSE(table::PlacesOf(named, {}).has_value());
}

TEST(SavedScriptObjects, APlaceGivesItsLoadedObjectAndNothingOutsideTheTable)
{
	const std::vector<uint32_t> loaded {900, 0, 901};
	EXPECT_EQ(table::ValueOf(loaded, 0), 0u);
	EXPECT_EQ(table::ValueOf(loaded, 1), 900u);
	EXPECT_EQ(table::ValueOf(loaded, 2), 0u);
	EXPECT_EQ(table::ValueOf(loaded, 3), 901u);
	EXPECT_EQ(table::ValueOf(loaded, 4), 0u);
}

TEST(SavedScriptObjects, TheMachineListsAndMapsOnlyItsObjectValues)
{
	const auto file = TempFile("script_objects_values");
	ASSERT_TRUE(ProgramNaming(70, 42, 9, 70).Write(file));
	LHVM vm;
	ASSERT_EQ(vm.RestoreState(file), EXIT_SUCCESS);
	// the globals, the task's local and stack, then the main stack; the int of the same value is no object
	EXPECT_EQ(vm.ObjectValues(), (std::vector<uint32_t> {70, 0, 42, 9, 70}));
	vm.MapObjects([](uint32_t value) { return value + 1000; });
	EXPECT_EQ(vm.ObjectValues(), (std::vector<uint32_t> {1070, 1000, 1042, 1009, 1070}));
	EXPECT_EQ(vm.GetVariables().at(1).value.uintVal, 70u);
	std::filesystem::remove(file);
}

// A save names the objects by their places, and a load whose objects became other entities names those
TEST(SavedScriptObjects, ASavedMachineNamesTheLoadedObjectsOfItsPlaces)
{
	const auto running = TempFile("script_objects_running");
	const auto saved = TempFile("script_objects_saved");
	ASSERT_TRUE(ProgramNaming(70, 42, 9, 70).Write(running));
	LHVM vm;
	ASSERT_EQ(vm.RestoreState(running), EXIT_SUCCESS);

	const auto values = vm.ObjectValues();
	const auto places = table::PlacesOf(values, {});
	ASSERT_TRUE(places.has_value());
	ASSERT_EQ(*places, (std::vector<uint32_t> {9, 42, 70}));
	vm.MapObjects([&places](uint32_t value) { return table::PlaceOf(*places, value); });
	ASSERT_EQ(vm.SaveState(saved), EXIT_SUCCESS);
	EXPECT_EQ(vm.ObjectValues(), (std::vector<uint32_t> {3, 0, 2, 1, 3}));

	// the load: object 9 became 309, 42 was not loaded, 70 became 370
	const std::vector<uint32_t> loaded {309, 0, 370};
	LHVM other;
	ASSERT_EQ(other.RestoreState(saved), EXIT_SUCCESS);
	other.MapObjects([&loaded](uint32_t place) { return table::ValueOf(loaded, place); });
	EXPECT_EQ(other.ObjectValues(), (std::vector<uint32_t> {370, 0, 0, 309, 370}));
	std::filesystem::remove(running);
	std::filesystem::remove(saved);
}
