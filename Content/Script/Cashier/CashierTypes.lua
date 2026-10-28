-- Logical names shared with Source/ProjectOne/Cashier/CashierInputActions.cpp
-- and Content/UI/Cashier/README.md.
-- Demo has no UndoScan action. Do not add one here.

local Types = {}

Types.Phase = {
    WaitCustomer = "WaitCustomer",
    Scanning = "Scanning",
    PayShown = "PayShown",
    MakingChange = "MakingChange",
    Confirm = "Confirm",
    NextCustomer = "NextCustomer",
    ShiftSettle = "ShiftSettle",
}

Types.Denom = { 1, 5, 10 }

-- Weak placeholder shown on the settle board while scanning. Not a number.
Types.PayPlaceholder = "？？"

Types.Action = {
    ScanItem = "ScanItem",
    AddDenom_1 = "AddDenom_1",
    AddDenom_5 = "AddDenom_5",
    AddDenom_10 = "AddDenom_10",
    RemoveLastDenom = "RemoveLastDenom",
    Confirm = "Confirm",
    FocusNext = "FocusNext",
    FocusPrev = "FocusPrev",
    OpenSettle = "OpenSettle",
}

Types.ActionList = {
    "ScanItem",
    "AddDenom_1",
    "AddDenom_5",
    "AddDenom_10",
    "RemoveLastDenom",
    "Confirm",
    "FocusNext",
    "FocusPrev",
    "OpenSettle",
}

function Types.IsDenom(value)
    return value == 1 or value == 5 or value == 10
end

function Types.IsPayRevealed(phase)
    return phase == Types.Phase.PayShown
        or phase == Types.Phase.MakingChange
        or phase == Types.Phase.Confirm
end

return Types
