local Types = require("Cashier.CashierTypes")

local SM = {}

local P = Types.Phase

-- Legal edges only. Scanning cannot skip PayShown, and there is no exact-change state.
SM.Edges = {
    [P.WaitCustomer] = { [P.Scanning] = true, [P.ShiftSettle] = true },
    [P.Scanning] = { [P.PayShown] = true, [P.ShiftSettle] = true },
    [P.PayShown] = { [P.MakingChange] = true, [P.Confirm] = true, [P.ShiftSettle] = true },
    [P.MakingChange] = { [P.Confirm] = true, [P.ShiftSettle] = true },
    [P.Confirm] = { [P.NextCustomer] = true, [P.ShiftSettle] = true },
    [P.NextCustomer] = { [P.WaitCustomer] = true, [P.ShiftSettle] = true },
    [P.ShiftSettle] = { [P.WaitCustomer] = true },
}

function SM.CanGo(fromPhase, toPhase)
    local nexts = SM.Edges[fromPhase]
    return nexts ~= nil and nexts[toPhase] == true
end

return SM
