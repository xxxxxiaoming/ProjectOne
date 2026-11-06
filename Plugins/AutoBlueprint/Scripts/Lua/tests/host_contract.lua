-- Real UE fixture for AutoBlueprint.P1.HostLifecycle.
local module = {}

function module.run(arguments)
    assert(AutoBlueprintTask == nil)
    assert(AutoBlueprintInjected == nil)
    assert(#arguments == 2)
    assert(arguments[1] == '"; AutoBlueprintInjected = true; --')
    assert(arguments[2] == "中文路径 with spaces")
    local counter = require("tests.module_counter")
    assert(counter.loads == 1)
    assert(require("tests.module_counter") == counter)

    for _, name in ipairs({ "../outside", "tests/module_counter", "tests..module_counter", "tests.module_counter.lua", "C:\\outside" }) do
        local ok, message = pcall(require, name)
        assert(not ok)
        assert(message:find("INVALID_MODULE_NAME", 1, true) or message:find("LUA_SCRIPT_NOT_FOUND", 1, true))
    end
    local ok, message = pcall(require, "UnLua")
    assert(not ok and message:find("LUA_SCRIPT_NOT_FOUND", 1, true))
    ok, message = pcall(require, "tests.module_failure")
    assert(not ok and message:find("AUTOBLUEPRINT_P1_MODULE_ERROR", 1, true))
    assert(message:find("module_failure.lua:", 1, true) and message:find("stack traceback:", 1, true))
    ok, message = pcall(require, "tests.cycle_a")
    assert(not ok and message:find("LUA_MODULE_CYCLE", 1, true))

    assert(require("examples.greeter").message({}) == "AutoBlueprint editor Lua is ready.")
    local nested = UE.FAutoBlueprintLuaRunRequest()
    nested.ScriptPath = "examples/hello.lua"
    local result = UE.UAutoBlueprintLibrary.RunEditorScript(nested)
    assert(result.bSuccess == false and result.Code == "EDITOR_BUSY")
    assert(AutoBlueprintInjected == nil)
    return true
end

return module
