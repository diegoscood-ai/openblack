/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mod library (src/Mods): versions and ranges, mod.json, engine switches, dependencies and load order, modpacks
// and the built-in manifests (the 11 mods that come with openblack).

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

#include <gtest/gtest.h>

#include "ECS/GUtilsAngle.h"
#include "ECS/GUtilsDistance.h"
#include "EngineConfig.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Mods/Api.h"
#include "Mods/BuiltinManifests.h"
#include "Mods/Lua/LuaHost.h"
#include "Mods/Manifest.h"
#include "Mods/ModLog.h"
#include "Mods/ModRegistry.h"
#include "Mods/Native/NativeHost.h"
#include "Mods/Replacements.h"
#include "Mods/RuleFiles.h"
#include "Mods/Switches.h"

using namespace openblack;
using namespace openblack::mods;

namespace
{
class ModsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::config::emplace();
		switches::Clear();
		switches::RegisterEngineSwitches();
		_folder = std::filesystem::temp_directory_path() / "openblack_test_mods";
		std::filesystem::remove_all(_folder);
		std::filesystem::create_directories(_folder);
	}

	void TearDown() override
	{
		std::error_code error;
		std::filesystem::remove_all(_folder, error);
		switches::Clear();
	}

	void WriteFile(const std::filesystem::path& relative, std::string_view text) const
	{
		std::filesystem::create_directories((_folder / relative).parent_path());
		std::ofstream file(_folder / relative, std::ios::binary);
		file << text;
	}

	std::filesystem::path _folder;
};

std::unique_ptr<Mod> Make(std::string_view json)
{
	auto result = ParseManifest(json, {}, "");
	EXPECT_TRUE(result.errors.empty()) << (result.errors.empty() ? "" : result.errors.front());
	return std::move(result.mod);
}
} // namespace

TEST(Semver, ParsesAndCompares)
{
	EXPECT_EQ(Version::Parse("1.2.3"), (Version {1, 2, 3}));
	EXPECT_EQ(Version::Parse("1.2"), (Version {1, 2, 0}));
	EXPECT_EQ(Version::Parse("2"), (Version {2, 0, 0}));
	EXPECT_EQ(Version::Parse("1.0.0-beta+abc"), (Version {1, 0, 0}));
	EXPECT_FALSE(Version::Parse(""));
	EXPECT_FALSE(Version::Parse("1..2"));
	EXPECT_FALSE(Version::Parse("1.2.3.4"));
	EXPECT_FALSE(Version::Parse("a.b"));
	EXPECT_LT(*Version::Parse("1.9.9"), *Version::Parse("1.10.0"));
}

TEST(Semver, Ranges)
{
	const auto check = [](std::string_view range, std::string_view version) {
		return VersionRange::Parse(range)->Contains(*Version::Parse(version));
	};
	EXPECT_TRUE(check("*", "0.0.1"));
	EXPECT_TRUE(check("", "5.0.0"));
	EXPECT_TRUE(check(">=1.0 <2.0", "1.5.0"));
	EXPECT_FALSE(check(">=1.0 <2.0", "2.0.0"));
	EXPECT_TRUE(check("^1.2", "1.9.0"));
	EXPECT_FALSE(check("^1.2", "2.0.0"));
	EXPECT_FALSE(check("^1.2", "1.1.0"));
	EXPECT_TRUE(check("^0.3", "0.3.9"));
	EXPECT_FALSE(check("^0.3", "0.4.0"));
	EXPECT_TRUE(check("~1.2", "1.2.7"));
	EXPECT_FALSE(check("~1.2", "1.3.0"));
	EXPECT_TRUE(check("1.2.3", "1.2.3"));
	EXPECT_FALSE(check("=1.2.3", "1.2.4"));
	EXPECT_FALSE(VersionRange::Parse(">=x"));
}

TEST_F(ModsTest, ManifestOptionsBindSwitches)
{
	auto mod = Make(R"({
		"id": "test.bind", "name": {"en": "Bind", "es": "Atar"}, "version": "1.2.0", "category": "Test",
		"switches": {"water.living": true},
		"options": [
			{"id": "samples", "values": ["2x", "4x", "8x"], "default": "4x", "bind": "graphics.msaa.samples"},
			{"id": "look", "values": ["off", "soft", "round"], "default": 2,
			 "bind": {"graphics.hd-tweaks.smooth": {"soft": 2, "round": 3}}},
			{"id": "sharp", "type": "bool", "default": true, "bind": {"graphics.hd-tweaks.mip-bias": -1.0}},
			{"id": "speed", "type": "slider", "values": ["x1", "x10"], "default": "x10", "bind": "world.crops.growth"}
		]})");
	ASSERT_NE(mod, nullptr);
	EXPECT_EQ(mod->GetInfo().name, "Bind");
	EXPECT_EQ(mod->GetInfo().version, (Version {1, 2, 0}));
	ASSERT_EQ(mod->GetOptions().size(), 4u);
	EXPECT_EQ(mod->GetChoice("samples"), "4x");
	EXPECT_EQ(mod->GetChoice("look"), "round");
	EXPECT_EQ(mod->GetChoice("sharp"), "on");
	EXPECT_TRUE(mod->GetOptions()[3].slider);

	std::map<std::string, double, std::less<>> values;
	mod->CollectSwitches(values);
	EXPECT_EQ(values["water.living"], 1.0);
	EXPECT_EQ(values["graphics.msaa.samples"], 4.0);
	EXPECT_EQ(values["graphics.hd-tweaks.smooth"], 3.0);
	EXPECT_EQ(values["graphics.hd-tweaks.mip-bias"], -1.0);
	EXPECT_EQ(values["world.crops.growth"], 10.0);
}

TEST_F(ModsTest, LanguageIsChosen)
{
	SetLanguage("es");
	auto mod = Make(R"({"id": "test.lang", "name": {"en": "Grass", "es": "Hierba"}})");
	EXPECT_EQ(mod->GetInfo().name, "Hierba");
	SetLanguage("de");
	mod = Make(R"({"id": "test.lang", "name": {"en": "Grass", "es": "Hierba"}})");
	EXPECT_EQ(mod->GetInfo().name, "Grass");
	SetLanguage("en");
}

TEST_F(ModsTest, BadManifests)
{
	EXPECT_FALSE(ParseManifest("not json", {}, "").mod);
	EXPECT_FALSE(ParseManifest(R"({"name": "no id"})", {}, "").mod);
	EXPECT_FALSE(ParseManifest(R"({"id": "Bad Id"})", {}, "").mod);
	EXPECT_FALSE(ParseManifest(R"({"id": "test.v", "version": "one"})", {}, "").mod);
	EXPECT_FALSE(ParseManifest(R"({"id": "test.v", "dependencies": {"x": ">=nope"}})", {}, "").mod);
	EXPECT_FALSE(ParseManifest(R"({"id": "test.v", "schema": 99})", {}, "").mod);
	// unknown switches only cost the binding, with a warning
	const auto result = ParseManifest(R"({"id": "test.w", "switches": {"no.such": true},
		"options": [{"id": "a", "values": ["x"], "bind": "no.such"}]})",
	                                  {}, "");
	ASSERT_TRUE(result.mod);
	EXPECT_EQ(result.warnings.size(), 2u);
}

TEST_F(ModsTest, SwitchesClampAndReset)
{
	auto& config = Locator::config::value();
	EXPECT_TRUE(switches::Set("graphics.msaa.samples", 99.0));
	EXPECT_EQ(config.msaa, 16);
	EXPECT_TRUE(switches::Set("world.crops.growth", 7.5));
	EXPECT_FLOAT_EQ(config.fieldGrowthMultiplier, 7.5f);
	EXPECT_TRUE(switches::Set("game.free-start", 3.0));
	EXPECT_TRUE(config.skipIntroFreeStart);
	EXPECT_FALSE(switches::Set("no.such.switch", 1.0));
	switches::ResetAll();
	EXPECT_EQ(config.msaa, 0);
	EXPECT_FLOAT_EQ(config.fieldGrowthMultiplier, 1.0f);
	EXPECT_FALSE(config.skipIntroFreeStart);
}

TEST_F(ModsTest, BuiltinManifestsAllParse)
{
	ASSERT_GE(builtin::Manifests().size(), 11u);
	for (const auto& [folder, json] : builtin::Manifests())
	{
		const auto result = ParseManifest(json, {}, "");
		EXPECT_TRUE(result.mod) << folder;
		EXPECT_TRUE(result.errors.empty()) << folder;
		EXPECT_TRUE(result.warnings.empty()) << folder << ": " << (result.warnings.empty() ? "" : result.warnings.front());
		if (result.mod)
		{
			EXPECT_EQ(result.mod->GetInfo().id, folder);
		}
	}
}

// The built-in mods set the same EngineConfig fields as the C++ classes they replace did, for every option
TEST_F(ModsTest, BuiltinModsSetTheOldValues)
{
	ModRegistry registry;
	registry.Discover(_folder);
	const auto apply = [&](std::initializer_list<std::string_view> arguments) {
		for (const auto& argument : arguments)
		{
			EXPECT_EQ(registry.ApplyArgument(argument), "") << argument;
		}
		registry.ApplyAll();
	};
	auto& config = Locator::config::value();

	// only skip-intro is on by default
	apply({});
	EXPECT_EQ(config.msaa, 0);
	EXPECT_EQ(config.skipTutorialChoice, 3);
	EXPECT_TRUE(config.skipIntroFreeStart);
	EXPECT_FLOAT_EQ(config.foliageDensity, 0.0f);
	EXPECT_TRUE(config.testDispensersSeed);

	apply({"game.skip-intro.skip=tutorial", "game.skip-intro.free start=off"});
	EXPECT_EQ(config.skipTutorialChoice, 1);
	EXPECT_FALSE(config.skipIntroFreeStart);
	apply({"game.skip-intro=off"});
	EXPECT_EQ(config.skipTutorialChoice, 0);

	apply({"graphics.msaa", "graphics.msaa.samples=8x"});
	EXPECT_EQ(config.msaa, 8);
	apply({"graphics.mipmaps", "graphics.anisotropic"});
	EXPECT_TRUE(config.textureMipmaps);
	EXPECT_TRUE(config.anisotropicFiltering);

	apply({"graphics.terrain-x2"});
	EXPECT_FLOAT_EQ(config.terrainTextureDensity, 2.0f);
	EXPECT_FALSE(config.terrainTexturesX2);
	EXPECT_TRUE(config.terrainTriplanar);
	apply({"graphics.terrain-x2.repeat=x4", "graphics.terrain-x2.upscale=on", "graphics.terrain-x2.cliffs=stretched"});
	EXPECT_FLOAT_EQ(config.terrainTextureDensity, 4.0f);
	EXPECT_TRUE(config.terrainTexturesX2);
	EXPECT_FALSE(config.terrainTriplanar);

	apply({"graphics.hd-tweaks"});
	EXPECT_TRUE(config.hdTweaksTextures);
	EXPECT_EQ(config.hdTweaksSmoothLevel, 3);
	EXPECT_EQ(config.hdTweaksLighting, 1);
	EXPECT_FLOAT_EQ(config.hdTweaksMipBias, -1.0f);
	EXPECT_TRUE(config.hdTweaksHighDetail);
	apply({"graphics.hd-tweaks.smooth=soft", "graphics.hd-tweaks.sharp=off", "graphics.hd-tweaks.detail=original"});
	EXPECT_EQ(config.hdTweaksSmoothLevel, 2);
	EXPECT_FLOAT_EQ(config.hdTweaksMipBias, 0.0f);
	EXPECT_FALSE(config.hdTweaksHighDetail);

	apply({"world.foliage", "world.foliage.density=very high", "world.foliage.distance=far"});
	EXPECT_FLOAT_EQ(config.foliageDensity, 4.0f);
	EXPECT_FLOAT_EQ(config.foliageDistance, 320.0f);
	EXPECT_TRUE(config.foliageFields);

	apply({"world.crops", "world.crops.speed=x50"});
	EXPECT_TRUE(config.fieldsWithoutFarmers);
	EXPECT_FLOAT_EQ(config.fieldGrowthMultiplier, 50.0f);

	apply({"test.miracle-dispensers", "test.miracle-dispensers.level=pu2", "test.miracle-dispensers.recharge=30s",
	       "test.miracle-dispensers.seed=off"});
	EXPECT_TRUE(config.testDispensers);
	EXPECT_EQ(config.testDispensersLevel, 2);
	EXPECT_FLOAT_EQ(config.testDispensersSeconds, 30.0f);
	EXPECT_FALSE(config.testDispensersSeed);

	apply({"water.living", "world.ground-statics"});
	EXPECT_TRUE(config.livingWater);
	EXPECT_TRUE(config.groundStaticObjects);

	// everything off again: the original's values
	apply({"graphics.msaa=off", "graphics.hd-tweaks=off", "world.foliage=off", "world.crops=off",
	       "test.miracle-dispensers=off", "water.living=off"});
	EXPECT_EQ(config.msaa, 0);
	EXPECT_FALSE(config.hdTweaksTextures);
	EXPECT_FLOAT_EQ(config.foliageDensity, 0.0f);
	EXPECT_FLOAT_EQ(config.fieldGrowthMultiplier, 1.0f);
	EXPECT_FALSE(config.testDispensers);
	EXPECT_FALSE(config.livingWater);
}

TEST_F(ModsTest, DependenciesBlockAndOrder)
{
	WriteFile("lib.base/mod.json", R"({"id": "lib.base", "version": "1.4.0"})");
	WriteFile("uses.base/mod.json", R"({"id": "uses.base", "dependencies": {"lib.base": "^1.2"}})");
	WriteFile("too.new/mod.json", R"({"id": "too.new", "dependencies": {"lib.base": ">=2.0"}})");
	WriteFile("chained/mod.json", R"({"id": "chained", "dependencies": {"too.new": "*"}})");
	WriteFile("hates.base/mod.json", R"({"id": "hates.base", "incompatible": {"lib.base": "*"}})");
	WriteFile("wrong.api/mod.json", R"({"id": "wrong.api", "api": ">=9.0"})");
	WriteFile("aaa.after/mod.json", R"({"id": "aaa.after", "load_after": ["lib.base"]})");
	ModRegistry registry;
	registry.Discover(_folder);
	for (const auto* id : {"lib.base", "uses.base", "too.new", "chained", "hates.base", "wrong.api", "aaa.after"})
	{
		EXPECT_EQ(registry.ApplyArgument(id), "") << id;
	}
	registry.ApplyAll();
	EXPECT_TRUE(registry.IsActive(*registry.Find("uses.base")));
	EXPECT_NE(registry.Find("too.new")->GetBlockedReason().find("needs lib.base >=2.0, found 1.4.0"), std::string::npos);
	EXPECT_NE(registry.Find("chained")->GetBlockedReason().find("blocked"), std::string::npos);
	EXPECT_FALSE(registry.IsActive(*registry.Find("hates.base")));
	EXPECT_FALSE(registry.IsActive(*registry.Find("wrong.api")));

	// switching the library off blocks what needs it
	EXPECT_EQ(registry.ApplyArgument("lib.base=off"), "");
	registry.ApplyAll();
	EXPECT_NE(registry.Find("uses.base")->GetBlockedReason().find("off"), std::string::npos);
	EXPECT_TRUE(registry.IsActive(*registry.Find("hates.base")));

	// load order: what a mod needs comes before it, even against the alphabet
	const auto& order = registry.GetLoadOrder();
	const auto position = [&order](std::string_view id) {
		return std::ranges::find_if(order, [id](const Mod* mod) { return mod->GetInfo().id == id; }) - order.begin();
	};
	EXPECT_LT(position("lib.base"), position("aaa.after"));
	EXPECT_LT(position("lib.base"), position("uses.base"));
	EXPECT_LT(position("too.new"), position("chained"));
}

TEST_F(ModsTest, ModpacksAndLegacyFolders)
{
	WriteFile("pack.demo/modpack.json", R"({"id": "pack.demo", "name": "Demo pack", "version": "2.0"})");
	WriteFile("pack.demo/demo.one/mod.json", R"({"id": "demo.one"})");
	WriteFile("pack.demo/demo.two/mod.json", R"({"id": "demo.two"})");
	WriteFile("old.data/mod.cfg", "name = Old data\n");
	WriteFile("old.module/mod.cfg", "module_of = world.foliage\nname = Old module\noption.density = Density | low, high | high\n");
	WriteFile("broken/mod.json", "{ this is not json");
	WriteFile("graphics.msaa/settings.cfg", "enabled = on\nsamples = 16x\n");
	ModRegistry registry;
	registry.Discover(_folder);
	registry.LoadSettings();
	registry.ApplyAll();

	ASSERT_EQ(registry.GetModpacks().size(), 1u);
	const auto& pack = registry.GetModpacks().front();
	EXPECT_EQ(pack.name, "Demo pack");
	EXPECT_EQ(pack.mods.size(), 2u);
	EXPECT_EQ(registry.Find("demo.one")->GetInfo().pack, "pack.demo");
	EXPECT_FALSE(registry.IsModpackEnabled(pack));
	registry.SetModpackEnabled(pack, true);
	EXPECT_TRUE(registry.IsModpackEnabled(pack));
	EXPECT_TRUE(std::filesystem::exists(_folder / "pack.demo" / "demo.one" / "settings.cfg"));

	ASSERT_NE(registry.Find("data.old.data"), nullptr);
	EXPECT_EQ(registry.Find("data.old.data")->GetInfo().kind, Mod::Kind::Data);
	const auto* module = registry.Find("old.module");
	ASSERT_NE(module, nullptr);
	EXPECT_EQ(module->GetInfo().parent, "world.foliage");
	EXPECT_EQ(module->GetChoice("density"), "high");
	EXPECT_EQ(registry.GetBroken().size(), 1u);

	// the user's settings of a built-in mod are read from its folder, which has no mod.json
	EXPECT_TRUE(registry.Find("graphics.msaa")->IsEnabled());
	EXPECT_EQ(Locator::config::value().msaa, 16);
}

TEST_F(ModsTest, ModuleNeedsItsParent)
{
	ModRegistry registry;
	registry.Discover(_folder);
	EXPECT_EQ(registry.ApplyArgument("world.foliage.beach"), "");
	registry.ApplyAll();
	const auto* beach = registry.Find("world.foliage.beach");
	ASSERT_NE(beach, nullptr);
	EXPECT_FALSE(registry.IsActive(*beach)); // blocked: world.foliage is off
	EXPECT_TRUE(registry.GetModules("world.foliage").empty());
	EXPECT_EQ(registry.ApplyArgument("world.foliage"), "");
	registry.ApplyAll();
	EXPECT_TRUE(registry.IsActive(*beach));
	const auto modules = registry.GetModules("world.foliage");
	ASSERT_EQ(modules.size(), 1u);
	EXPECT_EQ(modules.front().options.at("density"), "medium");
}

namespace
{
bool LogHas(std::string_view mod, std::string_view text)
{
	return std::ranges::any_of(log::Entries(), [&](const log::Entry& entry) {
		return entry.mod == mod && entry.text.find(text) != std::string::npos;
	});
}
} // namespace

TEST_F(ModsTest, LuaSandboxSwitchesAndInterfaces)
{
	WriteFile("lua.lib/mod.json", R"({"id": "lua.lib", "version": "1.0.0", "entry": {"lua": "scripts/main.lua"}})");
	WriteFile("lua.lib/scripts/main.lua", R"(ob.interfaces.provide("test.math.v1", { double = function(x) return x * 2 end }))");
	WriteFile("lua.user/mod.json", R"({"id": "lua.user", "dependencies": {"lua.lib": "^1.0"}, "entry": {"lua": "scripts/main.lua"},
		"options": [{"id": "word", "values": ["uno", "dos"], "default": "dos"}]})");
	WriteFile("lua.user/scripts/helper.lua", "return { name = 'helper' }");
	WriteFile("lua.user/scripts/main.lua", R"(
		local math2 = ob.interfaces.get("test.math.v1")
		ob.log.info("double of 21 is " .. math2.double(21))
		ob.log.info("helper is " .. require("helper").name .. ", option " .. ob.mod.option("word"))
		turns = 0
		ob.on("turn", function(turn) turns = turns + 1 end)
		ob.on("land_loaded", function(land) ob.log.info("loaded " .. land) end)
	)");
	WriteFile("lua.bad/mod.json", R"({"id": "lua.bad", "entry": {"lua": "scripts/main.lua"}})");
	WriteFile("lua.bad/scripts/main.lua", "ob.on('turn', function() error('boom') end)");

	log::Clear();
	auto& registry = Locator::mods::emplace();
	registry.Discover(_folder);
	for (const auto* id : {"lua.lib", "lua.user", "lua.bad"})
	{
		EXPECT_EQ(registry.ApplyArgument(id), "");
	}
	registry.ApplyAll();
	lua::Start(registry);
	EXPECT_EQ(lua::Running(), 3u);
	EXPECT_TRUE(LogHas("lua.user", "double of 21 is 42"));
	EXPECT_TRUE(LogHas("lua.user", "helper is helper, option dos"));
	EXPECT_NE(api::FindInterface("test.math.v1", "lua"), nullptr);

	// the sandbox: no disk, no OS, no loading of binaries
	EXPECT_EQ(lua::RunForTest("lua.user", "assert(io == nil and package == nil and debug == nil and dofile == nil)"), "");
	EXPECT_EQ(lua::RunForTest("lua.user", "assert(os.execute == nil and os.time ~= nil)"), "");
	// switches set by a script count while the mod is active
	EXPECT_EQ(lua::RunForTest("lua.user", "assert(ob.switch.set('water.living', true))"), "");
	EXPECT_TRUE(Locator::config::value().livingWater);
	EXPECT_NE(lua::RunForTest("lua.user", "ob.switch.set('water.living', 'yes')"), "");

	// events, and a script that fails: logged, never thrown, dropped after 10
	for (uint32_t turn = 0; turn < 12; ++turn)
	{
		lua::OnTurn(turn);
	}
	lua::OnLandLoaded("Land1");
	EXPECT_EQ(lua::RunForTest("lua.user", "assert(turns == 12)"), "");
	EXPECT_TRUE(LogHas("lua.user", "loaded Land1"));
	EXPECT_TRUE(LogHas("lua.bad", "boom"));
	EXPECT_TRUE(LogHas("lua.bad", "event functions are dropped"));

	// the mod off: its script switch goes back to the original
	EXPECT_EQ(registry.ApplyArgument("lua.user=off"), "");
	registry.ApplyAll();
	EXPECT_FALSE(Locator::config::value().livingWater);
	lua::Stop();
	EXPECT_EQ(api::FindInterface("test.math.v1", "lua"), nullptr);
	Locator::mods::reset();
}

// The native examples built next to the tests (mods/examples, copied to bin/<config>/Mods/examples)
TEST_F(ModsTest, NativeExamplesLoadAndTalk)
{
	const auto mods = std::filesystem::current_path() / "Mods";
	if (!std::filesystem::exists(mods / "examples" / "modpack.json"))
	{
		GTEST_SKIP() << "no Mods/examples next to the test";
	}
	log::Clear();
	auto& registry = Locator::mods::emplace();
	registry.Discover(mods);
	ASSERT_NE(registry.FindModpack("examples"), nullptr);
	for (const auto* id : {"example.native-hello", "example.native-library", "example.native-consumer"})
	{
		ASSERT_NE(registry.Find(id), nullptr) << id;
		EXPECT_EQ(registry.ApplyArgument(id), "");
	}
	registry.ApplyAll();
	native::Start(registry);
	EXPECT_EQ(native::Loaded(), 3u);
	EXPECT_TRUE(LogHas("example.native-hello", "hello from C: mod API 1"));
	EXPECT_TRUE(LogHas("example.native-library", "offering example.counter.v1"));
	EXPECT_TRUE(LogHas("example.native-consumer", "using example.counter.v1"));
	native::OnLandLoaded("Land1");
	native::OnLandLoaded("Land2");
	EXPECT_TRUE(LogHas("example.native-consumer", "land Land2 is load number 2"));
	EXPECT_TRUE(LogHas("example.native-hello", "land Land1 loaded"));
	native::Stop();
	EXPECT_TRUE(LogHas("example.native-hello", "goodbye from C"));
	EXPECT_EQ(api::FindInterface("example.counter.v1", "native"), nullptr);

	// the consumer without its library: blocked, not loaded
	EXPECT_EQ(registry.ApplyArgument("example.native-library=off"), "");
	registry.ApplyAll();
	EXPECT_FALSE(registry.IsActive(*registry.Find("example.native-consumer")));
	native::Start(registry);
	EXPECT_EQ(native::Loaded(), 1u);
	native::Stop();
	Locator::mods::reset();
}

TEST_F(ModsTest, ReplacementsAreCollectedAndPatchObjects)
{
	WriteFile("rep.one/mod.json", R"({"id": "rep.one", "replace": {
		"meshes": {"animalbat1": "meshes/bat.l3d", "#3": "meshes/three.l3d", "NoSuchMesh": "meshes/bat.l3d"},
		"textures": {"pack:47": "textures/47.png", "raw:ATMOS": "textures/atmos.png"},
		"objects": {"feature": {"Rock1": {"woodValue": 77, "weight": 2.5, "meshId": "AnimalBat2"}},
		            "nosuchtable": {}}}})");
	WriteFile("rep.one/meshes/bat.l3d", "x");
	WriteFile("rep.one/meshes/three.l3d", "x");
	WriteFile("rep.one/textures/47.png", "x");
	WriteFile("rep.one/textures/atmos.png", "x");
	WriteFile("rep.one/replace/Data/Sky.raw", "x");
	WriteFile("rep.two/mod.json", R"({"id": "rep.two", "load_after": ["rep.one"],
		"replace": {"textures": {"pack:0x47": "textures/mine.png"}}})");
	WriteFile("rep.two/textures/mine.png", "x");

	log::Clear();
	ModRegistry registry;
	registry.Discover(_folder);
	EXPECT_EQ(registry.ApplyArgument("rep.one"), "");
	EXPECT_EQ(registry.ApplyArgument("rep.two"), "");
	registry.ApplyAll();
	replace::Collect(registry);

	ASSERT_TRUE(replace::Mesh(1)); // AnimalBat1, case does not matter
	EXPECT_EQ(replace::Mesh(1)->filename(), "bat.l3d");
	EXPECT_TRUE(replace::Mesh(3));
	EXPECT_FALSE(replace::Mesh(2));
	EXPECT_TRUE(LogHas("rep.one", "there is no mesh 'NoSuchMesh'"));
	// two mods replace pack texture 0x47: the later one wins, and says so
	ASSERT_TRUE(replace::PackTexture(0x47));
	EXPECT_EQ(replace::PackTexture(0x47)->filename(), "mine.png");
	EXPECT_TRUE(LogHas("rep.two", "replaced by rep.one too"));
	EXPECT_TRUE(replace::RawTexture("atmos"));
	ASSERT_EQ(replace::Folders().size(), 1u);
	ASSERT_TRUE(replace::HasObjectPatches());

	auto info = std::make_unique<InfoConstants>();
	const std::string rock = "Rock1";
	std::ranges::copy(rock, info->feature[0].debugString.begin());
	EXPECT_EQ(replace::PatchObjects(*info), 3u);
	EXPECT_EQ(info->feature[0].woodValue, 77u);
	EXPECT_FLOAT_EQ(info->feature[0].weight, 2.5f);
	EXPECT_EQ(info->feature[0].meshId, static_cast<MeshId>(2));
	EXPECT_TRUE(LogHas("rep.one", "there is no table 'nosuchtable'"));
	replace::Clear();
}

TEST_F(ModsTest, WrongTypesNeverThrow)
{
	// nlohmann's value() throws on a wrong type: the mod is unusable, openblack goes on
	for (const auto* json : {R"({"id": "test.t", "restart_required": "yes"})", R"({"id": "test.t", "schema": "1"})",
	                         R"({"id": "test.t", "parent": null})", R"({"id": "test.t", "url": 5})",
	                         R"({"id": "test.t", "options": [{"id": "a", "values": ["x"], "type": 3}]})"})
	{
		ManifestResult result;
		EXPECT_NO_THROW(result = ParseManifest(json, {}, "")) << json;
		EXPECT_FALSE(result.mod) << json;
		EXPECT_FALSE(result.errors.empty()) << json;
	}
	std::vector<std::string> errors;
	EXPECT_NO_THROW(EXPECT_FALSE(ParseModpack(R"({"id": "pack.t", "name": 3, "version": 2})", {}, errors)));
	// a mod with code is always a restart mod
	auto mod = Make(R"({"id": "test.code", "entry": {"lua": "scripts/main.lua"}})");
	EXPECT_TRUE(mod->GetInfo().restartRequired);
}

TEST_F(ModsTest, NonFiniteSwitchValuesAreRefused)
{
	EXPECT_FALSE(switches::Set("world.foliage.distance", std::numeric_limits<double>::quiet_NaN()));
	EXPECT_FALSE(switches::Set("graphics.msaa.samples", std::numeric_limits<double>::infinity()));
	EXPECT_FLOAT_EQ(Locator::config::value().foliageDistance, 200.0f);
	EXPECT_EQ(Locator::config::value().msaa, 0);
}

TEST_F(ModsTest, LuaSandboxEdges)
{
	WriteFile("lua.edge/mod.json", R"({"id": "lua.edge", "entry": {"lua": "scripts/main.lua"}})");
	WriteFile("lua.edge/scripts/main.lua", R"(
		count = 0
		ob.on("turn", function() count = count + 1 end)
		string.format = function() return "mine" end
	)");
	WriteFile("lua.other/mod.json", R"({"id": "lua.other", "entry": {"lua": "scripts/main.lua"}})");
	WriteFile("lua.other/scripts/main.lua", "x = 1");
	log::Clear();
	auto& registry = Locator::mods::emplace();
	registry.Discover(_folder);
	EXPECT_EQ(registry.ApplyArgument("lua.edge"), "");
	EXPECT_EQ(registry.ApplyArgument("lua.other"), "");
	registry.ApplyAll();
	lua::Start(registry);
	// its own string library: the other mod still has the real string.format
	EXPECT_EQ(lua::RunForTest("lua.other", "assert(string.format('%d', 5) == '5' and string.dump == nil)"), "");
	// require cannot leave the mod's scripts/
	EXPECT_NE(lua::RunForTest("lua.edge", "require('../../x')"), "");
	EXPECT_NE(lua::RunForTest("lua.edge", "require('C:/Windows/x')"), "");
	// a runaway loop is cut, the game goes on
	EXPECT_NE(lua::RunForTest("lua.edge", "while true do end"), "");
	// switched off: no more events
	lua::OnTurn(1);
	EXPECT_EQ(registry.ApplyArgument("lua.edge=off"), "");
	registry.ApplyAll();
	lua::OnTurn(2);
	EXPECT_EQ(lua::RunForTest("lua.edge", "assert(count == 1)"), "");
	lua::Stop();
	Locator::mods::reset();
}

TEST_F(ModsTest, ObjectPatchEdges)
{
	WriteFile("rep.abode/mod.json", R"({"id": "rep.abode", "replace": {"objects": {
		"abode": {"NORSE_Hut": {"woodValue": 9}},
		"feature": {"Rock1": {"woodValue": -5}}}}})");
	log::Clear();
	ModRegistry registry;
	registry.Discover(_folder);
	EXPECT_EQ(registry.ApplyArgument("rep.abode"), "");
	registry.ApplyAll();
	replace::Collect(registry);
	auto info = std::make_unique<InfoConstants>();
	const std::string hut = "Hut";
	std::ranges::copy(hut, info->abode[0].debugString.begin());
	std::ranges::copy(hut, info->abode[1].debugString.begin());
	info->abode[0].tribeType = Tribe::NORSE;
	info->abode[1].tribeType = Tribe::CELTIC;
	const std::string rock = "Rock1";
	std::ranges::copy(rock, info->feature[0].debugString.begin());
	EXPECT_EQ(replace::PatchObjects(*info), 1u);
	EXPECT_EQ(info->abode[0].woodValue, 9u); // the Norse one only
	EXPECT_EQ(info->abode[1].woodValue, 0u);
	EXPECT_EQ(info->feature[0].woodValue, 0u); // a negative unsigned value is refused
	EXPECT_TRUE(LogHas("rep.abode", "woodValue must be 0 or more"));
	replace::Clear();
}

TEST_F(ModsTest, RestartIsAskedOnlyForRestartMods)
{
	ModRegistry registry;
	registry.Discover(_folder);
	registry.ApplyAll();
	registry.MarkStarted();
	EXPECT_TRUE(registry.PendingRestart().empty());
	// a live mod: nothing to restart
	registry.SetEnabled(*registry.Find("water.living"), true);
	EXPECT_TRUE(registry.PendingRestart().empty());
	// a restart mod switched on, then back: pending, then not
	registry.SetEnabled(*registry.Find("graphics.mipmaps"), true);
	ASSERT_EQ(registry.PendingRestart().size(), 1u);
	EXPECT_EQ(registry.PendingRestart().front()->GetInfo().id, "graphics.mipmaps");
	registry.SetEnabled(*registry.Find("graphics.mipmaps"), false);
	EXPECT_TRUE(registry.PendingRestart().empty());
	// an option of a restart mod
	registry.SetOption(*registry.Find("game.skip-intro"), 0, 0);
	EXPECT_EQ(registry.PendingRestart().size(), 1u);
}


TEST(ModRuleFiles, JsonGivesTheCfgLines)
{
	std::string error;
	const auto cfg = rule_files::JsonToCfg(R"(// a comment
		{"schema": 1, "rules": [
			{"section": "grass", "images": ["a.png", "b.png"], "per_cell": 90, "size": "0.68-1.2", "lean": 0.6, "cross": true},
			{"section": "field_stage brote", "colour": "90,120,40 - 120,150,60"}
		]})",
	                                         error);
	ASSERT_TRUE(cfg) << error;
	EXPECT_EQ(*cfg, "[grass]\nimages = a.png, b.png\nper_cell = 90\nsize = 0.68-1.2\nlean = 0.6\ncross = on\n\n"
	                "[field_stage brote]\ncolour = 90,120,40 - 120,150,60\n\n");
	const auto textures = rule_files::JsonToCfg(R"({"schema": 1, "textures": {"2": "c814f509", "1a": "00000001"}})", error);
	ASSERT_TRUE(textures) << error;
	EXPECT_EQ(*textures, "2 = c814f509\n1a = 00000001\n");
	EXPECT_FALSE(rule_files::JsonToCfg("{ not json", error));
	EXPECT_FALSE(rule_files::JsonToCfg(R"({"rules": [{"images": []}]})", error));
}

// The rule files that come with openblack are JSON now and read back as rules
TEST(ModRuleFiles, BuiltinRuleFilesRead)
{
	const auto mods = std::filesystem::current_path() / "Mods";
	if (!std::filesystem::exists(mods / "world.foliage" / "foliage.json"))
	{
		GTEST_SKIP() << "no Mods next to the test";
	}
	for (const auto* folder : {"world.foliage", "world.foliage.beach", "world.foliage.butterflies"})
	{
		std::filesystem::path used;
		std::string error;
		const auto rules = rule_files::Read(mods / folder, "foliage", used, error);
		ASSERT_TRUE(rules) << folder << ": " << error;
		EXPECT_EQ(used.filename(), "foliage.json");
		EXPECT_NE(rules->find('['), std::string::npos) << folder;
	}
	std::filesystem::path used;
	std::string error;
	const auto textures = rule_files::Read(mods / "graphics.hd-tweaks", "textures", used, error);
	ASSERT_TRUE(textures) << error;
	EXPECT_NE(textures->find(" = "), std::string::npos);
}

// API 1.1: the game's own geometry, the same numbers in C++, Lua and the C API
TEST_F(ModsTest, GeometryApiMatchesTheGame)
{
	EXPECT_FLOAT_EQ(api::Distance(0, 0, 30, 40), gutils::GetDistanceInMetres(glm::vec2(0, 0), glm::vec2(30, 40)));
	// GetDistanceInMetres 0x74CD70 goes through 16.16 fixed point: not exactly the float hypotenuse
	EXPECT_NEAR(api::Distance(0, 0, 0.2f, 0.2f), 0.2828f, 0.01f);
	EXPECT_EQ(api::AngleBetween(0, 0, 10, 0), gutils::GetAngleFromXZ(glm::vec2(0, 0), glm::vec2(10, 0)));
	const auto cell = api::CellAt(1434, 2233);
	EXPECT_EQ(cell.x, 143);
	EXPECT_EQ(cell.z, 223);
	EXPECT_TRUE(cell.inMap);
	EXPECT_FALSE(api::CellAt(-5, 10).inMap);
	const auto [px, pz] = api::PointAtAngle(100, 100, 0, 10);
	EXPECT_NEAR(std::hypot(px - 100, pz - 100), 10.0f, 0.01f);
	EXPECT_EQ(api::RadiansToAngle(api::AngleToRadians(512)), 512);
	EXPECT_FALSE(api::MeshRadius("AnimalBat1", 1.0f)); // no meshes loaded in the test
	EXPECT_FALSE(api::MeshRadius("NoSuchMesh", 1.0f));

	WriteFile("lua.geo/mod.json", R"({"id": "lua.geo", "entry": {"lua": "scripts/main.lua"}})");
	WriteFile("lua.geo/scripts/main.lua", "x = 1");
	auto& registry = Locator::mods::emplace();
	registry.Discover(_folder);
	EXPECT_EQ(registry.ApplyArgument("lua.geo"), "");
	registry.ApplyAll();
	lua::Start(registry);
	EXPECT_EQ(lua::RunForTest("lua.geo", R"(
		assert(math.abs(ob.map.distance(0, 0, 30, 40) - 50) < 0.5)
		local cx, cz, inside = ob.map.cell(1434, 2233)
		assert(cx == 143 and cz == 223 and inside)
		assert(ob.map.radians_to_angle(ob.map.angle_to_radians(512)) == 512)
		assert(ob.mesh.radius("AnimalBat1") == nil)
		assert(ob.api_version == "1.2.0")
		assert(type(ob.game.turn_fraction()) == "number" and type(ob.game.paused()) == "boolean")
	)"),
	          "");
	lua::Stop();
	Locator::mods::reset();
}

// API 1.2: sound, through Audio.h; without the audio running nothing plays and nothing breaks
TEST_F(ModsTest, SoundApiIsSafeWithoutAudio)
{
	const auto banks = api::Enumeration("sound_banks");
	EXPECT_TRUE(std::ranges::any_of(banks, [](const auto& bank) { return bank.first == "ScriptSfx"; }));
	EXPECT_TRUE(std::ranges::any_of(banks, [](const auto& bank) { return bank.first == "InGame"; }));
	auto mod = Make(R"({"id": "test.sound"})");
	EXPECT_FALSE(api::PlaySound(*mod, "NoSuchBank", "1", nullptr));
	EXPECT_FALSE(api::PlaySound(*mod, "InGame", "G_PickUpFood.wav", nullptr)); // no banks loaded in the test
	api::StopSounds(*mod); // nothing to stop: fine

	WriteFile("lua.snd/mod.json", R"({"id": "lua.snd", "entry": {"lua": "scripts/main.lua"}})");
	WriteFile("lua.snd/scripts/main.lua", "x = 1");
	auto& registry = Locator::mods::emplace();
	registry.Discover(_folder);
	EXPECT_EQ(registry.ApplyArgument("lua.snd"), "");
	registry.ApplyAll();
	lua::Start(registry);
	EXPECT_EQ(lua::RunForTest("lua.snd", R"(
		assert(ob.sound.play("InGame", "G_PickUpFood.wav") == false)
		assert(ob.sound.play("InGame", 3, 1700, 10, 2000) == false)
		ob.sound.stop()
		assert(ob.api_version == "1.2.0")
		assert(ob.enums.sound_banks.ScriptSfx ~= nil)
	)"),
	          "");
	lua::Stop();
	Locator::mods::reset();
}
