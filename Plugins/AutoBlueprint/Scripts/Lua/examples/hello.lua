-- No asset writes. Run with: AutoBlueprint.Run examples/hello.lua "hello world"
local greeter = require("examples.greeter")
local module = {}

function module.run(arguments)
    print(greeter.message(arguments))
    return true
end

return module
