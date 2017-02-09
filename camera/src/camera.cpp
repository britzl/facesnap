#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define LIB_NAME "Camera"
#define MODULE_NAME "camera"

// Defold SDK
#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_OSX)

#include "camera_private.h"

struct DefoldCamera
{
    int m_VideoBufferLuaRef;

    dmBuffer::HBuffer m_VideoBuffer;

    CameraInfo m_Params;
};

DefoldCamera g_DefoldCamera;


static int StartCapture(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

	int status = CameraPlatform_StartCapture(&g_DefoldCamera.m_VideoBuffer, CAMERA_TYPE_FRONT, g_DefoldCamera.m_Params);

	lua_pushboolean(L, status > 0);
	if( status == 0 )
	{
        printf("capture failed!\n");
		return 1;
	}

    printf("camera.cpp: Camera Size: %u x %u\n", g_DefoldCamera.m_Params.m_Width, g_DefoldCamera.m_Params.m_Height);

    const uint32_t size = g_DefoldCamera.m_Params.m_Width * g_DefoldCamera.m_Params.m_Height;

    printf("camera.cpp: SIZE: %u\n", size);

    {
	    uint8_t* data = 0;
	    uint32_t datasize = 0;
	    dmBuffer::GetBytes(g_DefoldCamera.m_VideoBuffer, (void**)&data, &datasize);
	    printf("camera.cpp: buffer: %p  data: %p\n", g_DefoldCamera.m_VideoBuffer, data);
	}

    // Increase ref count
    dmScript::PushBuffer(L, g_DefoldCamera.m_VideoBuffer);
    g_DefoldCamera.m_VideoBufferLuaRef = dmScript::Ref(L, LUA_REGISTRYINDEX);

    printf("start capture!\n");

	return 1;
}

static int StopCapture(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

	int status = CameraPlatform_StopCapture();
	if( !status )
	{
        luaL_error(L, "Failed to stop capture. Was it started?");
	}

    dmScript::Unref(L, LUA_REGISTRYINDEX, g_DefoldCamera.m_VideoBufferLuaRef); // We want it destroyed by the GC

    return 0;
}

static int GetInfo(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    lua_newtable(L);
    lua_pushstring(L, "width");
    lua_pushnumber(L, g_DefoldCamera.m_Params.m_Width);
    lua_rawset(L, -3);
    lua_pushstring(L, "height");
    lua_pushnumber(L, g_DefoldCamera.m_Params.m_Height);
    lua_rawset(L, -3);
    lua_pushstring(L, "bytes_per_pixel");
    lua_pushnumber(L, 3);
    lua_rawset(L, -3);

    return 1;
}

static int GetFrame(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    lua_rawgeti(L,LUA_REGISTRYINDEX, g_DefoldCamera.m_VideoBufferLuaRef);
    printf("get frame!\n");
    return 1;
}


static const luaL_reg Module_methods[] =
{
    {"start_capture", StartCapture},
    {"stop_capture", StopCapture},
    {"get_frame", GetFrame},
    {"get_info", GetInfo},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeCamera(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeCamera(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeCamera(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeCamera(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

#else // unsupported platforms


static dmExtension::Result AppInitializeCamera(dmExtension::AppParams* params)
{
    dmLogInfo("Registered %s (null) Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeCamera(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeCamera(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeCamera(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

#endif // platforms


DM_DECLARE_EXTENSION(Camera, LIB_NAME, AppInitializeCamera, AppFinalizeCamera, InitializeCamera, 0, 0, FinalizeCamera)
