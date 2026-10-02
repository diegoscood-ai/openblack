-- A library mod in Lua: it offers a table under an interface name; mods that list it in "dependencies" load after it
-- and get the table with ob.interfaces.get("example.places.v1").

local places = {
	-- Land 1 cameras of the wiki (mod-library.md, "Ganchos de prueba"): x, z on the ground
	beach = { x = 1706, z = 2004 },
	meadow = { x = 1434, z = 2233 },
	citadel = { x = 1915, z = 2508 },
}

local library = {}

--- The named places
function library.names()
	local names = {}
	for name in pairs(places) do names[#names + 1] = name end
	table.sort(names)
	return names
end

--- x, y, z of a place on the ground (nil if no such place or no land)
function library.where(name)
	local place = places[name]
	if not place then return nil end
	local y = ob.game.ground_height(place.x, place.z)
	if not y then return nil end
	return place.x, y, place.z
end

ob.interfaces.provide("example.places.v1", library)
ob.log.info("offering example.places.v1 with " .. #library.names() .. " places")
