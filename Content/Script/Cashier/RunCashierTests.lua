-- Host-side checks for the cashier Lua scaffold.
-- Run: lua Content/Script/Cashier/RunCashierTests.lua
-- Requiring this file from UnLua does not execute the checks.

local function script_dir()
    local src = debug.getinfo(1, "S").source
    if src:sub(1, 1) == "@" then
        src = src:sub(2)
    end
    return src:match("^(.*)[/\\]") or "."
end

local dir = script_dir()
local root = dir:match("^(.*)[/\\]") or "."
package.path = root .. "/?.lua;" .. package.path

local Types = require("Cashier.CashierTypes")
local Knobs = require("Cashier.CashierKnobs")
local SM = require("Cashier.CashierStateMachine")
local Change = require("Cashier.CashierChange")
local Order = require("Cashier.CashierOrder")
local Score = require("Cashier.CashierScore")
local View = require("Cashier.CashierView")
local Input = require("Cashier.CashierInput")
local Session = require("Cashier.CashierSession")

local failed = 0

local function check(cond, message)
    if not cond then
        failed = failed + 1
        io.stderr:write("FAIL: " .. message .. "\n")
    end
end

local function eq_list(a, b)
    if a == nil or b == nil or #a ~= #b then
        return false
    end
    for i = 1, #a do
        if a[i] ~= b[i] then
            return false
        end
    end
    return true
end

local function fast(extra)
    local knobs = {
        CustomerArriveSeconds = 0,
        PayRevealSeconds = 10,
        ConfirmLockSeconds = 10,
        DepartSeconds = 10,
        ShiftDurationSeconds = 1000,
    }
    if extra then
        for key, value in pairs(extra) do
            knobs[key] = value
        end
    end
    return knobs
end

local function new_session(extra, store)
    local session = Session.New({ knobs = fast(extra), store = store })
    session:StartShift()
    return session
end

local function replace_order(session, row)
    session.order = Order.Clone(row)
    session.bills = {}
    session:ApplyFocus(session.phase)
end

local function scan_all(session)
    local guard = 0
    while session.phase == Types.Phase.Scanning and guard < 8 do
        local index = session:FirstUnscanned()
        check(session:ScanItem(index), "scan item " .. tostring(index))
        guard = guard + 1
    end
end

local function play_optimal(session)
    scan_all(session)
    local changeDue = session.order.tender - Order.Total(session.order)
    local bills = Change.OptimalBills(changeDue)
    for i = 1, #bills do
        check(session:AddDenom(bills[i]), "add optimal denom")
    end
    check(session:Confirm(), "confirm optimal")
end

local function run()
    check(Types.Action.UndoScan == nil, "action table has UndoScan")
    check(Input.Rules.UndoScan == nil, "input rules have UndoScan")
    check(Session.UndoScan == nil, "session exposes UndoScan")
    check(#Types.ActionList == 9, "action count")
    check(#Types.Denom == 3 and Types.Denom[1] == 1 and Types.Denom[2] == 5 and Types.Denom[3] == 10, "denoms")

    local expectedBills = {
        { 1, 1 },
        { 1, 1 },
        { 1, 1, 1 },
        { 1 },
        { 10, 10, 1, 1, 1, 1 },
        {},
        { 5 },
        { 10, 10, 10, 10, 5, 1 },
        {},
        { 10, 1 },
    }
    local expectedS = { 3, 8, 17, 9, 26, 10, 15, 4, 10, 39 }
    local expectedPay = { 5, 10, 20, 10, 50, 10, 20, 50, 10, 50 }
    local hardCount = 0
    local zeroCount = 0
    check(#Order.Sample == 10, "sample size")
    for i = 1, #Order.Sample do
        local row = Order.Sample[i]
        local total = Order.Total(row)
        local changeDue = Order.ChangeDue(row)
        check(total == expectedS[i], "sample S " .. i)
        check(row.tender == expectedPay[i], "sample pay " .. i)
        check(changeDue == row.tender - total, "sample C identity " .. i)
        check(eq_list(Change.OptimalBills(changeDue), expectedBills[i]), "sample optimal " .. i)
        check(#row.items >= 1 and #row.items <= 3, "sample K " .. i)
        if row.bHard then
            hardCount = hardCount + 1
        end
        if changeDue == 0 then
            zeroCount = zeroCount + 1
        end
    end
    check(hardCount == 4, "sample hard ratio 4/10")
    check(zeroCount == 2, "sample C=0 ratio 2/10")
    -- Sample 10 is authored hard. The procedural predicate does not reproduce that tag.
    check(Change.IsProceduralHard(11, Knobs.Default()) == false, "procedural predicate is not the sample tag")

    check(SM.CanGo("Scanning", "PayShown"), "scan to pay")
    check(not SM.CanGo("Scanning", "MakingChange"), "no skip of pay reveal")
    check(not SM.CanGo("PayShown", "NextCustomer"), "no exact shortcut edge")
    check(SM.Edges.Exact == nil, "no exact phase")

    local session = new_session()
    check(session.phase == Types.Phase.Scanning, "auto customer")
    check(session.order.id == "sample-1", "first sample")
    local before = View.Build(session)
    check(before.amountDue == 0, "S starts at 0")
    check(before.pay == nil, "pay hidden before scan")
    check(before.payText == Types.PayPlaceholder, "placeholder")
    check(#View.ForbiddenKeys(before) == 0, "scanning view leak: " .. table.concat(View.ForbiddenKeys(before), ","))
    check(before.drawerEnabled == false and before.confirmEnabled == false, "drawer off while scanning")

    check(session:ScanItem(1), "scan water")
    check(not session:ScanItem(1), "no rescan")
    check(session:ScannedSum() == 3, "S after one scan")
    check(session.phase == Types.Phase.PayShown, "auto PayShown")
    local revealed = View.Build(session)
    check(revealed.bPayRevealed, "flag")
    check(revealed.pay == 5, "pay number on PayShown")
    check(revealed.payText == "5", "pay text")
    check(revealed.showChangeSum == false and revealed.changeSum == nil, "no sigma yet")
    check(revealed.changeDue == nil and revealed.tender == nil, "C stays off the view")
    check(revealed.drawerEnabled and revealed.confirmEnabled, "drawer on at PayShown")
    check(session.focus.kind == "denom" and session.focus.value == 1, "focus yuan 1")

    check(not Input.Dispatch(session, "ScanItem"), "scan ignored after pay")
    check(Input.Dispatch(session, "AddDenom_1"), "add 1")
    check(session.phase == Types.Phase.MakingChange, "first denom enters MakingChange")
    local summing = View.Build(session)
    check(summing.showChangeSum and summing.changeSum == 1, "sigma is the running sum")
    check(summing.changeDue == nil, "sigma is not C")
    check(Input.Dispatch(session, "AddDenom_1"), "second 1")
    check(session:BillSum() == 2, "sum 2")
    check(session:RemoveLastDenom(), "pop")
    check(session:BillSum() == 1, "sum after pop")
    check(session:AddDenom(1), "restore")
    check(session:Confirm(), "confirm")
    check(session.phase == Types.Phase.Confirm, "confirm lock")
    check(#session.ledger == 1, "recorded")
    check(session.ledger[1].bCorrect and session.ledger[1].bOptimal, "2 is correct and optimal")
    check(session.ledger[1].toast == nil and session.ledger[1].bToast == nil, "no toast field")
    local locked = View.Build(session)
    check(locked.bCorrect == nil and locked.correctCount == nil and locked.shiftScore == nil, "no mid-round verdict")
    check(locked.confirmEnabled == false, "confirm phase locks the drawer")

    session:Advance(10)
    check(session.phase == Types.Phase.NextCustomer, "depart")
    local leaving = View.Build(session)
    check(leaving.pay == nil and leaving.bCorrect == nil and leaving.amountDue == nil, "depart hides the verdict")
    check(not Input.Dispatch(session, "Confirm"), "confirm dead during depart")

    local wrong = new_session()
    scan_all(wrong)
    check(wrong:AddDenom(1), "underpay")
    check(wrong:Confirm(), "confirm underpay")
    check(wrong.ledger[1].bCorrect == false, "Σ ~= C")
    check(wrong.ledger[1].absDelta == 1, "abs delta")
    check(wrong.ledger[1].bOptimal == false, "not optimal")
    check(View.Build(wrong).bCorrect == nil, "wrong confirm still has no toast view")

    local zero = new_session()
    replace_order(zero, Order.Sample[6])
    scan_all(zero)
    check(zero.phase == Types.Phase.PayShown, "zero order reaches pay")
    check(View.Build(zero).pay == 10, "zero order still reveals pay")
    check(zero:BillSum() == 0, "no bills")
    check(zero:Confirm(), "zero-denom confirm")
    check(zero.ledger[1].bCorrect and zero.ledger[1].bOptimal, "C=0 zero bills is optimal")
    check(zero.ledger[1].sum == 0 and zero.ledger[1].c == 0, "sum and C are 0")

    local shortcut = new_session()
    scan_all(shortcut)
    check(shortcut:Confirm(), "empty confirm on C=2")
    check(shortcut.ledger[1].bCorrect == false, "empty confirm is not an exact-change success")
    check(shortcut.ledger[1].c == 2 and shortcut.ledger[1].absDelta == 2, "miss recorded")

    local fat = new_session()
    replace_order(fat, Order.Sample[7])
    scan_all(fat)
    for _ = 1, 5 do
        check(fat:AddDenom(1), "five ones")
    end
    check(fat:Confirm(), "confirm ones")
    check(fat.ledger[1].bCorrect and not fat.ledger[1].bOptimal, "correct but not optimal")

    local edited = new_session()
    scan_all(edited)
    check(edited:AddDenom(10), "too much")
    check(edited:ClearDenoms(), "clear")
    check(edited:BillSum() == 0, "cleared")
    check(edited:AddDenom(1) and edited:AddDenom(1), "rebuild")
    check(edited:Confirm(), "confirm after edit")
    check(edited.ledger[1].bCorrect, "edit before confirm is not a wrong order")

    check(not new_session():AddDenom(2), "reject unknown denom")
    local scanning = new_session()
    check(not Input.Dispatch(scanning, "AddDenom_5"), "denom blocked while scanning")
    check(not Input.Dispatch(scanning, "Confirm"), "confirm blocked while scanning")
    check(not Input.Dispatch(scanning, "OpenSettle"), "settle blocked in play")
    check(scanning:BillSum() == 0 and scanning.phase == Types.Phase.Scanning, "scan phase unchanged")

    local multi = new_session()
    replace_order(multi, {
        id = "k2",
        bHard = false,
        tender = 30,
        items = {
            { name = "水", price = 3 },
            { name = "面包", price = 8 },
        },
    })
    check(multi:ScanItem(1), "first of two")
    check(multi.phase == Types.Phase.Scanning, "still scanning")
    check(multi:ScannedSum() == 3, "partial S")
    local partial = View.Build(multi)
    check(partial.pay == nil and partial.payText == Types.PayPlaceholder, "partial scan hides pay")
    check(partial.amountDue == 3, "S is scanned sum, not the full basket")
    check(#View.ForbiddenKeys(partial) == 0, "partial leak")
    check(not multi:ScanItem(1), "first stays scanned")
    check(multi:ScanItem(2), "second")
    check(multi.phase == Types.Phase.PayShown, "all scanned")
    check(View.Build(multi).pay == 30, "pay arrives with PayShown")

    local reveal = new_session({ PayRevealSeconds = 0 })
    scan_all(reveal)
    check(reveal.phase == Types.Phase.MakingChange, "zero reveal lands in the drawer")
    local opened = View.Build(reveal)
    check(opened.pay == 5 and opened.showChangeSum and opened.changeSum == 0, "sigma 0 is visible and confirmable")
    check(opened.changeDue == nil, "still no C")

    local interrupted = new_session({ ShiftDurationSeconds = 0.4 })
    check(interrupted.phase == Types.Phase.Scanning, "customer arrived before the clock")
    interrupted:Advance(0.4)
    check(interrupted.phase == Types.Phase.ShiftSettle, "timer ends the shift")
    check(#interrupted.ledger == 0, "unconfirmed order is not a wrong order")
    local settled = View.Build(interrupted)
    check(settled.bHidePlayfield and settled.hudScoreVisible, "settle page")
    check(settled.shiftScore == 0 and settled.correctCount == 0, "empty shift")
    check(settled.pay == nil, "settle does not reveal an in-progress tender")
    check(settled.stars == nil, "no invented star table")

    local kept = new_session({ ShiftDurationSeconds = 5, ConfirmLockSeconds = 10 })
    play_optimal(kept)
    check(#kept.ledger == 1, "committed before the buzzer")
    kept:Advance(5)
    check(kept.phase == Types.Phase.ShiftSettle, "buzzer during confirm lock")
    check(#kept.ledger == 1 and kept.ledger[1].bCorrect, "committed order still counts")

    local store = {
        best = 0,
        Load = function(self)
            return self.best
        end,
        Commit = function(self, score)
            self.best = score
            return true
        end,
    }
    local scored = new_session({ ConfirmLockSeconds = 0, DepartSeconds = 0 }, store)
    play_optimal(scored)
    scored:ForceShiftSettle()
    local summary = scored.summary
    local expected = Score.ScoreOrder({
        bCorrect = true,
        bOptimal = true,
        secondsUsed = 0,
    }, scored.knobs)
    check(summary.score == expected, "shift score uses base+speed+optimal")
    check(summary.correctCount == 1 and summary.optimalCount == 1 and summary.penalty == 0, "counts")
    check(summary.bNewBest and summary.bestScore == expected, "new best")
    check(store.best == expected, "store wrote the best")

    local worse = new_session(nil, store)
    worse:ForceShiftSettle()
    check(worse.summary.bNewBest == false, "empty shift does not refresh")
    check(worse.summary.bestScore == expected, "previous best remains")

    local capped = Score.ScoreShift({
        { bCorrect = false, bOptimal = false, absDelta = 1000, secondsUsed = 0 },
    }, Knobs.Merge({ WrongPenaltyPerYuan = 5, WrongPenaltyCap = 200 }))
    check(capped.rawPenalty == 5000 and capped.penalty == 200, "penalty cap")
    check(capped.score == -200 and capped.correctCount == 0, "wrong order has no base score")

    local formula = Score.ScoreOrder({
        bCorrect = true,
        bOptimal = true,
        secondsUsed = 2.9,
    }, Knobs.Default())
    check(formula == 100 + (5 * (12 - 2)) + 40, "speed uses floored seconds")

    local deck = Order.NewDeck(Knobs.Merge({ OrderSource = "Procedural", Seed = 7 }))
    local zeros = 0
    local hards = 0
    local n = 200
    for _ = 1, n do
        local generated = Order.Next(deck)
        local total = Order.Total(generated)
        local k = #generated.items
        local changeDue = generated.tender - total
        check(k >= 1 and k <= 3, "procedural K")
        check(total >= 3 and total <= 40, "procedural S")
        check(generated.tender >= total, "pay is not below S")
        check(changeDue == Order.ChangeDue(generated), "C = Pay - S")
        if changeDue == 0 then
            zeros = zeros + 1
            check(not generated.bHard, "zero change is not hard")
        end
        if generated.bHard then
            hards = hards + 1
            check(Change.IsProceduralHard(changeDue, deck.knobs), "hard flag matches the stand-in")
        end
    end
    check(zeros / n > 0.15 and zeros / n < 0.40, "procedural C=0 ratio " .. zeros)
    check(hards / n > 0.25 and hards / n < 0.55, "procedural hard ratio " .. hards)

    local focus = new_session()
    check(focus.focus.kind == "item" and focus.focus.index == 1, "scanning focus")
    check(Input.Dispatch(focus, "ScanItem"), "focused scan")
    check(Input.Dispatch(focus, "FocusNext"), "focus in drawer")
    check(focus.focus.kind == "denom" and focus.focus.value == 5, "next denom")
    focus:ForceShiftSettle()
    check(focus.focus.kind == "settle", "settle focus")
    check(Input.Dispatch(focus, "OpenSettle"), "restart")
    check(focus.phase == Types.Phase.Scanning, "new shift scans again")

    if failed == 0 then
        io.write("cashier lua checks passed\n")
        return 0
    end
    io.stderr:write(string.format("%d cashier checks failed\n", failed))
    return 1
end

if arg and arg[0] and string.find(arg[0], "RunCashierTests", 1, true) then
    os.exit(run())
end

return { run = run }
