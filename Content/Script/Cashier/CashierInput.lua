-- Phase gates for the logical Enhanced Input actions.
-- Out-of-phase actions return false so one mapping context can bind Enter
-- to both ScanItem and Confirm. There is no UndoScan rule.

local Types = require("Cashier.CashierTypes")

local Input = {}

local P = Types.Phase

local DrawerPhases = {
    [P.PayShown] = true,
    [P.MakingChange] = true,
}

local FocusPhases = {
    [P.Scanning] = true,
    [P.PayShown] = true,
    [P.MakingChange] = true,
    [P.ShiftSettle] = true,
}

Input.Rules = {
    ScanItem = {
        phases = { [P.Scanning] = true },
        run = function(session)
            return session:ScanFocused()
        end,
    },
    AddDenom_1 = {
        phases = DrawerPhases,
        run = function(session)
            return session:AddDenom(1)
        end,
    },
    AddDenom_5 = {
        phases = DrawerPhases,
        run = function(session)
            return session:AddDenom(5)
        end,
    },
    AddDenom_10 = {
        phases = DrawerPhases,
        run = function(session)
            return session:AddDenom(10)
        end,
    },
    RemoveLastDenom = {
        phases = DrawerPhases,
        run = function(session)
            return session:RemoveLastDenom()
        end,
    },
    Confirm = {
        phases = DrawerPhases,
        run = function(session)
            return session:Confirm()
        end,
    },
    FocusNext = {
        phases = FocusPhases,
        run = function(session)
            return session:FocusStep(1)
        end,
    },
    FocusPrev = {
        phases = FocusPhases,
        run = function(session)
            return session:FocusStep(-1)
        end,
    },
    OpenSettle = {
        phases = { [P.ShiftSettle] = true },
        run = function(session)
            return session:RestartShift()
        end,
    },
}

function Input.Dispatch(session, actionName)
    if not session then
        return false
    end
    local rule = Input.Rules[actionName]
    if not rule then
        return false
    end
    if not rule.phases[session.phase] then
        return false
    end
    return rule.run(session) == true
end

return Input
