local module = {}

function module.message(arguments)
    if #arguments == 0 then
        return "AutoBlueprint editor Lua is ready."
    end
    return "AutoBlueprint arguments: " .. table.concat(arguments, " | ")
end

return module
