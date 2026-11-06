-- Run from the repository root: lua Plugins/AutoBlueprint/Scripts/Tests/p1_host_test.lua
-- Real bootstrap/SDK with mock file reads and UE calls. Native coverage is in AutoBlueprint.P1.
local pluginRoot = arg[1] or "Plugins/AutoBlueprint"
local function readFile(path)
    local file = assert(io.open(path, "rb"))
    local content = file:read("a")
    file:close()
    return content
end

local runner = readFile(pluginRoot .. "/Source/AutoBlueprintEditor/Private/Scripting/EditorLuaRunner.cpp")
local bootstrap = assert(runner:match('TEXT%(R"lua%((.-)%)lua"%)'), "C++ bootstrap not found")
local sourceRoot = pluginRoot .. "/Scripts/Lua/"
local sources = {}
for _, name in ipairs({
    "auto_blueprint", "examples.greeter", "tests.module_counter",
    "tests.module_failure", "tests.cycle_a", "tests.cycle_b"
}) do
    sources[name] = readFile(sourceRoot .. name:gsub("%.", "/") .. ".lua")
end

local function execute(source, filename, arguments, overrides)
    local output, reads = {}, {}
    local buildCalls = 0
    local environment = setmetatable({
        package = {
            path = "game/?.lua", cpath = "native/?.dll", searchers = {},
            loaded = { UnLua = {}, ["examples.greeter"] = { message = function() error("game module used") end } }
        },
        print = function(message) output[#output + 1] = message end,
        UE = {
            FAutoBlueprintLuaRunRequest = function() return {} end,
            UAutoBlueprintLibrary = {
                RunEditorScript = function() return { bSuccess = false, Code = "EDITOR_BUSY" } end,
                BuildSmokeTestWidget = function()
                    buildCalls = buildCalls + 1
                    error("Unexpected asset build")
                end
            }
        }
    }, { __index = _G })
    environment._G = environment
    environment.AutoBlueprintTask = {
        source = source, chunkName = "@" .. filename, arguments = arguments or {},
        readModule = function(name)
            reads[name] = (reads[name] or 0) + 1
            local value = overrides and overrides[name] or sources[name]
            if type(value) == "function" then
                value = value(reads[name])
            end
            if value == nil then
                return nil, "LUA_SCRIPT_NOT_FOUND: " .. name
            end
            return value, "@" .. name:gsub("%.", "/") .. ".lua"
        end
    }
    environment.load = function(chunk, name, mode)
        return load(chunk, name, mode, environment)
    end
    local ok, message = assert(load(bootstrap, "@AutoBlueprint/ToolRunner", "t", environment))()
    assert(environment.AutoBlueprintTask == nil)
    assert(environment.package.path == "" and environment.package.cpath == "")
    assert(next(environment.package.searchers) == nil)
    return ok, message, table.concat(output, "\n"), reads, buildCalls, environment
end

local count = 0
local function test(name, body)
    body()
    count = count + 1
    print("PASS: " .. name)
end

test("arguments are data", function()
    local payload = '"; AutoBlueprintInjected = true; --'
    local ok, message, output, _, _, environment = execute(
        readFile(sourceRoot .. "examples/hello.lua"), "examples/hello.lua", { payload, "中文 with spaces" })
    assert(ok and message == nil and environment.AutoBlueprintInjected == nil)
    assert(output:find(payload, 1, true) and output:find("中文 with spaces", 1, true))
end)

test("fresh module caches", function()
    for _ = 1, 2 do
        local ok, message = execute(readFile(sourceRoot .. "tests/host_contract.lua"), "tests/host_contract.lua",
            { '"; AutoBlueprintInjected = true; --', "中文路径 with spaces" })
        assert(ok, message)
    end
end)

test("module exception trace", function()
    local ok, message = execute('return { run = function() require("tests.module_failure") end }', "entry.lua")
    assert(not ok and message:find("AUTOBLUEPRINT_P1_MODULE_ERROR", 1, true))
    assert(message:find("tests/module_failure.lua:", 1, true) and message:find("stack traceback:", 1, true))
end)

test("module cycle", function()
    local ok, message = execute('return { run = function() require("tests.cycle_a") end }', "entry.lua")
    assert(not ok and message:find("LUA_MODULE_CYCLE", 1, true))
end)

test("entry syntax failure", function()
    local ok, message = execute("local =", "syntax.lua")
    assert(not ok and message:find("syntax.lua:", 1, true))
end)

test("module syntax failure", function()
    local ok, message = execute('return { run = function() require("syntax_module") end }',
        "entry.lua", {}, { syntax_module = "local =" })
    assert(not ok and message:find("syntax_module.lua:", 1, true))
end)

test("entry contract", function()
    local ok, message = execute("return {}", "invalid_entry.lua")
    assert(not ok and message:find("Tool script must return a table", 1, true))
end)

test("false entry result", function()
    local ok, message = execute("return { run = function() return false end }", "false_entry.lua")
    assert(not ok and message:find("Tool script must return true", 1, true))
end)

test("layout capability boundary", function()
    local ok, message, _, _, builds = execute(readFile(sourceRoot .. "generate_from_file.lua"),
        "generate_from_file.lua", { "UIBlueprintLayouts/test.layout.json" })
    assert(not ok and message:find("NOT_IMPLEMENTED", 1, true) and builds == 0)
end)

test("layout entry arguments", function()
    local ok, message = execute(readFile(sourceRoot .. "generate_from_file.lua"), "generate_from_file.lua")
    assert(not ok and message:find("INVALID_ARGUMENTS", 1, true))
end)

test("failed modules can retry", function()
    local ok, message, _, reads = execute([[
return { run = function()
    local ok = pcall(require, "retry")
    assert(not ok)
    assert(require("retry").ready)
    return true
end }
]], "retry_entry.lua", {}, {
        retry = function(attempt)
            return attempt == 1 and 'error("first load failed")' or 'return { ready = true }'
        end
    })
    assert(ok, message)
    assert(reads.retry == 2)
end)

test("no fallback to preloaded game modules", function()
    local ok, message = execute('return { run = function() require("UnLua") end }', "entry.lua")
    assert(not ok and message:find("LUA_SCRIPT_NOT_FOUND", 1, true))
end)

test("nil and false module exports", function()
    local ok, message, _, reads = execute([[
return { run = function()
    assert(require("no_value") == true and require("no_value") == true)
    assert(require("false_value") == false and require("false_value") == false)
    return true
end }
]], "exports.lua", {}, { no_value = "return", false_value = "return false" })
    assert(ok, message)
    assert(reads.no_value == 1 and reads.false_value == 2)
end)

test("module load limit", function()
    local modules = {}
    for index = 1, 65 do
        modules["limit.m" .. index] = "return {}"
    end
    local ok, message = execute([[
return { run = function()
    for index = 1, 65 do require("limit.m" .. index) end
    return true
end }
]], "limit.lua", {}, modules)
    assert(not ok and message:find("LUA_MODULE_LIMIT", 1, true))
end)

print("PASS: " .. count .. " P1 host/SDK cases (mock file reads and UE facade; no engine validation)")
