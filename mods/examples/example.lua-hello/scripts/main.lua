-- Lua hello: what a Lua mod can do (openblack mod API 1). Everything it writes goes to the Mods window, tab Log.

local util = require("util") -- scripts/util.lua of this mod

ob.log.info(util.greet(ob.mod.option("greeting")) .. " from " .. ob.mod.name .. " " .. ob.mod.version)

-- enumerations: names the engine knows
local meshes = 0
for _ in pairs(ob.enums.meshes) do meshes = meshes + 1 end
ob.log.info("openblack knows " .. meshes .. " mesh names; AnimalBat1 is mesh #" .. tostring(ob.enums.meshes.AnimalBat1))

-- engine switches: read them (a mod that changes one uses ob.switch.set(name, value))
ob.log.info("living water switch: " .. tostring(ob.switch.get("water.living")))

ob.on("land_loaded", function(land)
	local magic = 0
	for _ in pairs(ob.enums.magic) do magic = magic + 1 end
	ob.log.info("land " .. land .. " loaded, with " .. magic .. " miracles in info.dat")
end)

ob.on("turn", function(turn)
	if turn % 100 == 0 then
		local x, y, z, fx, fy, fz = ob.game.camera()
		local ground = ob.game.ground_height(fx, fz)
		ob.log.info(string.format("turn %d, hour %.2f, ground under the camera's focus at %.1f m", turn, ob.game.hour(),
			ground or -1))
	end
end)
