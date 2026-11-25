-- Native P6 fixture: mix rejected, saved and unsaved calls, then optionally throw.
local module = {}

function module.run(arguments)
    assert(#arguments == 3, "P6 expects layout path, fresh smoke package and throw/return")
    assert(AutoBlueprintP6Marker == nil, "A previous task leaked globals")
    AutoBlueprintP6Marker = true
    assert(require("tests.module_counter").loads == 1)

    local nested = UE.FAutoBlueprintLuaRunRequest()
    nested.ScriptPath = "examples/hello.lua"
    local rejected = UE.UAutoBlueprintLibrary.RunEditorScript(nested)
    assert(not rejected.bSuccess and rejected.Code == "EDITOR_BUSY")
    assert(#rejected.Operations:ToTable() == 0, "Nested request inherited the outer journal")

    local noSave = UE.FAutoBlueprintBuildOptions()
    noSave.bSave = false
    noSave.bWriteReport = false
    local legacy = UE.UAutoBlueprintLibrary.BuildFromLayoutFile(arguments[1], noSave)
    assert(not legacy.bSuccess and not legacy.bAssetCreated)

    local options = UE.FAutoBlueprintBuildOptions()
    local task = UE.UAutoBlueprintLibrary.GenerateFromLayoutFile(arguments[1], options)
    assert(task.Widget.bAssetCreated and task.Widget.bCompiled)

    local smoke = UE.FAutoBlueprintSmokeTestRequest()
    smoke.PackagePath = arguments[2]
    smoke.bSave = false
    local built = UE.UAutoBlueprintLibrary.BuildSmokeTestWidget(smoke)
    assert(built.bSuccess and built.bCompiled and not built.bSaved)

    assert(not UE.UAutoBlueprintLibrary.BuildWidgetBlueprint("{}", noSave).bSuccess)
    assert(not UE.UAutoBlueprintLibrary.GenerateFromLayout("{}", noSave).bSuccess)
    if arguments[3] == "throw" then
        error("AUTOBLUEPRINT_P6_AFTER_GENERATION")
    end
    assert(arguments[3] == "return")
    return true
end

return module
