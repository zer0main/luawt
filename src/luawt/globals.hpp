/* luawt, Lua bindings for Wt
 * Copyright (c) 2015-2017 Pavel Dolgov and Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#ifndef GLOBALS_HPP_
#define GLOBALS_HPP_

#include <algorithm>
#include <cassert>
#include <cstring>
#include <exception>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <typeinfo>

#include <boost/cast.hpp>
#include <boost/shared_ptr.hpp>

#include "boost-xtime.hpp"
#include <Wt/WApplication>
#include <Wt/WContainerWidget>
#include <Wt/WDefaultLoadingIndicator>
#include <Wt/WEnvironment>
#include <Wt/WObject>
#include <Wt/WOverlayLoadingIndicator>
#include <Wt/WServer>
#include <Wt/WText>

#include <lua.hpp>

#include "Global.hpp"
#include "enums.hpp"

#if LUA_VERSION_NUM == 501
#define my_setfuncs(L, funcs) luaL_register(L, 0, funcs)
#define my_equal lua_equal
#define my_rawlen lua_objlen
#define LUA_OK 0
#else
#define my_setfuncs(L, funcs) luaL_setfuncs(L, funcs, 0)
#define my_equal(L, i, j) lua_compare(L, i, j, LUA_OPEQ)
#define my_rawlen lua_rawlen
#endif

using namespace Wt;

extern "C" {
#ifdef LUAWTEST
int luaopen_luawtest(lua_State* L);
#else
int luaopen_luawt(lua_State* L);
#endif
}

void* luawt_getShared(lua_State* L);
void luawt_setShared(lua_State* L, void* sss);

class MyApplication : public WApplication {
public:
    MyApplication(
        lua_State* L,
        void* shared,
        const WEnvironment& env
    )
        : WApplication(env)
        , L_(L)
        , owns_L_(false)
    {
        if (L == 0) {
            owns_L_ = true;
            L_ = luaL_newstate();
            luaL_openlibs(L_);
            luawt_setShared(L_, shared);
#ifdef LUAWTEST
            luaopen_luawtest(L_);
#else
            luaopen_luawt(L_);
#endif
        }
    }

    ~MyApplication() {
        if (owns_L_) {
            lua_close(L_);
            L_ = 0;
        }
    }

    static MyApplication* instance() {
        WApplication* wapp = WApplication::instance();
        return wapp ? boost::polymorphic_downcast
            <MyApplication*>(wapp) : 0;
    }

    lua_State* L() const {
        return L_;
    }

private:
    lua_State* L_;
    bool owns_L_;
};

inline void checkPcallStatus(lua_State* L, int status) {
    if (status != LUA_OK) {
        const char* e = lua_tostring(L, -1);
        throw std::logic_error(e);
    }
}

inline lua_State* getLuaState() {
    MyApplication* app = MyApplication::instance();
    return app ? app->L() : 0;
}

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
inline T* luawt_parseId(MyApplication* app, const char* id) {
    WWidget* widget = app->root()->findById(id);
    return boost::polymorphic_downcast<T*>(widget);
}

template<>
inline WEnvironment* luawt_parseId<WEnvironment>(
    MyApplication* app, const char* id) {
    const char* wanted = luawt_typeToStr<WEnvironment>();
    if (strcmp(id, wanted) == 0) {
        return const_cast<WEnvironment*>(&app->environment());
    } else {
        return 0;
    }
}

template<>
inline MyApplication* luawt_parseId<MyApplication>(
    MyApplication* app, const char* id) {
    const char* wanted = luawt_typeToStr<MyApplication>();
    if (strcmp(id, wanted) == 0) {
        return app;
    } else {
        return 0;
    }
}

/*
T is the object's class or a parent of its class
Stack usage:
-1. mt of object or a parent
-2. mt of T
*/
template<typename T>
T* luawt_fromLua(lua_State* L, int index) {
    // get mt of the object
    if (!lua_getmetatable(L, index)) {
        return 0;
    }
    // get mt of target class to find it among ancestors
    const char* base_type = luawt_typeToStr<T>();
    luaL_getmetatable(L, base_type);
    // swap -1 and -2 to follow comment about stack
    lua_insert(L, -2);
    while (true) {
        if (my_equal(L, -1, -2)) {
            lua_pop(L, 2); // mt of T, mt of object
            void* raw_obj = lua_touserdata(L, index);
            char* id = reinterpret_cast<char*>(raw_obj);
            MyApplication* app =
                MyApplication::instance();
            if (!app) {
                return 0;
            } else {
                return luawt_parseId<T>(app, id);
            }
        } else {
            // go to next base class
            lua_getfield(L, -1, "__base");
            lua_remove(L, -2);
            if (lua_type(L, -1) != LUA_TTABLE) {
                lua_pop(L, 2); // mt of base, mt of object
                return 0;
            }
        }
    }
}

template<typename T>
T* luawt_checkFromLua(lua_State* L, int index) {
    int stack_size1 = lua_gettop(L);
    T* t = luawt_fromLua<T>(L, index);
    int stack_size2 = lua_gettop(L);
    assert(stack_size1 == stack_size2);
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
inline void luawt_toLua<WDefaultLoadingIndicator>(
    lua_State* L,
    WDefaultLoadingIndicator* obj
) {
    size_t lobj_size = 1 + obj->WText::id().size(); // with 0x00
    void* lobj = lua_newuserdata(L, lobj_size);
    std::string id = obj->WText::id();
    memcpy(lobj, id.c_str(), id.size() + 1);
    luaL_getmetatable(L, luawt_typeToStr<WDefaultLoadingIndicator>());
    assert(lua_type(L, -1) == LUA_TTABLE);
    lua_setmetatable(L, -2);
}

template<>
inline void luawt_toLua<WOverlayLoadingIndicator>(
    lua_State* L,
    WOverlayLoadingIndicator* obj
) {
    size_t lobj_size = 1 + obj->WContainerWidget::id().size(); // with 0x00
    void* lobj = lua_newuserdata(L, lobj_size);
    std::string id = obj->WContainerWidget::id();
    memcpy(lobj, id.c_str(), id.size() + 1);
    luaL_getmetatable(L, luawt_typeToStr<WOverlayLoadingIndicator>());
    assert(lua_type(L, -1) == LUA_TTABLE);
    lua_setmetatable(L, -2);
}

template<>
inline void luawt_toLua<WEnvironment>(
    lua_State* L,
    WEnvironment* obj
) {
    const char* id = luawt_typeToStr<WEnvironment>();
    size_t id_len = strlen(id) + 1;
    void* lobj = lua_newuserdata(L, id_len);
    memcpy(lobj, id, id_len);
    luaL_getmetatable(L, luawt_typeToStr<WEnvironment>());
    assert(lua_type(L, -1) == LUA_TTABLE);
    lua_setmetatable(L, -2);
}

template<>
inline void luawt_toLua<MyApplication>(
    lua_State* L,
    MyApplication* obj
) {
    const char* id = luawt_typeToStr<MyApplication>();
    size_t id_len = strlen(id) + 1;
    void* lobj = lua_newuserdata(L, id_len);
    memcpy(lobj, id, id_len);
    luaL_getmetatable(L, luawt_typeToStr<MyApplication>());
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

struct SlotWrapper {
    /* Slot func must be at the top of the stack. */
    SlotWrapper():
        app_(MyApplication::instance())
    {
        func_id_ = luaL_ref(app_->L(), LUA_REGISTRYINDEX);
    }

    ~SlotWrapper() {
        if (app_->L()) {
            luaL_unref(app_->L(), LUA_REGISTRYINDEX, func_id_);
        }
    }

    int func_id_;
    /* Use app_ member to access L. We can't keep L itself here
       because lua_close() is triggered first in some cases.
    */
    MyApplication* app_;
};

class SlotWrapperPtr {
public:
    SlotWrapperPtr():
        slot_wrapper_(new SlotWrapper) {
    }

    SlotWrapperPtr(const SlotWrapperPtr& other)
        : slot_wrapper_(other.slot_wrapper_)
    {
    }

    /* Will be called from Wt as slot. */
    template <typename T>
    void operator()(T event) {
        lua_State* L = getLuaState();
        if (!L) {
            throw std::logic_error(
                "LuaWt: no WApplication (no web session) when "
                "calling slot func."
            );
        }
        lua_rawgeti(
            L,
            LUA_REGISTRYINDEX,
            slot_wrapper_->func_id_
        );
        int status = lua_pcall(L, 0, 0, 0);
        checkPcallStatus(L, status);
    }

private:
    boost::shared_ptr<SlotWrapper> slot_wrapper_;
};

#define SET_SIGNAL_FIELD(signal, widget_type, field) \
    lua_pushcfunction( \
        L, \
        wrap<luawt_##widget_type##_##field##_##signal>::func \
    ); \
    lua_setfield(L, -2, #field);

#define GET_WIDGET(widget_type) \
    luaL_checktype(L, 1, LUA_TTABLE); \
    lua_getfield(L, 1, "widget"); \
    widget_type* widget = luawt_checkFromLua<widget_type>(L, -1); \
    lua_pop(L, 1);

#define CREATE_EMIT_SIGNAL_FUNC(signal, widget_type, event_for_emit) \
    int luawt_##widget_type##_emit_##signal(lua_State* L) { \
        GET_WIDGET(widget_type) \
        widget->signal().emit(event_for_emit()); \
        return 0; \
    }

#define CREATE_CONNECT_SIGNAL_FUNC(signal, widget_type) \
    int luawt_##widget_type##_connect_##signal(lua_State* L) { \
        GET_WIDGET(widget_type) \
        SlotWrapperPtr slot_wrapper; \
        widget->signal().connect(slot_wrapper); \
        return 0; \
    }

#define CREATE_SIGNAL_FUNC(signal, widget_type) \
    int luawt_##widget_type##_##signal(lua_State* L) { \
        lua_newtable(L); \
        lua_insert(L, -2); \
        lua_setfield(L, -2, "widget"); \
        SET_SIGNAL_FIELD(signal, widget_type, connect) \
        SET_SIGNAL_FIELD(signal, widget_type, emit) \
        return 1; \
    }

#define ADD_SIGNAL(signal, widget_type, event_for_emit) \
    CREATE_EMIT_SIGNAL_FUNC(signal, widget_type, event_for_emit) \
    CREATE_CONNECT_SIGNAL_FUNC(signal, widget_type) \
    CREATE_SIGNAL_FUNC(signal, widget_type)

template<typename T>
class luawt_DeclareType {
public:
    static void declare(
        lua_State* L,
        const luaL_Reg* mt,
        const luaL_Reg* methods,
        const char* base
    ) {
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
            // mt(__index) = base_mt
            // in order to enable base methods in current obj
            lua_getfield(L, -1, "__index");
            luaL_getmetatable(L, base);
            lua_setmetatable(L, -2);
            lua_pop(L, 1);
            // this_mt.__base = base_mt
            // in order to enable type conversions (curr --> base)
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
    luawt_DeclareType<type>::declare(L, mt, methods, base); \
    if (make) { \
        luaL_getmetatable(L, "luawt"); \
        assert(lua_type(L, -1) == LUA_TTABLE); \
        lua_pushcfunction(L, make); \
        lua_setfield(L, -2, #type); \
        lua_pop(L, 1); \
    }

inline bool luawt_ascendToBase(
    lua_State* L,
    std::string expected_name,
    std::string real_name
) {
    luaL_getmetatable(L, real_name.c_str());
    while (real_name.compare(expected_name)) {
        // go to next base class
        lua_getfield(L, -1, "__base");
        lua_remove(L, -2);
        if (lua_type(L, -1) != LUA_TTABLE) {
            lua_pop(L, 1); // base mt
            return false;
        }
        lua_getfield(L, -1, "__name");
        real_name = lua_tostring(L, -1);
        lua_pop(L, 1); // field name
    }
    lua_pop(L, 1); // base mt
    return true;
}

inline bool luawt_equalTypes(
    lua_State* L,
    int index,
    const char* expected_type
) {
    int real_type = lua_type(L, index);
    // The main result.
    bool result = false;
    // Auxiliary variables to parse `expected_type` strs.
    bool is_int = (strcmp(expected_type, "int") == 0);
    bool is_double = (strcmp(expected_type, "double") == 0);
    bool is_enum = (strcmp(expected_type, "enum") == 0);
    bool is_string = (strcmp(expected_type, "char const *") == 0);
    if (real_type == LUA_TNUMBER) {
        result = is_int || is_double || is_enum;
    } else if (real_type == LUA_TBOOLEAN) {
        result = (strcmp(expected_type, "bool") == 0);
    } else if (real_type == LUA_TSTRING) {
        result = is_string || is_enum;
    } else if (real_type == LUA_TUSERDATA) {
        std::string real_name;
        if (lua_getmetatable(L, index)) {
            lua_getfield(L, -1, "__name");
            real_name = lua_tostring(L, -1);
            // metatable; name field
            lua_pop(L, 2);
        }
        result = luawt_ascendToBase(L, std::string(expected_type), real_name);
    } else if (real_type == LUA_TTABLE) {
        result = is_enum;
    }
    return result;
}

/* Compare stack (L) and args group type by type. */
inline bool luawt_checkArgsGroup(
    lua_State* L,
    const char* const group[]
) {
    int stack_size = lua_gettop(L);
    int group_size = 0;
    while (group[group_size] != NULL) {
        group_size++;
    }
    if (stack_size != group_size) {
        return false;
    }
    for (int index = 0; index < stack_size; index++) {
        const char* expected_type = group[index];
        if (!luawt_equalTypes(L, index + 1, expected_type)) {
            return false;
        }
    }
    return true;
}

/* All overloads and different variants of optional arguments
   are given in `args_groups`. Function finds group corresponding
   to the given stack state (L arg).
*/
inline int luawt_getSuitableArgsGroup(
    lua_State* L,
    const char* const* const args_groups[]
) {
    int group_n = 0;
    while (args_groups[group_n] != NULL) {
        if (luawt_checkArgsGroup(L, args_groups[group_n])) {
            return group_n;
        }
        group_n++;
    }
    // Error, will be emitted as LuaL_error()
    return -1;
}

/* Facilities for dealing with enums. */

#define CALL_SET_ENUM_TABLE(enum_name) \
    luawt_setEnumTable( \
        L, \
        #enum_name, \
        luawt_enum_##enum_name##_val, \
        luawt_enum_##enum_name##_str \
    );

/* Set global enums table (luawt.enums). */
inline void luawt_setEnumsTable(lua_State* L) {
    lua_newtable(L);
    lua_setfield(L, -2, "enums");
}

/* Function creates and sets table to store enum values
   indexing by enum strings. For instance:
   `luawt.enums.EnumName['SomeFlag'] --> integer value of 'SomeFlag'`
   in the context of `EnumName`.
*/
inline void luawt_setEnumTable(
    lua_State* L,
    const char* enum_name,
    const lint enum_values[],
    const char* const enum_strings[]
) {
    luaL_getmetatable(L, "luawt");
    lua_getfield(L, -1, "enums");
    assert(lua_type(L, -1) == LUA_TTABLE);
    lua_getfield(L, -1, enum_name);
    if (lua_type(L, -1) != LUA_TNIL) {
        // Already exists.
        // luawt, luawt.enums, luawt.enums.GivenEnum.
        lua_pop(L, 3);
        return;
    }
    // nil
    lua_pop(L, 1);
    lua_newtable(L);
    int enum_n = 0;
    while (enum_strings[enum_n] != NULL) {
        lua_pushinteger(L, enum_values[enum_n]);
        lua_setfield(L, -2, enum_strings[enum_n]);
        enum_n++;
    }
    lua_setfield(L, -2, enum_name);
    // luawt, luawt.enums
    lua_pop(L, 2);
}

/* Sets all the enums tables, code is generated by script. */
inline void luawt_setEnumsTables(lua_State* L) {
    luawt_setEnumsTable(L);
    
    CALL_SET_ENUM_TABLE(WGLWidget_ClientSideRenderer)
    CALL_SET_ENUM_TABLE(WAbstractMedia_Options)
    CALL_SET_ENUM_TABLE(DomElementType)
    CALL_SET_ENUM_TABLE(WGLWidget_GLenum)
    CALL_SET_ENUM_TABLE(SelectionBehavior)
    CALL_SET_ENUM_TABLE(WMenuItem_LoadPolicy)
    CALL_SET_ENUM_TABLE(WMediaPlayer_MediaType)
    CALL_SET_ENUM_TABLE(WApplication_AjaxMethod)
    CALL_SET_ENUM_TABLE(Icon)
    CALL_SET_ENUM_TABLE(WMediaPlayer_Encoding)
    CALL_SET_ENUM_TABLE(WTreeNode_LoadPolicy)
    CALL_SET_ENUM_TABLE(WCalendar_HorizontalHeaderFormat)
    CALL_SET_ENUM_TABLE(WGoogleMap_MapTypeControl)
    CALL_SET_ENUM_TABLE(WGoogleMap_ApiVersion)
    CALL_SET_ENUM_TABLE(WAbstractItemView_EditTrigger)
    CALL_SET_ENUM_TABLE(WMediaPlayer_TextId)
    CALL_SET_ENUM_TABLE(WMediaPlayer_ButtonControlId)
    CALL_SET_ENUM_TABLE(WSuggestionPopup_PopupTrigger)
    CALL_SET_ENUM_TABLE(WTreeNode_ChildCountPolicy)
    CALL_SET_ENUM_TABLE(PositionScheme)
    CALL_SET_ENUM_TABLE(SelectionMode)
    CALL_SET_ENUM_TABLE(Orientation)
    CALL_SET_ENUM_TABLE(RenderFlag)
    CALL_SET_ENUM_TABLE(AnchorTarget)
    CALL_SET_ENUM_TABLE(AlignmentFlag)
    CALL_SET_ENUM_TABLE(WMediaPlayer_ReadyState)
    CALL_SET_ENUM_TABLE(WScrollArea_ScrollBarPolicy)
    CALL_SET_ENUM_TABLE(TextFormat)
    CALL_SET_ENUM_TABLE(WTabWidget_LoadPolicy)
    CALL_SET_ENUM_TABLE(StandardButton)
    CALL_SET_ENUM_TABLE(CheckState)
    CALL_SET_ENUM_TABLE(Side)
    CALL_SET_ENUM_TABLE(WMediaPlayer_BarControlId)
    CALL_SET_ENUM_TABLE(PaintFlag)
    CALL_SET_ENUM_TABLE(WPaintedWidget_Method)
    CALL_SET_ENUM_TABLE(SortOrder)
    CALL_SET_ENUM_TABLE(WValidator_State)
    CALL_SET_ENUM_TABLE(WContainerWidget_Overflow)
    CALL_SET_ENUM_TABLE(WLineEdit_EchoMode)
    CALL_SET_ENUM_TABLE(WDialog_DialogCode)
    CALL_SET_ENUM_TABLE(WAbstractItemView_EditOption)
    CALL_SET_ENUM_TABLE(WAbstractMedia_PreloadMode)
    CALL_SET_ENUM_TABLE(WSlider_TickPosition)
    CALL_SET_ENUM_TABLE(MatchFlag)
    CALL_SET_ENUM_TABLE(LayoutDirection)
    CALL_SET_ENUM_TABLE(WAbstractMedia_ReadyState)
    CALL_SET_ENUM_TABLE(MetaHeaderType)
}

/* Get enum value corresponding to the given enum string. */
inline lint luawt_enumStrToValue(
    lua_State* L,
    const char* const enum_strings[],
    const lint enum_values[],
    int index
) {
    if (lua_type(L, index) != LUA_TSTRING) {
        return luaL_error(L, "Enum array must contain only strings");
    }
    int enum_index = luaL_checkoption(
        L,
        index,
        NULL,
        enum_strings
    );
    return enum_values[enum_index];
}

/* Function examines all possible options of representing enums,
   chooses the appropriate one and converts it to `lint`.
*/
inline lint luawt_getEnum(
    lua_State* L,
    const char* const enum_strings[],
    const lint enum_values[],
    int index,
    const char* error_message
) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        // The simplest case: we have the value as a number.
        return lua_tointeger(L, index);
    } else if (lua_type(L, index) == LUA_TSTRING) {
        // Convert from string to `lint`.
        return luawt_enumStrToValue(L, enum_strings, enum_values, index);
    } else if (lua_type(L, index) == LUA_TTABLE) {
        // 'Special' enum (with bitwise different values).
        lint result = 0;
        for (int i = 1; i <= my_rawlen(L, index); i++) {
            lua_pushinteger(L, i);
            lua_gettable(L, index);
            result |= luawt_enumStrToValue(
                L,
                enum_strings,
                enum_values,
                -1
            );
            // Enum string.
            lua_pop(L, 1);
        }
        return result;
    } else {
        return luaL_error(L, error_message);
    }
}

/* Check whether `enum_name` is in global registry of special enums.
*/
inline bool luawt_isSpecialEnum(const char* enum_name) {
    int index = 0;
    while (luawt_SpecialEnums_arr[index] != NULL) {
        if (!strcmp(enum_name, luawt_SpecialEnums_arr[index])) {
            return true;
        }
        index++;
    }
    return false;
}

/* Return enum in the form of string or table([str] -> val).
   Use tables only for representing enums with bitwise different
   values, in case of combination of multiple values in flag enums.
*/
inline void luawt_returnEnum(
    lua_State* L,
    const char* const enum_strings[],
    const lint enum_values[],
    lint enum_value,
    const char* enum_name
) {
    if (luawt_isSpecialEnum(enum_name)) {
        // 'Special' enum (bitwise different values).
        int index = 0;
        lua_newtable(L);
        while (enum_strings[index] != NULL) {
            if (enum_values[index] & enum_value) {
                lua_pushstring(L, enum_strings[index]);
                lua_pushinteger(L, enum_values[index]);
                lua_settable(L, -3);
            }
            index++;
        }
    } else {
        // Simple case: value -> string.
        int index = 0;
        while (enum_strings[index] != NULL) {
            if (enum_values[index] == enum_value) {
                lua_pushstring(L, enum_strings[index]);
                return;
            }
            index++;
        }
        throw std::logic_error("LuaWt: error enum value not found.");
    }
}

/* These functions are called from luaopen() */
void luawt_MyApplication(lua_State* L);
void luawt_Shared(lua_State* L);
void luawt_Test(lua_State* L);
void luawt_WAbstractItemView(lua_State* L);
void luawt_WAbstractMedia(lua_State* L);
void luawt_WAbstractSpinBox(lua_State* L);
void luawt_WAbstractToggleButton(lua_State* L);
void luawt_WAnchor(lua_State* L);
void luawt_WAudio(lua_State* L);
void luawt_WBreak(lua_State* L);
void luawt_WCalendar(lua_State* L);
void luawt_WCheckBox(lua_State* L);
void luawt_WComboBox(lua_State* L);
void luawt_WCompositeWidget(lua_State* L);
void luawt_WContainerWidget(lua_State* L);
void luawt_WDateEdit(lua_State* L);
void luawt_WDatePicker(lua_State* L);
void luawt_WDefaultLoadingIndicator(lua_State* L);
void luawt_WDialog(lua_State* L);
void luawt_WDoubleSpinBox(lua_State* L);
void luawt_WEnvironment(lua_State* L);
void luawt_WFileUpload(lua_State* L);
void luawt_WFlashObject(lua_State* L);
void luawt_WFormWidget(lua_State* L);
void luawt_WGLWidget(lua_State* L);
void luawt_WGoogleMap(lua_State* L);
void luawt_WGroupBox(lua_State* L);
void luawt_WIconPair(lua_State* L);
void luawt_WImage(lua_State* L);
void luawt_WInPlaceEdit(lua_State* L);
void luawt_WInteractWidget(lua_State* L);
void luawt_WLabel(lua_State* L);
void luawt_WLineEdit(lua_State* L);
void luawt_WMediaPlayer(lua_State* L);
void luawt_WMenu(lua_State* L);
void luawt_WMenuItem(lua_State* L);
void luawt_WMessageBox(lua_State* L);
void luawt_WNavigationBar(lua_State* L);
void luawt_WOverlayLoadingIndicator(lua_State* L);
void luawt_WPaintedWidget(lua_State* L);
void luawt_WPanel(lua_State* L);
void luawt_WPopupMenu(lua_State* L);
void luawt_WPopupWidget(lua_State* L);
void luawt_WProgressBar(lua_State* L);
void luawt_WPushButton(lua_State* L);
void luawt_WRadioButton(lua_State* L);
void luawt_WScrollArea(lua_State* L);
void luawt_WSelectionBox(lua_State* L);
void luawt_WSlider(lua_State* L);
void luawt_WSpinBox(lua_State* L);
void luawt_WSplitButton(lua_State* L);
void luawt_WStackedWidget(lua_State* L);
void luawt_WSuggestionPopup(lua_State* L);
void luawt_WTabWidget(lua_State* L);
void luawt_WTable(lua_State* L);
void luawt_WTableCell(lua_State* L);
void luawt_WTableView(lua_State* L);
void luawt_WTemplate(lua_State* L);
#ifdef LUAWTEST
void luawt_WTestEnvironment(lua_State* L);
#else
void luawt_WServer(lua_State* L);
#endif
void luawt_WTemplateFormView(lua_State* L);
void luawt_WText(lua_State* L);
void luawt_WTextArea(lua_State* L);
void luawt_WTextEdit(lua_State* L);
void luawt_WTimerWidget(lua_State* L);
void luawt_WToolBar(lua_State* L);
void luawt_WTree(lua_State* L);
void luawt_WTreeNode(lua_State* L);
void luawt_WTreeTable(lua_State* L);
void luawt_WTreeTableNode(lua_State* L);
void luawt_WTreeView(lua_State* L);
void luawt_WValidationStatus(lua_State* L);
void luawt_WVideo(lua_State* L);
void luawt_WViewWidget(lua_State* L);
void luawt_WVirtualImage(lua_State* L);
void luawt_WWebWidget(lua_State* L);
void luawt_WWidget(lua_State* L);

#endif
