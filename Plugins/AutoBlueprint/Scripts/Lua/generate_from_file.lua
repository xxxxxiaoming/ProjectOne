local api = require("auto_blueprint")
local module = {}

function module.run(arguments)
    assert(#arguments == 1, "INVALID_ARGUMENTS: generate_from_file.lua requires one project-relative layout path")
    api.generateFromFile(arguments[1])
    return true
end

return module
