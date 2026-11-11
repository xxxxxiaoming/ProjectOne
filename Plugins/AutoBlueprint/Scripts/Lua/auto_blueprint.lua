-- Shared editor tool API; reflected C++ names are preserved.
local module = {}

local function formatDiagnostics(diagnostics)
    local messages = {}
    for _, item in ipairs(diagnostics:ToTable()) do
        messages[#messages + 1] = item.Code .. ": " .. item.Message
    end
    return table.concat(messages, "\n")
end

function module.reportBuildResult(result, saveRequested)
    assert(type(saveRequested) == "boolean", "saveRequested must be a Lua boolean")
    -- Exercise the single-UI contract through real reflected fields.
    assert(type(result.bSuccess) == "boolean", "bSuccess must be a Lua boolean")
    assert(type(result.bAssetCreated) == "boolean", "bAssetCreated must be a Lua boolean")
    assert(type(result.bCompiled) == "boolean", "bCompiled must be a Lua boolean")
    assert(type(result.bSaved) == "boolean", "bSaved must be a Lua boolean")
    assert(type(result.TargetAssetPath) == "string", "TargetAssetPath must be a Lua string")
    assert(type(result.CreatedPackageName) == "string", "CreatedPackageName must be a Lua string")

    if result.bAssetCreated then
        print("Created asset: " .. result.TargetAssetPath)
    end
    -- A package may exist even when Widget Blueprint creation failed.
    if result.CreatedPackageName ~= "" then
        local label = result.bSaved and "Saved package: " or "Unsaved package: "
        print(label .. result.CreatedPackageName)
    end

    local diagnostics = formatDiagnostics(result.Diagnostics)
    if not result.bSuccess then
        error("Widget generation failed:\n" .. diagnostics)
    end
    assert(result.bAssetCreated, "Widget generation returned without creating the target asset")
    assert(result.TargetAssetPath ~= "", "Widget generation returned without a target asset path")
    assert(result.CreatedPackageName ~= "", "Widget generation returned without a created package")
    assert(result.bCompiled, "Widget generation returned without a successful compile")
    if saveRequested then
        assert(result.bSaved, "Widget generation returned without saving the requested package")
    else
        assert(not result.bSaved, "Widget generation unexpectedly saved a package")
    end
    if diagnostics ~= "" then
        print(diagnostics)
    end
    print("Generated: " .. result.TargetAssetPath)
    return result
end

function module.buildSmokeTest(request)
    local result = UE.UAutoBlueprintLibrary.BuildSmokeTestWidget(request)
    return module.reportBuildResult(result, request.bSave)
end

function module.validateLayout(layoutJson)
    assert(type(layoutJson) == "string", "INVALID_ARGUMENTS: expected layout JSON")
    return UE.UAutoBlueprintLibrary.ValidateLayout(layoutJson)
end

function module.validateLayoutFile(layoutPath)
    assert(type(layoutPath) == "string" and layoutPath ~= "", "INVALID_ARGUMENTS: expected layout path")
    return UE.UAutoBlueprintLibrary.ValidateLayoutFile(layoutPath)
end

function module.generateFromFile(layoutPath)
    assert(type(layoutPath) == "string" and layoutPath ~= "",
        "INVALID_ARGUMENTS: expected a project-relative layout file path")
    -- P2 provides read-only validation; actual layout construction belongs to P3.
    -- Never substitute the fixed P0 tree for a requested layout.
    error("NOT_IMPLEMENTED: layout-file generation requires P2/P3. No layout was read and no asset was created.", 2)
end

return module
