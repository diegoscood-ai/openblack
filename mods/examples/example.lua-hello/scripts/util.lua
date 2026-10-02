-- A module of the mod, loaded with require("util"): it returns its table, as any Lua module
local util = {}

function util.greet(word)
	return word:sub(1, 1):upper() .. word:sub(2)
end

return util
