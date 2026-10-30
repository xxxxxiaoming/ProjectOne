-- Deliberate failure used to verify traceback propagation and VM cleanup.
local module = {}

local function failFromNestedCall()
    error("AUTOBLUEPRINT_P0_EXPECTED_ERROR")
end

function module.run(packagePath)
    failFromNestedCall()
    return true
end

return module
