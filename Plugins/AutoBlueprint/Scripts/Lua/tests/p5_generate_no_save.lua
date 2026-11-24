local api = require("auto_blueprint")
local module = {}

function module.run(arguments)
    assert(#arguments == 1, "INVALID_ARGUMENTS: expected one layout path")
    local options = UE.FAutoBlueprintBuildOptions()
    options.bSave = false
    options.bWriteReport = false
    local result = api.generateTaskFromFile(arguments[1], options)
    assert(result.bSuccess and result.Widget.bCompiled and not result.Widget.bSaved)
    assert(result.ReportFilePath == "", "Disabled report unexpectedly written")
    local resources = result.Resources:ToTable()
    assert(#resources == 1 and resources[1].bImported and not resources[1].bSaved)
    return true
end

return module
