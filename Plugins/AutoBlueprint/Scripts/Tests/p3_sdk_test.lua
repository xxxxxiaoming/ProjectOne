-- Run with Lua 5.4 from the repository root. Mocks UE; does not test reflection or assets.
local root = arg[1] or "Plugins/AutoBlueprint"
local function array(values)
    return { ToTable = function() return values end }
end
local function outcome(success, created, compiled, saved, code)
    return {
        bSuccess = success, bAssetCreated = created, bCompiled = compiled, bSaved = saved,
        TargetAssetPath = "/Game/UI/WBP_Test.WBP_Test", CreatedPackageName = created and "/Game/UI/WBP_Test" or "",
        ReportFilePath = "Saved/AutoBlueprint/Reports/test.json",
        Diagnostics = array(code and { { Code = code, JsonPath = "$.root", Message = "fixture failure" } } or {})
    }
end
local valid = true
local result = outcome(true, true, true, true)
local calls, output = {}, {}
local originalPrint = print
print = function(value) output[#output + 1] = value end
UE = {
    FAutoBlueprintBuildOptions = function() return { bSave = true, bWriteReport = true } end,
    UAutoBlueprintLibrary = {
        ValidateLayoutFile = function(path)
            calls[#calls + 1] = { "validate", path }
            return { bValid = valid, Diagnostics = array(valid and {} or {
                { Code = "LAYOUT_READ_FAILED", JsonPath = "$", Message = "fixture file missing" }
            }) }
        end,
        BuildFromLayoutFile = function(path, options)
            calls[#calls + 1] = { "build", path, options }
            return result
        end
    }
}
local api = assert(loadfile(root .. "/Scripts/Lua/auto_blueprint.lua"))()
local path = "UIBlueprintLayouts/with spaces.json"
assert(api.generateFromFile(path) == result)
assert(#calls == 2 and calls[1][1] == "validate" and calls[2][1] == "build")
assert(calls[2][2] == path and calls[2][3].bSave and calls[2][3].bWriteReport)
assert(table.concat(output, "\n"):find("Build report:", 1, true))

local options = { bSave = false, bWriteReport = false }
result = outcome(true, true, true, false)
assert(api.generateFromFile(path, options) == result)
assert(calls[#calls][3] == options)

valid = false
local previous = #calls
local ok, message = pcall(api.generateFromFile, path)
assert(not ok and message:find("LAYOUT_READ_FAILED: $:", 1, true))
assert(#calls == previous + 1 and calls[#calls][1] == "validate")

valid = true
for _, case in ipairs({
    { "ASSET_ALREADY_EXISTS", false, false },
    { "BUILD_WIDGET_NOT_IMPLEMENTED", false, false },
    { "BLUEPRINT_COMPILE_FAILED", true, false },
    { "PACKAGE_SAVE_FAILED", true, true }
}) do
    result = outcome(false, case[2], case[3], false, case[1])
    output = {}
    ok, message = pcall(api.generateFromFile, path)
    assert(not ok and message:find(case[1], 1, true) and message:find("$.root", 1, true))
    assert(not table.concat(output, "\n"):find("Generated:", 1, true))
    if case[2] then
        assert(table.concat(output, "\n"):find("Unsaved package:", 1, true))
    end
end

previous = #calls
assert(not pcall(api.generateFromFile, ""))
assert(not pcall(api.generateFromFile, path, { bSave = "false" }))
assert(#calls == previous)
print = originalPrint
print("PASS: P3 Lua preflight/build ordering, option forwarding, saved/unsaved results, four failure stages and invalid arguments")
