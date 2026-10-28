-- UI snapshot. This is the only table widgets should read.
-- Pay is a numeric field only after PayShown. Change-due (C) is never copied.

local Types = require("Cashier.CashierTypes")

local View = {}

local P = Types.Phase

local function focusMatches(focus, kind, value)
    if not focus or focus.kind ~= kind then
        return false
    end
    if kind == "item" then
        return focus.index == value
    end
    if kind == "denom" then
        return focus.value == value
    end
    return true
end

function View.Build(session)
    local phase = session.phase
    local revealed = Types.IsPayRevealed(phase)
    local showSum = phase == P.MakingChange or phase == P.Confirm
    local drawer = phase == P.PayShown or phase == P.MakingChange
    local play = phase == P.Scanning
        or phase == P.PayShown
        or phase == P.MakingChange
        or phase == P.Confirm

    local view = {
        phase = phase,
        remainingSeconds = session:GetRemainingSeconds(),
        bPayRevealed = revealed,
        showChangeSum = showSum,
        drawerEnabled = drawer,
        confirmEnabled = drawer,
        clearEnabled = drawer,
        bHidePlayfield = phase == P.ShiftSettle,
        hudScoreVisible = false,
        billStrip = {},
        billCount = 0,
        items = {},
        denom = {},
        focus = session.focus,
        bConfirmFocus = focusMatches(session.focus, "confirm"),
        bClearFocus = focusMatches(session.focus, "clear"),
        bSettleFocus = focusMatches(session.focus, "settle"),
    }

    if play and session.order then
        view.amountDue = session:ScannedSum()
    end

    if phase == P.Scanning then
        view.payText = Types.PayPlaceholder
    elseif revealed and session.order then
        view.pay = session.order.tender
        view.payText = tostring(session.order.tender)
    end

    if showSum then
        view.changeSum = session:BillSum()
    end

    if session.order and play then
        for i = 1, #session.order.items do
            local item = session.order.items[i]
            view.items[i] = {
                index = i,
                name = item.name,
                price = item.price,
                bScanned = item.bScanned == true,
                bFocus = focusMatches(session.focus, "item", i),
            }
        end
    end

    local strip = session.bills or {}
    for i = 1, #strip do
        view.billStrip[i] = strip[i]
    end
    view.billCount = #view.billStrip

    for i = 1, #Types.Denom do
        local value = Types.Denom[i]
        view.denom[i] = {
            value = value,
            bEnabled = drawer,
            bFocus = focusMatches(session.focus, "denom", value),
        }
    end

    if phase == P.ShiftSettle and session.summary then
        local summary = session.summary
        view.hudScoreVisible = true
        view.shiftScore = summary.score
        view.correctCount = summary.correctCount
        view.optimalCount = summary.optimalCount
        view.wrongAbsSum = summary.wrongAbs
        view.wrongPenalty = summary.penalty
        view.bNewBest = summary.bNewBest == true
        view.bestScore = summary.bestScore
        -- Star cutoffs are not in the design doc.
        view.stars = nil
    end

    return view
end

local FORBIDDEN_BEFORE_REVEAL = {
    "pay",
    "tender",
    "changeDue",
    "c",
    "pendingChange",
    "bCorrect",
    "bOptimal",
    "correctCount",
    "optimalCount",
    "shiftScore",
    "wrongAbsSum",
    "wrongPenalty",
}

function View.ForbiddenKeys(view)
    local found = {}
    local revealed = view.bPayRevealed == true
    for i = 1, #FORBIDDEN_BEFORE_REVEAL do
        local key = FORBIDDEN_BEFORE_REVEAL[i]
        if key == "pay" and revealed then
            -- numeric pay is allowed only once the phase has revealed it
        elseif view[key] ~= nil then
            found[#found + 1] = key
        end
    end
    if not revealed and view.phase == Types.Phase.Scanning then
        if view.payText ~= Types.PayPlaceholder then
            found[#found + 1] = "payText"
        end
    end
    if view.changeDue ~= nil or view.pendingChange ~= nil or view.tender ~= nil then
        found[#found + 1] = "changeDue"
    end
    return found
end

return View
