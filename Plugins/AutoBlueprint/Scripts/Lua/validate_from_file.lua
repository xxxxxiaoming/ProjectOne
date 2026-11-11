local api = require("auto_blueprint")
local module = {}
function module.run(arguments)
    assert(#arguments == 1, "INVALID_ARGUMENTS: expected one project-relative layout path")
    local result = api.validateLayoutFile(arguments[1])
    for _, item in ipairs(result.Diagnostics:ToTable()) do
        print(item.Code .. ": " .. item.JsonPath .. ": " .. item.Message)
    end
    assert(result.bValid, "Layout validation failed; no assets were created")
    print("Layout valid: " .. result.PlannedAssetPath .. " (read-only)")
    return true
end
return module
