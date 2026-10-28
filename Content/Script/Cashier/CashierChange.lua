-- Drawer math for the fixed set {1, 5, 10}. Greedy is optimal for this set.

local Types = require("Cashier.CashierTypes")

local Change = {}

local DENOMS_DESC = { 10, 5, 1 }

function Change.Sum(bills)
    local total = 0
    for i = 1, #bills do
        total = total + bills[i]
    end
    return total
end

function Change.OptimalBills(amount)
    if amount < 0 then
        return nil
    end
    local bills = {}
    local rest = amount
    for i = 1, #DENOMS_DESC do
        local denom = DENOMS_DESC[i]
        local count = math.floor(rest / denom)
        for _ = 1, count do
            bills[#bills + 1] = denom
        end
        rest = rest - count * denom
    end
    if rest ~= 0 then
        return nil
    end
    return bills
end

function Change.OptimalBillCount(amount)
    local bills = Change.OptimalBills(amount)
    if not bills then
        return nil
    end
    return #bills
end

function Change.IsProceduralHard(amount, knobs)
    if amount <= 0 then
        return false
    end
    local bills = Change.OptimalBillCount(amount)
    if not bills then
        return false
    end
    local ones = amount % 5
    return ones >= knobs.HardMinOnes or bills >= knobs.HardMinBills
end

function Change.IsLegal(value)
    return Types.IsDenom(value)
end

return Change
