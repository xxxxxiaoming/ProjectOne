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

#include "ObjectRegistry.h"
#include "LowLevel.h"
#include "LuaEnv.h"
#include "UnLuaDelegates.h"

namespace UnLua
{
    static const char* REGISTRY_KEY = "UnLua_ObjectMap"; /* UObject* : RAW_UOBJECT(full user data，没有绑定脚本的情况) / INSTANCE Table对象（有绑定脚本的情况）. 这是一个 weakvaluetable，就是说，这个table对value相当于，没有引用  */
    static const char* MANUAL_REF_PROXY_MAP = "UnLua_ManualRefProxyMap"; /* 在Lua脚本端管理的Obejcts, key: UObject*, value:FManualRefProxy userdata  */

    static int ReleaseSharedPtr(lua_State* L)
    {
        const auto Ptr = (TSharedPtr<void>*)lua_touserdata(L, 1);
        Ptr->Reset();
        return 0;
    }

    /*
     * 这个是 FManualRefProxy 的 __gc 元方法，不要跟 FOjectRegistry::RemoveManualRef 函数弄混了
     * 一个栗子：
     * 1. 脚本中调用 UnLua.UnRef(obj) 释放object引用 
     * 2. FOjectRegistry::RemoveManualRef，这个函数会将 UnLua_ManualRefProxyMap key等于obj的value置为nil
     * 3. 于是，这个obj对应的proxy就没有引用了，Lua GC的时候，就会被回收
     * 4. 触发__gc来到这个函数
     */
    static int ReleaseManualRef(lua_State* L)
    {
        auto& Env = FLuaEnv::FindEnvChecked(L);
        const auto Proxy = (FManualRefProxy*)lua_touserdata(L, 1);
        const auto Object = Proxy->Object.Get();
        if (!Object)
            return 0;

        lua_getfield(L, LUA_REGISTRYINDEX, MANUAL_REF_PROXY_MAP);
        lua_pushlightuserdata(L, Object);
        if (lua_rawget(L, -2) == LUA_TNIL)
            Env.RemoveManualObjectReference(Object);
        return 0;
    }

    FObjectRegistry::FObjectRegistry(FLuaEnv* Env)
        : Env(Env)
    {
        const auto L = Env->GetMainState();

        lua_pushstring(L, REGISTRY_KEY);
        LowLevel::CreateWeakValueTable(L);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushstring(L, MANUAL_REF_PROXY_MAP);
        LowLevel::CreateWeakValueTable(L);
        lua_rawset(L, LUA_REGISTRYINDEX);
        
        luaL_newmetatable(L, "TSharedPtr");
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, ReleaseSharedPtr);
        lua_rawset(L, -3);

        luaL_newmetatable(L, "UnLuaManualRefProxy");
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, ReleaseManualRef);
        lua_rawset(L, -3);

        lua_pop(L, 2);
    }

    void FObjectRegistry::NotifyUObjectDeleted(UObject* Object)
    {
        Unbind(Object);
    }

    void FObjectRegistry::NotifyUObjectLuaGC(UObject* Object)
    {
        Env->AutoObjectReference.Remove(Object);
    }

    /*
     * 没有绑定脚本的UObject对象会执行到这个函数。（根据AI的说法）
     */
    void FObjectRegistry::Push(lua_State* L, UObject* Object)
    {
        if (!Object)
        {
            lua_pushnil(L);
            return;
        }

        // avoid invalid ptrs in containers from lua
        if (!UnLua::IsUObjectValid(Object))
        {
            luaL_error(L, "attempt to read invalid uobject ptr from lua, maybe from containers like TArray.");
            return;
        }

        lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        lua_pushlightuserdata(L, Object);
        const auto Type = lua_rawget(L, -2);
        /*
         * 检查UnLua_ObjectMap是否存在这个Object对应的UData对象了
         * 如果有（Type ！= LUA_TNIL）,就不用创建UData对象了，需要保证Lua脚本中，多个地方访问这个UObject对象的时候，都是拿到同一个UData对象
         * 如果没有（Type == LUA_TNIL）,那就需要为这个UData对象创建UDdata对象，缓存到UnLua_ObjectMap中。
         */
        if (Type == LUA_TNIL)
        {
            lua_pop(L, 1);
            PushObjectCore(L, Object);
            lua_pushlightuserdata(L, Object);
            lua_pushvalue(L, -2);
            lua_rawset(L, -4);
            ObjectRefs.Add(Object, LUA_NOREF);
        }
        lua_remove(L, -2);
    }

    /*
     * 经过这个函数的一系列操作，最终
     * 1. Registry中，存一个Table（这里叫它INSTANCE），这个表的结构是这样的：
     * {
     *      Object = RAW_UOBJECT //一个Full UserData对象，指向UObject对象的地址
     *      metatable = REQUIRED_MODULE // 这个UObject对象绑定的Lua脚本Table
     *      // REQUIRED_MODULE.metatable = METATABLE_UOBJECT
     * }
     * 2. 1中的这个Table在Registry中的索引会存入OjectRefs这个TMap中
     */
    int FObjectRegistry::Bind(UObject* Object)
    {
        if (const auto Exists = ObjectRefs.Find(Object))
        {
            /*
             * 此前已经为这个UObject对象创建（绑定）Lua Table对象了，
             * 就无须再创建新的 Lua Table 对象了，直接返回已经创建的那个 Lua Table 对象就OK了。
             */
            if (*Exists != LUA_NOREF)
                return *Exists;
        }

        const auto L = Env->GetMainState();

        int OldTop = lua_gettop(L);

        lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        lua_pushlightuserdata(L, Object);
        lua_newtable(L); // create a Lua table ('INSTANCE')
        /*
         * PushObjectCore的完整作用：
         * 1. 往 Lua 栈上创建一个指向一个Full Usedata(UData)对象（这里姑且就叫这个对象RAW_UOBJECT）的TValue对象
         * 2. 为这个 RAW_UOBJECT 对象设置metatable
         */
        PushObjectCore(L, Object); // push UObject ('RAW_UOBJECT')
        lua_pushstring(L, "Object");
        lua_pushvalue(L, -2);
        lua_rawset(L, -4); // INSTANCE.Object = RAW_UOBJECT

        // in some case may occur module or object metatable can 
        // not be found problem
        const auto Class = Object->IsA<UClass>() ? static_cast<UClass*>(Object) : Object->GetClass();
        const auto ClassBoundRef = Env->GetManager()->GetBoundRef(Class);
        int32 TypeModule = lua_rawgeti(L, LUA_REGISTRYINDEX, ClassBoundRef); // push the required module/table ('REQUIRED_MODULE') to the top of the stack
        int32 TypeMetatable = lua_getmetatable(L, -2); // get the metatable ('METATABLE_UOBJECT') of 'RAW_UOBJECT' 
        if (TypeModule != LUA_TTABLE || TypeMetatable == LUA_TNIL)
        {
            /*
             * 如果这个UObject对象没有绑定脚本，执行到这里就返回了。
             */
            lua_pop(L, lua_gettop(L) - OldTop);
            return LUA_REFNIL;
        }

#if ENABLE_CALL_OVERRIDDEN_FUNCTION
        lua_pushstring(L, "Overridden");
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
#endif
        lua_setmetatable(L, -2); // REQUIRED_MODULE.metatable = METATABLE_UOBJECT
        lua_setmetatable(L, -3); // INSTANCE.metatable = REQUIRED_MODULE
        lua_pop(L, 1);

        lua_pushvalue(L, -1);
        const auto Ret = luaL_ref(L, LUA_REGISTRYINDEX);
        ObjectRefs.Add(Object, Ret);

        FUnLuaDelegates::OnObjectBinded.Broadcast(Object); // 'INSTANCE' is on the top of stack now

        lua_rawset(L, -3);
        lua_pop(L, 1);
        return Ret;
    }

    bool FObjectRegistry::IsBound(const UObject* Object) const
    {
        const auto Exists = ObjectRefs.Find(Object);
        return Exists && *Exists != LUA_NOREF;
    }

    int FObjectRegistry::GetBoundRef(const UObject* Object) const
    {
        const auto Ref = ObjectRefs.Find(Object);
        if (Ref)
            return *Ref;
        return LUA_NOREF;
    }

    void FObjectRegistry::Unbind(UObject* Object)
    {
        int32 Ref;
        if (!ObjectRefs.RemoveAndCopyValue(Object, Ref))
            return;

        const auto L = Env->GetMainState();
        const auto Top = lua_gettop(L);
        RemoveFromObjectMapAndPushToStack(Object);

        if (Ref == LUA_NOREF)
        {
            if (lua_isnil(L, -1))
            {
                lua_pop(L, 1);
                return;
            }
            check(lua_isuserdata(L, -1));
            bool bTwoLvlPtr;
            void* Userdata = GetUserdataFast(L, -1, &bTwoLvlPtr);
            check(bTwoLvlPtr)
            *((void**)Userdata) = (void*)LowLevel::ReleasedPtr;
            lua_settop(L, Top);
            return;
        }

        check(lua_istable(L, -1));
        luaL_unref(L, LUA_REGISTRYINDEX, Ref);
        FUnLuaDelegates::OnObjectUnbinded.Broadcast(Object); // object instance ('INSTANCE') is on the top of stack now

        lua_pushstring(L, "Object");
        lua_rawget(L, -2);
        void* Userdata = lua_touserdata(L, -1);
        *((void**)Userdata) = (void*)LowLevel::ReleasedPtr;

        lua_settop(L, Top);
    }

    /*
     * 给 Lua 脚本端提供一个方法，能够在Lua脚本端“引用”住UE中的Object （UnLuaLib中的Ref函数就是调用这个地方，也就是脚本中可以UnLua.Ref(obj)）
     * 防止UE中的Object被GC，而Lua脚本端却还需要用到这个Object
     */
    void FObjectRegistry::AddManualRef(lua_State* L, UObject* Object)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, MANUAL_REF_PROXY_MAP);
        lua_pushlightuserdata(L, Object);
        if (lua_rawget(L, -2) == LUA_TNIL)
        {
            lua_pop(L, 1);
            Env->AddManualObjectReference(Object);
            auto Ptr = lua_newuserdata(L, sizeof(FManualRefProxy));
            auto Proxy = new(Ptr)FManualRefProxy;
            Proxy->Object = Object;
            /* "UnLuaManualRefProxy" 这个 Lua Table 是在 FObjectRegistry 的构造函数中创建的，又一个__gc函数 */
            luaL_getmetatable(L, "UnLuaManualRefProxy");
            lua_setmetatable(L, -2);
            lua_pushlightuserdata(L, Object);
            lua_pushvalue(L, -2);
            lua_rawset(L, -4);
        }
        lua_remove(L, -2);
    }

    /*
     * 给 Lua 脚本端提供一个方法，能够在Lua脚本端“释放”掉对UE中的Object的引用 （UnLuaLib中的UnRef函数就是调用这个地方，也就是脚本中可以UnLua.Ref(obj)）
     */
    void FObjectRegistry::RemoveManualRef(UObject* Object)
    {
        const auto L = Env->GetMainState();
        lua_getfield(L, LUA_REGISTRYINDEX, MANUAL_REF_PROXY_MAP);
        lua_pushlightuserdata(L, Object);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        Env->RemoveManualObjectReference(Object);
    }

    void FObjectRegistry::RemoveFromObjectMapAndPushToStack(UObject* Object)
    {
        const auto L = Env->GetMainState();
        lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        lua_pushlightuserdata(L, Object);
        lua_rawget(L, -2);
        lua_pushlightuserdata(L, Object);
        lua_pushnil(L);
        lua_rawset(L, -4);
        lua_remove(L, -2);
    }
}
