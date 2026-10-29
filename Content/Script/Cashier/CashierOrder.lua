-- Order catalog and generator.
-- Sample rows are the design doc's 10-order table (section 6).
-- Combo lines stay one scannable card: the doc gives a single price, not unit prices.
-- Rows marked only as 难 have no product name; "杂货" is a display placeholder.

local Change = require("Cashier.CashierChange")

local Order = {}

Order.Sample = {
    { id = "sample-1", bHard = false, tender = 5, items = { { name = "水", price = 3 } } },
    { id = "sample-2", bHard = false, tender = 10, items = { { name = "面包", price = 8 } } },
    { id = "sample-3", bHard = true, tender = 20, items = { { name = "杂货", price = 17 } } },
    { id = "sample-4", bHard = false, tender = 10, items = { { name = "泡面+肠", price = 9 } } },
    { id = "sample-5", bHard = true, tender = 50, items = { { name = "杂货", price = 26 } } },
    { id = "sample-6", bHard = false, tender = 10, items = { { name = "饭团", price = 10 } } },
    { id = "sample-7", bHard = false, tender = 20, items = { { name = "牛奶+蕉", price = 15 } } },
    { id = "sample-8", bHard = true, tender = 50, items = { { name = "杂货", price = 4 } } },
    { id = "sample-9", bHard = false, tender = 10, items = { { name = "糖+纸巾", price = 10 } } },
    { id = "sample-10", bHard = true, tender = 50, items = { { name = "杂货", price = 39 } } },
}

local NAME_POOL = { "水", "面包", "泡面", "肠", "饭团", "牛奶", "蕉", "糖", "纸巾" }
local TENDERS = { 5, 10, 20, 50, 100 }

function Order.Total(order)
    local total = 0
    for i = 1, #order.items do
        total = total + order.items[i].price
    end
    return total
end

function Order.ChangeDue(order)
    return order.tender - Order.Total(order)
end

local function copyItems(items)
    local out = {}
    for i = 1, #items do
        out[i] = {
            name = items[i].name,
            price = items[i].price,
            bScanned = false,
        }
    end
    return out
end

function Order.Clone(row)
    return {
        id = row.id,
        bHard = row.bHard == true,
        tender = row.tender,
        items = copyItems(row.items),
    }
end

local function Rng(seed)
    local state = seed % 2147483647
    if state <= 0 then
        state = state + 2147483646
    end
    local rng = {}
    function rng:next()
        state = (state * 16807) % 2147483647
        return (state - 1) / 2147483646
    end
    function rng:int(a, b)
        if b < a then
            return a
        end
        return a + math.floor(self:next() * (b - a + 1))
    end
    return rng
end

Order.Rng = Rng

local function splitSum(total, k, rng)
    if k < 1 then
        k = 1
    end
    if k > total then
        k = total
    end
    local prices = {}
    local rest = total
    for i = 1, k - 1 do
        local maxForThis = rest - (k - i)
        local price = rng:int(1, maxForThis)
        prices[i] = price
        rest = rest - price
    end
    prices[k] = rest
    return prices
end

local function tendersAbove(amountDue)
    local list = {}
    for i = 1, #TENDERS do
        if TENDERS[i] > amountDue then
            list[#list + 1] = TENDERS[i]
        end
    end
    return list
end

function Order.GenerateProcedural(rng, knobs)
    local k = rng:int(knobs.MinK, knobs.MaxK)
    local minS = knobs.MinS
    if minS < k then
        minS = k
    end
    local maxS = knobs.MaxS
    if maxS < minS then
        maxS = minS
    end
    local amountDue = rng:int(minS, maxS)
    local prices = splitSum(amountDue, k, rng)

    local roll = rng:next()
    local category = "normal"
    if roll < knobs.ZeroChangeRatio then
        category = "zero"
    elseif roll < knobs.ZeroChangeRatio + knobs.HardRatio then
        category = "hard"
    end

    local tender
    local bHard
    if category == "zero" then
        tender = amountDue
        bHard = false
    else
        local picked = nil
        local candidates = tendersAbove(amountDue)
        for i = 1, #candidates do
            local pay = candidates[i]
            local hard = Change.IsProceduralHard(pay - amountDue, knobs)
            if (category == "hard" and hard) or (category == "normal" and not hard) then
                picked = pay
                break
            end
        end
        if picked then
            tender = picked
            bHard = Change.IsProceduralHard(tender - amountDue, knobs)
        elseif category == "hard" then
            local changeDue = 1
            while changeDue < 80 and not Change.IsProceduralHard(changeDue, knobs) do
                changeDue = changeDue + 1
            end
            tender = amountDue + changeDue
            bHard = true
        else
            tender = amountDue + 1
            bHard = false
        end
    end

    local offset = rng:int(0, #NAME_POOL - 1)
    local items = {}
    for i = 1, #prices do
        items[i] = {
            name = NAME_POOL[((i - 1 + offset) % #NAME_POOL) + 1],
            price = prices[i],
            bScanned = false,
        }
    end

    return {
        id = "proc",
        bHard = bHard,
        tender = tender,
        items = items,
    }
end

function Order.NewDeck(knobs)
    local rng = Rng(knobs.Seed or 1)
    if knobs.OrderSource == "Procedural" then
        return {
            mode = "Procedural",
            rng = rng,
            knobs = knobs,
            serial = 0,
        }
    end
    return {
        mode = "Sample",
        index = 1,
    }
end

function Order.Next(deck)
    if deck.mode == "Procedural" then
        deck.serial = deck.serial + 1
        local order = Order.GenerateProcedural(deck.rng, deck.knobs)
        order.id = "proc-" .. tostring(deck.serial)
        return order
    end
    local row = Order.Sample[deck.index]
    deck.index = deck.index + 1
    if deck.index > #Order.Sample then
        deck.index = 1
    end
    return Order.Clone(row)
end

return Order
