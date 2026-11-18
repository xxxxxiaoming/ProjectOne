local api = require("auto_blueprint")
local module = {}

function module.run(arguments)
    assert(#arguments == 1, "INVALID_ARGUMENTS: expected one layout path")
    local result = api.generateTaskFromFile(arguments[1])
    assert(result.bSuccess and result.Widget.bSaved and result.Widget.bCompiled)
    local resources = result.Resources:ToTable()
    assert(#resources == 2, "Expected PNG and JPEG import results")
    for _, item in ipairs(resources) do
        assert(item.bImported and item.bSaved and item.bSuccess and not item.bReused)
        assert(item.SourceFile ~= "" and item.CreatedPackageName ~= "")
    end
    assert(result.ReportFilePath ~= "", "Expected generation report")
    return true
end

return module
