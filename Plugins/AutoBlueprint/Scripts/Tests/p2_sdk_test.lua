-- Lua-only facade wiring and invalid-result propagation; real reflection needs UE.
local root = arg[1] or "Plugins/AutoBlueprint"
local calls = {}
local invalid = { bValid = false, Diagnostics = { ToTable = function() return {} end } }
UE = { UAutoBlueprintLibrary = {
    ValidateLayout = function(value) calls.json = value; return invalid end,
    ValidateLayoutFile = function(value) calls.file = value; return invalid end,
} }
local api = assert(loadfile(root .. "/Scripts/Lua/auto_blueprint.lua"))()
assert(api.validateLayout('{"schema_version":2}') == invalid)
assert(calls.json == '{"schema_version":2}')
assert(api.validateLayoutFile("UIBlueprintLayouts/a.json") == invalid)
assert(calls.file == "UIBlueprintLayouts/a.json")
assert(not pcall(api.validateLayout, {}))
assert(not pcall(api.validateLayoutFile, ""))
assert(not pcall(api.generateFromFile, "UIBlueprintLayouts/a.json"))
print("PASS: P2 Lua validation forwarding, invalid result propagation and generation boundary")
