// Tencent is pleased to support the open source community by making UnLua available.
// 
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LuaDelegateHandler.generated.h"

namespace UnLua
{
    class FDelegateRegistry;
}

UCLASS()
/*
 * ULuaDelegateHandler 这个类简单理解，就是用来记录:
 * 1. 回调的 Lua 函数（LuaRef，Lua注册表中的位置，FDelegateRegistry::Bind的时候放进注册表的）
 * 2. Lua 回调函数是属于（定义在）哪个UObject对象中的（SelfObject）
 */
class UNLUA_API ULuaDelegateHandler : public UObject
{
    friend UnLua::FDelegateRegistry;

    GENERATED_BODY()

public:
    ULuaDelegateHandler();

    UFUNCTION()
    void Dummy();

    void BindTo(FScriptDelegate* InDelegate);

    void AddTo(FMulticastDelegateProperty* InProperty, void* InDelegate);

    virtual UWorld* GetWorld() const override;

    virtual void ProcessEvent(UFunction* Function, void* Parms) override;

    void RemoveFrom(FMulticastDelegateProperty* InProperty, void* InDelegate);

    virtual void BeginDestroy() override;

    void Reset();

private:
    /* 这里对UObject使用的是弱指针，因为Delegate不应该能够影响UObject的GC */
    TWeakObjectPtr<UObject> SelfObject;
    UnLua::FDelegateRegistry* Registry;
    int32 LuaRef;
    void* Delegate;
};
