local TestObject = UnLua.Class()

function TestObject:ReceiveBeginPlay()
    print("BP_Test BeginPlay from lua")
end

return TestObject