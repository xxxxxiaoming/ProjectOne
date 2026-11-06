-- Fixture: every task VM must evaluate this once with a fresh global environment.
AutoBlueprintModuleLoads = (AutoBlueprintModuleLoads or 0) + 1
return { loads = AutoBlueprintModuleLoads }
