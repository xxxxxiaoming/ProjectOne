-- Standalone mocks verify Lua orchestration only. Real reflection: tests/p4_generate.lua.
local root = arg[1] or "Plugins/AutoBlueprint"
local function array(values)
    return { ToTable = function() return values end }
end
local function diagnostic(code)
    return { Code = code, JsonPath = "$.resources.photo", Message = "fixture failure" }
end
local function resource(alias, reused, saved)
    return { Alias = alias, SourceFile = reused and "" or "Content/UI/source.png",
        TargetAssetPath = "/Game/UI/T_" .. alias .. ".T_" .. alias,
        CreatedPackageName = reused and "" or "/Game/UI/T_" .. alias,
        bAttempted = true, bReused = reused, bImported = not reused,
        bSaved = saved, bSuccess = true, Diagnostics = array({}) }
end
local function task(save)
    return { bSuccess = true, Resources = array({ resource("existing", true, false), resource("new", false, save) }),
        Widget = { bSuccess = true, bAssetCreated = true, bCompiled = true, bSaved = save,
            TargetAssetPath = "/Game/UI/WBP_Test.WBP_Test", CreatedPackageName = "/Game/UI/WBP_Test",
            ReportFilePath = "", Diagnostics = array({}) },
        Diagnostics = array({}), ReportFilePath = "Saved/AutoBlueprint/Reports/test.json" }
end
local calls, output = {}, {}
local result = task(true)
local valid = true
local originalPrint = print
print = function(value) output[#output + 1] = value end
UE = {
    FAutoBlueprintBuildOptions = function() return { bSave = true, bWriteReport = true } end,
    UAutoBlueprintLibrary = {
        ValidateLayoutFile = function(path)
            calls[#calls + 1] = { "validate", path }
            return { bValid = valid, Diagnostics = array(valid and {} or { diagnostic("INVALID_IMAGE") }) }
        end,
        GenerateFromLayoutFile = function(path, options)
            calls[#calls + 1] = { "generate", path, options }
            return result
        end
    }
}
local api = assert(loadfile(root .. "/Scripts/Lua/auto_blueprint.lua"))()
local path = "UIBlueprintLayouts/p4.json"
assert(api.generateTaskFromFile(path) == result)
assert(#calls == 2 and calls[1][1] == "validate" and calls[2][1] == "generate")
assert(calls[2][2] == path and calls[2][3].bSave and calls[2][3].bWriteReport)
assert(table.concat(output, "\n"):find("Resource existing [reused]", 1, true))

result = task(false)
local options = { bSave = false, bWriteReport = false }
assert(api.generateTaskFromFile(path, options) == result and calls[#calls][3] == options)

valid = false
local before = #calls
local ok, message = pcall(api.generateTaskFromFile, path)
assert(not ok and message:find("INVALID_IMAGE", 1, true) and #calls == before + 1)
valid = true

-- Earlier saved packages must still be printed when a later resource fails.
result = task(false)
result.bSuccess = false
result.Widget.bSuccess = false
result.Resources = array({ resource("first", false, true), resource("second", false, false),
    { Alias = "third", SourceFile = "", TargetAssetPath = "/Game/UI/T_third.T_third", CreatedPackageName = "",
      bAttempted = false, bReused = false, bImported = false, bSaved = false, bSuccess = false, Diagnostics = array({}) } })
result.Resources:ToTable()[2].bSuccess = false
result.Resources:ToTable()[2].Diagnostics = array({ diagnostic("PACKAGE_SAVE_FAILED") })
output = {}
ok, message = pcall(api.generateTaskFromFile, path)
local log = table.concat(output, "\n")
assert(not ok and message:find("PACKAGE_SAVE_FAILED", 1, true))
assert(log:find("Saved package: /Game/UI/T_first", 1, true))
assert(log:find("Unsaved package: /Game/UI/T_second", 1, true))
assert(log:find("Unsaved package: /Game/UI/WBP_Test", 1, true))
assert(log:find("Resource third [not attempted]", 1, true))
assert(not log:find("Generated:", 1, true))

result = task(true)
result.Resources:ToTable()[1].bSaved = true
assert(not pcall(api.generateTaskFromFile, path), "Reused asset must never claim a save")
before = #calls
assert(not pcall(api.generateTaskFromFile, ""))
assert(not pcall(api.generateTaskFromFile, path, { bSave = "true" }))
assert(#calls == before)

-- The actual console entry uses the resource-aware task facade.
result = task(true)
package.loaded.auto_blueprint = api
local entry = assert(loadfile(root .. "/Scripts/Lua/generate_from_file.lua"))()
entry.run({ path })
assert(calls[#calls][1] == "generate")
print = originalPrint
print("PASS: P4 task routing, nested states, reuse/no-save, partial failures, diagnostics and console entry")
