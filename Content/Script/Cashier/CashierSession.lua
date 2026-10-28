-- One shift of cashier mode A.
-- Money is integer yuan. C = tender - S is used only to score a confirm.
-- The view builder is what UI reads; this object keeps tender internally.

local Types = require("Cashier.CashierTypes")
local SM = require("Cashier.CashierStateMachine")
local Knobs = require("Cashier.CashierKnobs")
local Order = require("Cashier.CashierOrder")
local Change = require("Cashier.CashierChange")
local Score = require("Cashier.CashierScore")

local Session = {}
Session.__index = Session

local P = Types.Phase

function Session.New(opts)
    opts = opts or {}
    local self = setmetatable({}, Session)
    self.knobs = Knobs.Merge(opts.knobs)
    self.clock = opts.clock
    self.store = opts.store
    self.localBest = 0
    self.phase = nil
    self.ledger = {}
    self.summary = nil
    self.bills = {}
    return self
end

function Session:GetRemainingSeconds()
    if self.clock and self.clock.GetRemainingSeconds then
        return self.clock:GetRemainingSeconds()
    end
    if not self.simRemaining then
        return self.knobs.ShiftDurationSeconds
    end
    if self.simRemaining < 0 then
        return 0
    end
    return self.simRemaining
end

function Session:ScannedSum()
    if not self.order then
        return 0
    end
    local total = 0
    for i = 1, #self.order.items do
        local item = self.order.items[i]
        if item.bScanned then
            total = total + item.price
        end
    end
    return total
end

function Session:BillSum()
    return Change.Sum(self.bills or {})
end

function Session:EnterPhase(nextPhase)
    if not SM.CanGo(self.phase, nextPhase) then
        return false
    end
    self.phase = nextPhase
    self.phaseTime = 0
    self:ApplyFocus(nextPhase)
    return true
end

function Session:ApplyFocus(phase)
    if phase == P.Scanning then
        self.focus = { kind = "item", index = self:FirstUnscanned() or 1 }
    elseif phase == P.PayShown or phase == P.MakingChange then
        -- Drawer is the main control once pay is revealed. Doc names ¥1 for MakingChange.
        self.focus = { kind = "denom", value = 1 }
    elseif phase == P.ShiftSettle then
        self.focus = { kind = "settle" }
    else
        self.focus = nil
    end
end

function Session:FirstUnscanned()
    if not self.order then
        return nil
    end
    for i = 1, #self.order.items do
        if not self.order.items[i].bScanned then
            return i
        end
    end
    return nil
end

function Session:StartShift()
    self.ledger = {}
    self.summary = nil
    self.order = nil
    self.bills = {}
    self.focus = nil
    self.orderSeconds = 0
    self.bShiftOver = false
    self.simRemaining = self.knobs.ShiftDurationSeconds
    self.deck = Order.NewDeck(self.knobs)
    self.phase = P.WaitCustomer
    self.phaseTime = 0
    self:ApplyFocus(P.WaitCustomer)
    if self.knobs.CustomerArriveSeconds <= 0 then
        self:BeginOrder()
    end
    return true
end

function Session:BeginOrder()
    if self.phase ~= P.WaitCustomer or self.bShiftOver then
        return false
    end
    self.order = Order.Next(self.deck)
    self.bills = {}
    self.orderSeconds = 0
    return self:EnterPhase(P.Scanning)
end

function Session:ScanItem(index)
    if self.phase ~= P.Scanning or not self.order then
        return false
    end
    local item = self.order.items[index]
    if not item or item.bScanned then
        return false
    end
    item.bScanned = true
    if self:FirstUnscanned() == nil then
        self:EnterPayShown()
    else
        self.focus = { kind = "item", index = self:FirstUnscanned() }
    end
    return true
end

function Session:ScanFocused()
    if not self.focus or self.focus.kind ~= "item" then
        return false
    end
    return self:ScanItem(self.focus.index)
end

function Session:EnterPayShown()
    if not self:EnterPhase(P.PayShown) then
        return false
    end
    if self.knobs.PayRevealSeconds <= 0 then
        self:EnterPhase(P.MakingChange)
    end
    return true
end

function Session:AddDenom(value)
    if self.phase ~= P.PayShown and self.phase ~= P.MakingChange then
        return false
    end
    if not Types.IsDenom(value) then
        return false
    end
    if self.phase == P.PayShown then
        if not self:EnterPhase(P.MakingChange) then
            return false
        end
    end
    self.bills[#self.bills + 1] = value
    return true
end

function Session:RemoveLastDenom()
    if self.phase ~= P.PayShown and self.phase ~= P.MakingChange then
        return false
    end
    if #self.bills == 0 then
        return false
    end
    table.remove(self.bills)
    return true
end

function Session:ClearDenoms()
    if self.phase ~= P.PayShown and self.phase ~= P.MakingChange then
        return false
    end
    if #self.bills == 0 then
        return false
    end
    self.bills = {}
    return true
end

function Session:Confirm()
    if self.phase ~= P.PayShown and self.phase ~= P.MakingChange then
        return false
    end
    if not self.order or self:FirstUnscanned() ~= nil then
        return false
    end
    self:CommitOrder()
    return self:EnterPhase(P.Confirm)
end

function Session:CommitOrder()
    local total = Order.Total(self.order)
    local changeDue = self.order.tender - total
    local sum = self:BillSum()
    local correct = sum == changeDue
    local optimalCount = Change.OptimalBillCount(changeDue)
    local record = {
        id = self.order.id,
        bCorrect = correct,
        bOptimal = correct and optimalCount ~= nil and #self.bills == optimalCount,
        sum = sum,
        c = changeDue,
        absDelta = math.abs(sum - changeDue),
        secondsUsed = self.orderSeconds or 0,
        bHard = self.order.bHard == true,
    }
    self.ledger[#self.ledger + 1] = record
end

function Session:FinishConfirm()
    if self.phase ~= P.Confirm then
        return false
    end
    return self:EnterPhase(P.NextCustomer)
end

function Session:FinishDepart()
    if self.phase ~= P.NextCustomer then
        return false
    end
    if not self:EnterPhase(P.WaitCustomer) then
        return false
    end
    self.order = nil
    self.bills = {}
    if self.knobs.CustomerArriveSeconds <= 0 then
        self:BeginOrder()
    end
    return true
end

function Session:FocusList()
    if self.phase == P.Scanning and self.order then
        local list = {}
        for i = 1, #self.order.items do
            if not self.order.items[i].bScanned then
                list[#list + 1] = { kind = "item", index = i }
            end
        end
        return list
    end
    if self.phase == P.PayShown or self.phase == P.MakingChange then
        return {
            { kind = "denom", value = 1 },
            { kind = "denom", value = 5 },
            { kind = "denom", value = 10 },
            { kind = "clear" },
            { kind = "confirm" },
        }
    end
    if self.phase == P.ShiftSettle then
        return { { kind = "settle" } }
    end
    return {}
end

local function sameFocus(a, b)
    if not a or not b or a.kind ~= b.kind then
        return false
    end
    if a.kind == "item" then
        return a.index == b.index
    end
    if a.kind == "denom" then
        return a.value == b.value
    end
    return true
end

function Session:FocusStep(dir)
    local list = self:FocusList()
    if #list == 0 then
        return false
    end
    local current = nil
    for i = 1, #list do
        if sameFocus(self.focus, list[i]) then
            current = i
            break
        end
    end
    if not current then
        self.focus = list[1]
        return true
    end
    local nextIndex = ((current - 1 + dir) % #list) + 1
    self.focus = list[nextIndex]
    return true
end

function Session:EvaluateBest()
    local summary = Score.ScoreShift(self.ledger, self.knobs)
    local previous = self.localBest
    if self.store and self.store.Load then
        previous = self.store:Load() or 0
    end
    local refreshed = summary.score > previous
    if refreshed then
        local committed = true
        if self.store and self.store.Commit then
            committed = self.store:Commit(summary.score) ~= false
        end
        if committed then
            self.localBest = summary.score
        else
            refreshed = false
            self.localBest = previous
        end
    else
        self.localBest = previous
    end
    if self.store and self.store.Load then
        summary.bestScore = self.store:Load() or self.localBest
    else
        summary.bestScore = self.localBest
    end
    summary.bNewBest = refreshed
    self.summary = summary
    return summary
end

function Session:ForceShiftSettle()
    if self.phase == P.ShiftSettle then
        return false
    end
    self.bShiftOver = true
    if not self:EnterPhase(P.ShiftSettle) then
        self.phase = P.ShiftSettle
        self.phaseTime = 0
        self:ApplyFocus(P.ShiftSettle)
    end
    -- Drop the in-progress order. A confirm already committed stays in the ledger.
    self.order = nil
    self.bills = {}
    self:EvaluateBest()
    return true
end

function Session:RestartShift()
    if self.phase ~= P.ShiftSettle then
        return false
    end
    if self.clock and self.clock.SetShiftDuration and self.clock.StartShift then
        self.clock:SetShiftDuration(self.knobs.ShiftDurationSeconds)
        self.clock:StartShift()
    end
    return self:StartShift()
end

function Session:TickPhase(dt)
    if self.phase == P.Scanning or self.phase == P.PayShown or self.phase == P.MakingChange then
        self.orderSeconds = (self.orderSeconds or 0) + dt
    end
    self.phaseTime = (self.phaseTime or 0) + dt
    if self.phase == P.WaitCustomer and self.phaseTime >= self.knobs.CustomerArriveSeconds then
        self:BeginOrder()
    elseif self.phase == P.PayShown and self.phaseTime >= self.knobs.PayRevealSeconds then
        self:EnterPhase(P.MakingChange)
    elseif self.phase == P.Confirm and self.phaseTime >= self.knobs.ConfirmLockSeconds then
        self:FinishConfirm()
    elseif self.phase == P.NextCustomer and self.phaseTime >= self.knobs.DepartSeconds then
        self:FinishDepart()
    end
end

function Session:Advance(dt)
    if self.phase == P.ShiftSettle or self.bShiftOver then
        return
    end
    if self.clock and self.clock.IsTimeUp and self.clock:IsTimeUp() then
        self:ForceShiftSettle()
        return
    end
    if self.clock == nil then
        self.simRemaining = (self.simRemaining or 0) - dt
        if self.simRemaining <= 0 then
            self.simRemaining = 0
            self:ForceShiftSettle()
            return
        end
    end
    self:TickPhase(dt)
end

return Session
