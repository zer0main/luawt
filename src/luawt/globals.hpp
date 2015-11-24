/* luawt, Lua bindings for Wt
 * Copyright (c) 2015 Pavel Dolgov and Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#ifndef GLOBALS_HPP_
#define GLOBALS_HPP_

#include <cassert>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <typeinfo>

#include <boost/cast.hpp>

#include "boost-xtime.hpp"
#include <Wt/WApplication>
#include <Wt/WContainerWidget>
#include <Wt/WEnvironment>
#include <Wt/WServer>
#include <Wt/WText>

#include <lua.hpp>

#if LUA_VERSION_NUM == 501
#define my_setfuncs(L, funcs) luaL_register(L, 0, funcs)
#define my_equal lua_equal
#define my_rawlen lua_objlen
#else
#define my_setfuncs(L, funcs) luaL_setfuncs(L, funcs, 0)
#define my_equal(L, i, j) lua_compare(L, i, j, LUA_OPEQ)
#define my_rawlen lua_rawlen
#endif

using namespace Wt;

extern "C" {
int luaopen_luawt(lua_State* L);
}

class LuaWApplication : public WApplication {
public:
    LuaWApplication(lua_State* L,
                    const WEnvironment& env):
        WApplication(env), L_(L), owns_L_(false) {
        if (L == 0) {
            owns_L_ = true;
            L_ = luaL_newstate();
            luaL_openlibs(L_);
            luaopen_luawt(L_);
        }
    }

    ~LuaWApplication() {
        if (owns_L_) {
            lua_close(L_);
        }
    }

    static LuaWApplication* instance() {
        WApplication* wapp = WApplication::instance();
        return wapp ? boost::polymorphic_downcast
            <LuaWApplication*>(wapp) : 0;
    }

    lua_State* L() const {
        return L_;
    }

private:
    lua_State* L_;
    bool owns_L_;
};

template<typename T>
const char* luawt_typeToStr() {
    const char* name = typeid(T).name();
    // TODO use wrapper for assert
    assert(name != 0);
    assert(*name != '\0');
    // Example: _ZN2Wt7WServer
    // Remove _ from beginning of name
    return name + 1;
}

/* All Wt classes have metatables. Metatables have 2
   fields:
   - __base -- base class metatable
   - __name -- name of class
*/

template<typename T>
inline T* luawt_parseId(LuaWApplication* app, const char* id) {
    WWidget* widget = app->root()->findById(id);
    return boost::polymorphic_downcast<T*>(widget);
}

template<>
inline WEnvironment* luawt_parseId<WEnvironment>(
    LuaWApplication* app, const char* id) {
    const char* wanted = luawt_typeToStr<WEnvironment>();
    if (strcmp(id, wanted) == 0) {
        return const_cast<WEnvironment*>(&app->environment());
    } else {
        return 0;
    }
}

template<>
inline LuaWApplication* luawt_parseId<LuaWApplication>(
    LuaWApplication* app, const char* id) {
    const char* wanted = luawt_typeToStr<LuaWApplication>();
    if (strcmp(id, wanted) == 0) {
        return app;
    } else {
        return 0;
    }
}

template<typename T>
T* luawt_fromLua(lua_State* L, int index) {
    // Stack usage:
    // 1. table: mt
    // 2. string: name
    if (!lua_getmetatable(L, index)) {
        return 0;
    }
    const char* base_type = luawt_typeToStr<T>();
    while (true) {
        lua_getfield(L, -1, "__name");
        if (lua_type(L, -1) == LUA_TNIL) {
            lua_pop(L, 2); // metatable, field name
            return 0;
        }
        size_t name_len;
        const char* name = lua_tolstring(L, -1, &name_len);
        if (!name) {
            lua_pop(L, 2); // metatable, field name
            return 0;
        }
        if (memcmp(base_type, name, name_len) == 0) {
            // get mt of target type to ensure it is equal
            luaL_getmetatable(L, base_type);
            if (!my_equal(L, -1, -3)) {
                lua_pop(L, 3); // metatable, field name, mt2
                return 0;
            }
            lua_pop(L, 3); // metatable, field name, mt2
            void* raw_obj = lua_touserdata(L, index);
            char* id = reinterpret_cast<char*>(raw_obj);
            LuaWApplication* app =
                LuaWApplication::instance();
            if (!app) {
                return 0;
            } else {
                return luawt_parseId<T>(app, id);
            }
        } else {
            // go to next base class
            lua_pop(L, 1); // name
            lua_getfield(L, -1, "__base");
            lua_remove(L, -2);
            if (lua_type(L, -1) == LUA_TNIL) {
                lua_pop(L, 2); // metatable, field name
                return 0;
            }
        }
    }
}

template<typename T>
T* luawt_checkFromLua(lua_State* L, int index) {
    T* t = luawt_fromLua<T>(L, index);
    if (t == 0) {
        throw std::logic_error("LuaWt: Type mismatch or "
                               "no WApplication (no web "
                               "session)");
    } else {
        return t;
    }
}

/* In Lua: string with object ID instead of pointer
   WApplication::findWidget(), WObject::id()
*/
template<typename T>
inline void luawt_toLua(lua_State* L, T* obj) {
    size_t lobj_size = 1 + obj->id().size(); // with 0x00
    void* lobj = lua_newuserdata(L, lobj_size);
    std::string id = obj->id();
    memcpy(lobj, id.c_str(), id.size() + 1);
    luaL_getmetatable(L, luawt_typeToStr<T>());
    assert(lua_type(L, -1) == LUA_TTABLE);
    lua_setmetatable(L, -2);
}

template<>
inline void luawt_toLua<WEnvironment>(lua_State* L,
                                      WEnvironment* obj) {
    const char* id = luawt_typeToStr<WEnvironment>();
    size_t id_len = strlen(id) + 1;
    void* lobj = lua_newuserdata(L, id_len);
    memcpy(lobj, id, id_len);
    luaL_getmetatable(L, luawt_typeToStr<WEnvironment>());
    assert(lua_type(L, -1) == LUA_TTABLE);
    lua_setmetatable(L, -2);
}

template<>
inline void luawt_toLua<LuaWApplication>(lua_State* L,
                                         LuaWApplication* obj) {
    const char* id = luawt_typeToStr<LuaWApplication>();
    size_t id_len = strlen(id) + 1;
    void* lobj = lua_newuserdata(L, id_len);
    memcpy(lobj, id, id_len);
    luaL_getmetatable(L, luawt_typeToStr<LuaWApplication>());
    assert(lua_type(L, -1) == LUA_TTABLE);
    lua_setmetatable(L, -2);
}

template<lua_CFunction F>
struct wrap {
    static int func(lua_State* L) {
        try {
            return F(L);
        } catch (std::exception& e) {
            lua_pushstring(L, e.what());
        } catch (...) {
            lua_pushliteral(L, "Unknown exception");
        }
        return lua_error(L);
    }
};

template<typename T>
class LuaDeclareType {
public:
    static void declare(lua_State* L,
                        const luaL_Reg* mt,
                        const luaL_Reg* methods,
                        const char* base) {
        luaL_newmetatable(L, luawt_typeToStr<T>());
        // name
        lua_pushstring(L, luawt_typeToStr<T>());
        lua_setfield(L, -2, "__name");
        if (mt) {
            my_setfuncs(L, mt);
        }
        if (methods) {
            lua_newtable(L);
            my_setfuncs(L, methods);
            lua_setfield(L, -2, "__index");
        }
        if (base) {
            luaL_getmetatable(L, base);
            assert(lua_type(L, -1) == LUA_TTABLE);
            lua_setfield(L, -2, "__base");
        }
        // remove metatable from stack
        lua_pop(L, 1);
    }
};

#define METHOD(Klass, method) \
    {#method, wrap<luawt_##Klass##_##method>::func}

#define MT_METHOD(Klass, method) \
    {"__"#method, wrap<luawt_##Klass##_##method>::func}

#define DECLARE_CLASS(type, L, make, mt, \
                      methods, base) \
    LuaDeclareType<type>::declare(L, mt, methods, base); \
    if (make) { \
        luaL_getmetatable(L, "luawt"); \
        assert(lua_type(L, -1) == LUA_TTABLE); \
        lua_pushcfunction(L, make); \
        lua_setfield(L, -2, #type); \
        lua_pop(L, 1); \
    }

/* This functions are called from luaopen() */
void luawtShared(lua_State* L);
void luawtTest(lua_State* L);
void luawtWApplication(lua_State* L);
void luawtWContainerWidget(lua_State* L);
void luawtWEnvironment(lua_State* L);
void luawtWPushButton(lua_State* L);
void luawtWServer(lua_State* L);
void luawtWWidget(lua_State* L);

#endif
