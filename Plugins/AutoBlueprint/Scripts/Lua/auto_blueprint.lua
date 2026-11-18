-- Shared editor tool API; reflected C++ names are preserved.
local module = {}

local function formatDiagnostics(diagnostics)
    local messages = {}
    for _, item in ipairs(diagnostics:ToTable()) do
        local location = item.JsonPath and item.JsonPath ~= "" and (item.JsonPath .. ": ") or ""
        messages[#messages + 1] = item.Code .. ": " .. location .. item.Message
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

    if result.ReportFilePath and result.ReportFilePath ~= "" then
        print("Build report: " .. result.ReportFilePath)
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

function module.generateFromFile(layoutPath, options)
    assert(type(layoutPath) == "string" and layoutPath ~= "",
        "INVALID_ARGUMENTS: expected a project-relative layout file path")
    options = options or UE.FAutoBlueprintBuildOptions()
    assert(type(options.bSave) == "boolean", "bSave must be a Lua boolean")
    -- This preview is read-only. C++ repeats validation immediately before writing.
    local validation = module.validateLayoutFile(layoutPath)
    if not validation.bValid then
        error("Layout validation failed:\n" .. formatDiagnostics(validation.Diagnostics), 2)
    end
    local result = UE.UAutoBlueprintLibrary.BuildFromLayoutFile(layoutPath, options)
    return module.reportBuildResult(result, options.bSave)
end

-- P4 task API: one Widget result plus independently reported texture dependencies.
function module.reportGenerationResult(result, saveRequested)
    assert(type(result.bSuccess) == "boolean", "task bSuccess must be a Lua boolean")
    local messages = { formatDiagnostics(result.Diagnostics), formatDiagnostics(result.Widget.Diagnostics) }
    for _, item in ipairs(result.Resources:ToTable()) do
        assert(type(item.bAttempted) == "boolean" and type(item.bReused) == "boolean"
            and type(item.bImported) == "boolean" and type(item.bSaved) == "boolean"
            and type(item.bSuccess) == "boolean", "resource states must be Lua booleans")
        assert(type(item.Alias) == "string" and type(item.TargetAssetPath) == "string"
            and type(item.CreatedPackageName) == "string", "resource paths must be Lua strings")
        local state = not item.bAttempted and "not attempted" or item.bReused and "reused"
            or item.bSaved and "saved" or item.bImported and "imported, unsaved" or "failed"
        print("Resource " .. item.Alias .. " [" .. state .. "]: " .. item.TargetAssetPath)
        if item.CreatedPackageName ~= "" then
            print((item.bSaved and "Saved package: " or "Unsaved package: ") .. item.CreatedPackageName)
        end
        messages[#messages + 1] = formatDiagnostics(item.Diagnostics)
        if result.bSuccess then
            assert(item.bSuccess and item.bAttempted, "successful task contains an incomplete resource")
            assert(item.bReused ~= item.bImported, "resource must be reused or imported")
            assert(item.bSaved == (item.bImported and saveRequested), "unexpected resource save state")
        end
    end
    if result.ReportFilePath ~= "" then
        print("Generation report: " .. result.ReportFilePath)
    end
    if not result.bSuccess then
        if result.Widget.CreatedPackageName ~= "" then
            print((result.Widget.bSaved and "Saved package: " or "Unsaved package: ") .. result.Widget.CreatedPackageName)
        end
        error("Generation task failed:\n" .. table.concat(messages, "\n"), 2)
    end
    module.reportBuildResult(result.Widget, saveRequested)
    return result
end

function module.generateTaskFromFile(layoutPath, options)
    assert(type(layoutPath) == "string" and layoutPath ~= "",
        "INVALID_ARGUMENTS: expected a project-relative layout file path")
    options = options or UE.FAutoBlueprintBuildOptions()
    assert(type(options.bSave) == "boolean", "bSave must be a Lua boolean")
    local validation = module.validateLayoutFile(layoutPath)
    if not validation.bValid then
        error("Layout validation failed:\n" .. formatDiagnostics(validation.Diagnostics), 2)
    end
    local result = UE.UAutoBlueprintLibrary.GenerateFromLayoutFile(layoutPath, options)
    return module.reportGenerationResult(result, options.bSave)
end

return module
