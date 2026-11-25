-- Offline bootstrap/fixture checks with mock UE. Native journals and asset writes require P6 UE tests.
local plugin = arg[1] or "Plugins/AutoBlueprint"
local function read(path)
    local file = assert(io.open(plugin .. "/" .. path, "rb"))
    local value = file:read("a")
    file:close()
    return value
end
local runner = read("Source/AutoBlueprintEditor/Private/Scripting/EditorLuaRunner.cpp")
local bootstrap = assert(runner:match('TEXT%(R"lua%((.-)%)lua"%)'))
local fixture = read("Scripts/Lua/tests/p6_outcomes.lua")
local counter = read("Scripts/Lua/tests/module_counter.lua")

local function execute(arguments, partial, compileFailure)
    local calls = {}
    local function call(name) calls[#calls + 1] = name end
    local environment = setmetatable({ package = {} }, { __index = _G })
    environment._G = environment
    environment.load = function(source, name, mode) return load(source, name, mode, environment) end
    environment.UE = {
        FAutoBlueprintLuaRunRequest = function() return {} end,
        FAutoBlueprintBuildOptions = function() return { bSave = true, bWriteReport = true } end,
        FAutoBlueprintSmokeTestRequest = function() return {} end,
        UAutoBlueprintLibrary = {
            RunEditorScript = function(request)
                assert(request.ScriptPath == "examples/hello.lua")
                return { bSuccess = false, Code = "EDITOR_BUSY", Operations = { ToTable = function() return {} end } }
            end,
            BuildFromLayoutFile = function(path, options)
                assert(path == "UIBlueprintLayouts/p6.json" and not options.bSave and not options.bWriteReport)
                call("BuildFromLayoutFile")
                return { bSuccess = false, bAssetCreated = false }
            end,
            GenerateFromLayoutFile = function(path, options)
                assert(path == "UIBlueprintLayouts/p6.json" and options.bSave and options.bWriteReport)
                call("GenerateFromLayoutFile")
                return { bSuccess = not partial, Widget = { bAssetCreated = true, bCompiled = not compileFailure } }
            end,
            BuildSmokeTestWidget = function(request)
                assert(request.PackagePath == "/Game/UI/AutoBlueprintTests/WBP_P6" and not request.bSave)
                call("BuildWidgetBlueprint")
                return { bSuccess = true, bCompiled = true, bSaved = false }
            end,
            BuildWidgetBlueprint = function(json, options)
                assert(json == "{}" and not options.bSave)
                call("BuildWidgetBlueprint")
                return { bSuccess = false }
            end,
            GenerateFromLayout = function(json, options)
                assert(json == "{}" and not options.bSave)
                call("GenerateFromLayout")
                return { bSuccess = false }
            end
        }
    }
    environment.AutoBlueprintTask = {
        source = fixture, chunkName = "@tests/p6_outcomes.lua", arguments = arguments,
        readModule = function(name)
            assert(name == "tests.module_counter")
            return counter, "@tests/module_counter.lua"
        end
    }
    local ok, message = assert(load(bootstrap, "@AutoBlueprint/ToolRunner", "t", environment))()
    return ok, message, calls
end

local count = 0
for _, case in ipairs({ { false, "throw" }, { true, "throw" }, { true, "return" } }) do
    local ok, message, calls = execute({ "UIBlueprintLayouts/p6.json", "/Game/UI/AutoBlueprintTests/WBP_P6", case[2] }, case[1])
    assert(ok == (case[2] == "return"))
    assert(table.concat(calls, ",") == "BuildFromLayoutFile,GenerateFromLayoutFile,BuildWidgetBlueprint,BuildWidgetBlueprint,GenerateFromLayout")
    if not ok then
        assert(message:find("AUTOBLUEPRINT_P6_AFTER_GENERATION", 1, true))
        assert(message:find("p6_outcomes.lua:", 1, true) and message:find("stack traceback:", 1, true))
    end
    count = count + 1
end
local ok, message, calls = execute({}, false)
assert(not ok and message:find("P6 expects", 1, true) and #calls == 0)
count = count + 1
ok, message, calls = execute({ "UIBlueprintLayouts/p6.json", "/Game/UI/AutoBlueprintTests/WBP_P6", "throw" }, true, true)
assert(not ok and not message:find("AUTOBLUEPRINT_P6_AFTER_GENERATION", 1, true) and #calls == 2)
count = count + 1
print("PASS: P6 bootstrap/fixture checks (" .. count .. "; mock UE, no native journal or assets)")
