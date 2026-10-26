#include "LuaOverrides.h"
#include "LuaFunction.h"
#include "LuaOverridesClass.h"
#include "UObject/MetaData.h"

namespace UnLua
{
    FLuaOverrides& FLuaOverrides::Get()
    {
        static FLuaOverrides Override;
        return Override;
    }

    FLuaOverrides::FLuaOverrides()
    {
        GUObjectArray.AddUObjectDeleteListener(this);
    }

    void FLuaOverrides::NotifyUObjectDeleted(const UObjectBase* Object, int32 Index)
    {
        TWeakObjectPtr<ULuaOverridesClass> OverridesClass;
        if (Overrides.RemoveAndCopyValue((UClass*)Object, OverridesClass) )
        {
            if ( OverridesClass.IsValid( ) )
            {
                OverridesClass->Restore();
             }
         }
    }

    void FLuaOverrides::OnUObjectArrayShutdown()
    {
        GUObjectArray.RemoveUObjectDeleteListener(this);
    }

    void FLuaOverrides::Override(UFunction* Function, UClass* Class, FName NewName)
    {
        const auto OverridesClass = GetOrAddOverridesClass(Class); /* 搞一个OverrideClass(UClass对象)来管理ULuaFunction对象，而不是交给Class这个UClass对象来管，因为涉及到序列话的问题 */

        ULuaFunction* LuaFunction;
        const auto bAddNew = Function->GetOuter() != Class; /* bAddNew 为 true 时，说明要覆盖的UE函数是这个UObject对象的父类中的函数 */
        if (bAddNew)
        {
            const auto Exists = Class->FindFunctionByName(NewName, EIncludeSuperFlag::ExcludeSuper);
            if (Exists && Exists->GetSuperStruct() == Function)
                return;
        }
        else
        {
            LuaFunction = Cast<ULuaFunction>(Function);
            if (LuaFunction)
            {
                /*
                 * 这种情况是之前已经覆盖（或者说初始化过的了），对应的情况是改了脚本/蓝图中函数的实现，hotreload/recompile的情况
                 * 所以就不用执行后面创建ULuaFunction对象的代码了
                 */
                LuaFunction->Initialize();
                return;
            }
        }

        const auto OriginalFunctionFlags = Function->FunctionFlags;
        Function->FunctionFlags &= (~EFunctionFlags::FUNC_Native);

        /* 创建一个ULuaFunction对象，并将Function中的东西复制过来 */
        FObjectDuplicationParameters DuplicationParams(Function, OverridesClass);
        DuplicationParams.InternalFlagMask &= ~EInternalObjectFlags::Native;
        DuplicationParams.DestName = NewName;
        DuplicationParams.DestClass = ULuaFunction::StaticClass();
        LuaFunction = static_cast<ULuaFunction*>(StaticDuplicateObjectEx(DuplicationParams));

        Function->FunctionFlags = OriginalFunctionFlags;
        LuaFunction->FunctionFlags = OriginalFunctionFlags;

        /* 将创建出来的ULuaFunction对象挂到OverrideClass这个UClass对象上，由OverrideClass进行管理，这个是出于GC的目的 */
        LuaFunction->Next = OverridesClass->Children;
        OverridesClass->Children = LuaFunction;

        LuaFunction->StaticLink(true);
        LuaFunction->Initialize();  /* 主要是生成这个ULuaFunction对象的ULuaFunctionDesc，用来描述这个LuaFunction由多少个参数，参数类型，返回值之类的东西 */
        LuaFunction->Override(Function, Class, bAddNew); /* 真正的override在这里了 */
        LuaFunction->Bind();

        if (Class->IsRooted() || GUObjectArray.IsDisregardForGC(Class))
            LuaFunction->AddToRoot();
        else
            LuaFunction->AddToCluster(Class);
    }

    void FLuaOverrides::Restore(UClass* Class)
    {
        TWeakObjectPtr<ULuaOverridesClass> OverridesClass;
        if ( !Overrides.RemoveAndCopyValue( Class, OverridesClass) )
            return;
            
        if ( !OverridesClass.IsValid() )
            return;

        OverridesClass->Restore();
    }

    void FLuaOverrides::RestoreAll()
    {
        TArray<UClass*> Classes;
        Overrides.GenerateKeyArray(Classes);
        for (const auto& Class : Classes)
            Restore(Class);
    }

    void FLuaOverrides::Suspend(UClass* Class)
    {
        if (const auto Exists = Overrides.Find(Class))
        {
            if ( Exists != nullptr && Exists->IsValid() )
            {
                Exists->Get()->SetActive(false);
            }
        }
    }

    void FLuaOverrides::Resume(UClass* Class)
    {
        if (const auto Exists = Overrides.Find(Class))
        {
            if ( Exists != nullptr && Exists->IsValid() )
            {
                Exists->Get()->SetActive(true);
            }
        }
    }

    UClass* FLuaOverrides::GetOrAddOverridesClass(UClass* Class)
    {
        const auto Exists = Overrides.Find(Class);
        if ( Exists != nullptr && Exists->IsValid() )
            return Exists->Get();

        const auto OverridesClass = ULuaOverridesClass::Create(Class);
        Overrides.Add(Class, OverridesClass);
        return OverridesClass;
    }
}
