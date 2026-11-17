local api = require("auto_blueprint")
local module = {}

function module.run(arguments)
    assert(#arguments == 1, "INVALID_ARGUMENTS: expected one layout path")
    local options = UE.FAutoBlueprintBuildOptions()
    assert(options.bSave == true and options.bWriteReport == true, "Unexpected reflected option defaults")
    options.bSave = false
    options.bWriteReport = false
    local result = api.generateFromFile(arguments[1], options)
    assert(result.bSuccess and result.bAssetCreated and result.bCompiled and not result.bSaved)
    assert(result.ReportFilePath == "", "Disabled report unexpectedly written")
    return true
end

return module
