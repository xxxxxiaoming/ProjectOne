-- Shift score shape from the design doc:
--   correct base + speed + optimal bonus - min(wrong-change penalty, cap)
-- Wrong orders add no base, speed, or optimal points. The penalty is applied
-- once at shift end on the summed absolute yuan miss, then capped.

local Score = {}

function Score.ScoreOrder(record, knobs)
    if not record.bCorrect then
        return 0
    end
    local seconds = math.floor(record.secondsUsed or 0)
    local under = knobs.SpeedParSeconds - seconds
    if under < 0 then
        under = 0
    end
    local optimal = 0
    if record.bOptimal then
        optimal = knobs.OptimalBonus
    end
    return knobs.CorrectBase + (knobs.SpeedBonusPerSecond * under) + optimal
end

function Score.ScoreShift(ledger, knobs)
    local gross = 0
    local wrongAbs = 0
    local correctCount = 0
    local optimalCount = 0
    for i = 1, #ledger do
        local record = ledger[i]
        gross = gross + Score.ScoreOrder(record, knobs)
        if record.bCorrect then
            correctCount = correctCount + 1
            if record.bOptimal then
                optimalCount = optimalCount + 1
            end
        else
            wrongAbs = wrongAbs + (record.absDelta or 0)
        end
    end
    local rawPenalty = wrongAbs * knobs.WrongPenaltyPerYuan
    local penalty = rawPenalty
    if penalty > knobs.WrongPenaltyCap then
        penalty = knobs.WrongPenaltyCap
    end
    return {
        score = gross - penalty,
        gross = gross,
        wrongAbs = wrongAbs,
        rawPenalty = rawPenalty,
        penalty = penalty,
        correctCount = correctCount,
        optimalCount = optimalCount,
    }
end

return Score
