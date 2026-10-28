-- Tunable magnitudes from the design doc. These numbers are scaffold knobs,
-- not a locked balance table. Retune them without changing the formula shape.

local Knobs = {}

function Knobs.Default()
    return {
        ShiftDurationSeconds = 120,
        CustomerArriveSeconds = 0.6,
        PayRevealSeconds = 0.6,
        ConfirmLockSeconds = 0.35,
        DepartSeconds = 0.5,

        -- Sample deck, or "Procedural" for the magnitude generator.
        OrderSource = "Sample",
        MinK = 1,
        MaxK = 3,
        MinS = 3,
        MaxS = 40,
        ZeroChangeRatio = 0.25,
        HardRatio = 0.40,
        -- Procedural stand-in only. Sample rows keep the authored hard flag.
        HardMinOnes = 3,
        HardMinBills = 4,
        Seed = 1,

        -- score = correct base + speed + optimal - min(wrong yuan * rate, cap)
        CorrectBase = 100,
        SpeedParSeconds = 12,
        SpeedBonusPerSecond = 5,
        OptimalBonus = 40,
        WrongPenaltyPerYuan = 5,
        WrongPenaltyCap = 200,
    }
end

function Knobs.Merge(overrides)
    local knobs = Knobs.Default()
    if overrides then
        for key, value in pairs(overrides) do
            knobs[key] = value
        end
    end
    return knobs
end

return Knobs
