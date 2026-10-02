-- Uses the library mod example.lua-library (it is in our "dependencies", so it has loaded first)
local places = ob.interfaces.get("example.places.v1")
if not places then
	error("example.places.v1 is not there")
end

ob.on("land_loaded", function(land)
	for _, name in ipairs(places.names()) do
		local x, y, z = places.where(name)
		if x then
			ob.log.info(string.format("%s: %s is at %.0f, %.1f, %.0f", land, name, x, y, z))
		end
	end
	-- API 1.1: distances and angles as the game measures them
	local bx, _, bz = places.where("beach")
	local cx, _, cz = places.where("citadel")
	if bx and cx then
		ob.log.info(string.format("from the beach to the citadel: %.1f m, game angle %d", ob.map.distance(bx, bz, cx, cz),
			ob.map.angle(bx, bz, cx, cz)))
	end
end)
