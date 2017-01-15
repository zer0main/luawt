#include "boost-xtime.hpp"

#include <Wt/WVideo>
#include <Wt/WContainerWidget>

#include "globals.hpp"

static const char* WVideo_make_args0[] = {NULL};
static const char* WVideo_make_args1[] = {luawt_typeToStr<Wt::WContainerWidget>(), NULL};
static const char* const* const luawt_WVideo_make_args[] = {WVideo_make_args0, WVideo_make_args1, NULL};

int luawt_WVideo_make(lua_State* L) {
    int index = luawt_getSuitableArgsGroup(L, luawt_WVideo_make_args);
    if (index == 0) {
    WVideo * result = new WVideo();
    luawt_toLua(L, result);
    return 1;

    } else if (index == 1) {
    Wt::WContainerWidget* parent =
        luawt_checkFromLua<Wt::WContainerWidget>(L, 1);
    WVideo * result = new WVideo(parent);
    luawt_toLua(L, result);
    return 1;

    } else {
        return luaL_error(L, "Wrong arguments for WVideo.make");
    }
}

static const char* WVideo_jsVideoRef_args0[] = {luawt_typeToStr<WVideo>(), NULL};
static const char* const* const luawt_WVideo_jsVideoRef_args[] = {WVideo_jsVideoRef_args0, NULL};

int luawt_WVideo_jsVideoRef(lua_State* L) {
    int index = luawt_getSuitableArgsGroup(L, luawt_WVideo_jsVideoRef_args);
    WVideo* self = luawt_checkFromLua<WVideo>(L, 1);
    if (index == 0) {
    std::string result = self->jsVideoRef();
    lua_pushstring(L, result.c_str());
    return 1;

    } else {
        return luaL_error(L, "Wrong arguments for WVideo.jsVideoRef");
    }
}

static const char* WVideo_setPoster_args0[] = {luawt_typeToStr<WVideo>(), "char const *", NULL};
static const char* const* const luawt_WVideo_setPoster_args[] = {WVideo_setPoster_args0, NULL};

int luawt_WVideo_setPoster(lua_State* L) {
    int index = luawt_getSuitableArgsGroup(L, luawt_WVideo_setPoster_args);
    WVideo* self = luawt_checkFromLua<WVideo>(L, 1);
    if (index == 0) {
    char const * raw2 = lua_tostring(L, 2);
    std::string url = std::string(raw2);
    self->setPoster(url);
    return 0;
    
    } else {
        return luaL_error(L, "Wrong arguments for WVideo.setPoster");
    }
}

ADD_SIGNAL(playbackStarted, WVideo, Wt::NoClass)
ADD_SIGNAL(playbackPaused, WVideo, Wt::NoClass)
ADD_SIGNAL(ended, WVideo, Wt::NoClass)
ADD_SIGNAL(timeUpdated, WVideo, Wt::NoClass)
ADD_SIGNAL(volumeChanged, WVideo, Wt::NoClass)
ADD_SIGNAL(keyWentDown, WVideo, Wt::WKeyEvent)
ADD_SIGNAL(keyPressed, WVideo, Wt::WKeyEvent)
ADD_SIGNAL(keyWentUp, WVideo, Wt::WKeyEvent)
ADD_SIGNAL(enterPressed, WVideo, Wt::NoClass)
ADD_SIGNAL(escapePressed, WVideo, Wt::NoClass)
ADD_SIGNAL(clicked, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(doubleClicked, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(mouseWentDown, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(mouseWentUp, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(mouseWentOut, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(mouseWentOver, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(mouseMoved, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(mouseDragged, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(mouseWheel, WVideo, Wt::WMouseEvent)
ADD_SIGNAL(touchStarted, WVideo, Wt::WTouchEvent)
ADD_SIGNAL(touchEnded, WVideo, Wt::WTouchEvent)
ADD_SIGNAL(touchMoved, WVideo, Wt::WTouchEvent)
ADD_SIGNAL(gestureStarted, WVideo, Wt::WGestureEvent)
ADD_SIGNAL(gestureChanged, WVideo, Wt::WGestureEvent)
ADD_SIGNAL(gestureEnded, WVideo, Wt::WGestureEvent)

static const luaL_Reg luawt_WVideo_methods[] = {
    METHOD(WVideo, setPoster),
    METHOD(WVideo, jsVideoRef),
    METHOD(WVideo, playbackStarted),
    METHOD(WVideo, playbackPaused),
    METHOD(WVideo, ended),
    METHOD(WVideo, timeUpdated),
    METHOD(WVideo, volumeChanged),
    METHOD(WVideo, keyWentDown),
    METHOD(WVideo, keyPressed),
    METHOD(WVideo, keyWentUp),
    METHOD(WVideo, enterPressed),
    METHOD(WVideo, escapePressed),
    METHOD(WVideo, clicked),
    METHOD(WVideo, doubleClicked),
    METHOD(WVideo, mouseWentDown),
    METHOD(WVideo, mouseWentUp),
    METHOD(WVideo, mouseWentOut),
    METHOD(WVideo, mouseWentOver),
    METHOD(WVideo, mouseMoved),
    METHOD(WVideo, mouseDragged),
    METHOD(WVideo, mouseWheel),
    METHOD(WVideo, touchStarted),
    METHOD(WVideo, touchEnded),
    METHOD(WVideo, touchMoved),
    METHOD(WVideo, gestureStarted),
    METHOD(WVideo, gestureChanged),
    METHOD(WVideo, gestureEnded),
    {NULL, NULL},
};

void luawt_WVideo(lua_State* L) {
    const char* base = luawt_typeToStr<WAbstractMedia>();
    assert(base);
    DECLARE_CLASS(
        WVideo,
        L,
        wrap<luawt_WVideo_make>::func,
        0,
        luawt_WVideo_methods,
        base
    );
}