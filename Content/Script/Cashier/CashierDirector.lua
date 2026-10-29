-- UnLua binding for ACashierDirector.
-- Module path must match GetModuleName_Implementation: Cashier.CashierDirector
--
-- Layout (all under Content/Script/Cashier/):
--   CashierTypes.lua        phase names, denoms, action names
--   CashierKnobs.lua        tunable shift / order / score knobs
--   CashierStateMachine.lua legal phase edges
--   CashierOrder.lua        sample table + procedural deck
--   CashierChange.lua       {1,5,10} sum and optimal count
--   CashierScore.lua        end-of-shift formula
--   CashierView.lua         widget snapshot (pay gate lives here)
--   CashierInput.lua        action name -> phase gate
--   CashierSession.lua      one shift
--   CashierDirector.lua     this UnLua class
--   RunCashierTests.lua     host-side checks, not loaded by the actor

local Session = require("Cashier.CashierSession")
local View = require("Cashier.CashierView")
local Input = require("Cashier.CashierInput")
local Types = require("Cashier.CashierTypes")

local M = UnLua.Class()

local function MakeStore()
    if not UE or not UE.UCashierLibrary then
        return nil
    end
    return {
        Load = function()
            return UE.UCashierLibrary.LoadBestScore()
        end,
        Commit = function(_, score)
            return UE.UCashierLibrary.CommitBestScore(math.floor(score))
        end,
    }
end

function M:ReceiveBeginPlay()
    self.Session = Session.New({
        clock = self.ShiftClock,
        store = MakeStore(),
    })
    if self.ShiftClock then
        self.ShiftClock:SetShiftDuration(self.Session.knobs.ShiftDurationSeconds)
        self.ShiftClock:StartShift()
    end
    self.Session:StartShift()
    self:Publish()
end

function M:OnCashierTick(DeltaSeconds)
    if not self.Session then
        return
    end
    self.Session:Advance(DeltaSeconds)
    self:Publish()
end

function M:OnShiftTimeUp()
    if not self.Session then
        return
    end
    self.Session:ForceShiftSettle()
    self:Publish()
end

local function fillBoard(view)
    local board = UE.FCashierBoardView()
    board.Phase = view.phase or ""
    board.bHasAmountDue = view.amountDue ~= nil
    board.AmountDue = view.amountDue or 0
    board.bPayRevealed = view.bPayRevealed == true
    board.PayDisplay = view.payText or ""
    board.PayValue = view.pay or 0
    board.bShowChangeSum = view.showChangeSum == true
    board.ChangeSum = view.changeSum or 0
    board.bDrawerEnabled = view.drawerEnabled == true
    board.bConfirmEnabled = view.confirmEnabled == true
    board.bClearEnabled = view.clearEnabled == true
    board.BillCount = view.billCount or 0
    board.RemainingSeconds = view.remainingSeconds or 0
    board.bHudScoreVisible = view.hudScoreVisible == true
    board.HudScore = view.shiftScore or 0
    board.bHidePlayfield = view.bHidePlayfield == true
    board.bSettleValid = view.shiftScore ~= nil
    board.ShiftScore = view.shiftScore or 0
    board.CorrectCount = view.correctCount or 0
    board.OptimalCount = view.optimalCount or 0
    board.WrongAbsSum = view.wrongAbsSum or 0
    board.WrongPenalty = view.wrongPenalty or 0
    board.bNewBest = view.bNewBest == true
    board.BestScore = view.bestScore or 0
    board.bFocusConfirm = view.bConfirmFocus == true
    board.bFocusClear = view.bClearFocus == true
    board.bFocusSettle = view.bSettleFocus == true
    board.FocusDenom = 0
    if view.focus and view.focus.kind == "denom" then
        board.FocusDenom = view.focus.value
    end
    board.BillStrip:Clear()
    for i = 1, #view.billStrip do
        board.BillStrip:Add(view.billStrip[i])
    end
    board.Items:Clear()
    for i = 1, #view.items do
        local src = view.items[i]
        local item = UE.FCashierItemView()
        item.Index = src.index
        item.Name = src.name
        item.Price = src.price
        item.bScanned = src.bScanned
        item.bFocus = src.bFocus
        board.Items:Add(item)
    end
    return board
end

function M:Publish()
    if not self.Session then
        return nil
    end
    local view = View.Build(self.Session)
    self.CachedView = view
    if self.PushBoardView and UE and UE.FCashierBoardView then
        self:PushBoardView(fillBoard(view))
    end
    return view
end

function M:GetView()
    if not self.CachedView then
        return self:Publish()
    end
    return self.CachedView
end

local function after(self, ok)
    self:Publish()
    return ok == true
end

function M:Input_ScanItem()
    return after(self, Input.Dispatch(self.Session, Types.Action.ScanItem))
end

function M:Input_AddDenom_1()
    return after(self, Input.Dispatch(self.Session, Types.Action.AddDenom_1))
end

function M:Input_AddDenom_5()
    return after(self, Input.Dispatch(self.Session, Types.Action.AddDenom_5))
end

function M:Input_AddDenom_10()
    return after(self, Input.Dispatch(self.Session, Types.Action.AddDenom_10))
end

function M:Input_RemoveLastDenom()
    return after(self, Input.Dispatch(self.Session, Types.Action.RemoveLastDenom))
end

function M:Input_ClearDenoms()
    if not self.Session then
        return false
    end
    return after(self, self.Session:ClearDenoms())
end

function M:Input_Confirm()
    return after(self, Input.Dispatch(self.Session, Types.Action.Confirm))
end

function M:Input_FocusNext()
    return after(self, Input.Dispatch(self.Session, Types.Action.FocusNext))
end

function M:Input_FocusPrev()
    return after(self, Input.Dispatch(self.Session, Types.Action.FocusPrev))
end

function M:Input_OpenSettle()
    return after(self, Input.Dispatch(self.Session, Types.Action.OpenSettle))
end

function M:ScanItem(Index)
    if not self.Session then
        return false
    end
    return after(self, self.Session:ScanItem(Index))
end

function M:AddDenom(Value)
    if not self.Session then
        return false
    end
    return after(self, self.Session:AddDenom(Value))
end

return M
