#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define EXTENSION_NAME Facerec
#define LIB_NAME "Facerec"
#define MODULE_NAME "facerec"

// Defold SDK
#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_OSX)

#include <extdlib/image_processing/frontal_face_detector.h>
#include <extdlib/image_processing/render_face_detections.h>
#include <extdlib/image_processing.h>
//#include <extdlib/image_io.h>
//#include <extdlib/image_saver/image_saver.h>
#include <iostream>

// Data set
// http://dlib.net/files/data

// Example face landmark code
// http://dlib.net/face_landmark_detection_ex.cpp.html

// http://stackoverflow.com/a/13059195
// Have to create a buffer stream manually, because c++...
#include <streambuf>
#include <istream>

namespace {
    struct membuf: std::streambuf {
        membuf(char const* base, size_t size) {
            char* p(const_cast<char*>(base));
            this->setg(p, p, p + size);
        }
    };
    struct imemstream: virtual membuf, std::istream {
        imemstream(char const* base, size_t size)
            : membuf(base, size)
            , std::istream(static_cast<std::streambuf*>(this)) {
        }
    };
}

struct Facerec
{
    int m_TrainingDataLuaRef;

    dmBuffer::HBuffer m_TrainingData;

    dlib::frontal_face_detector   m_Detector;
    dlib::shape_predictor         m_Predictor;
};

Facerec g_Facerec;


static int FacerecStart(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 1);

    dmScript::PushBuffer(L, *buffer);
    g_Facerec.m_TrainingDataLuaRef = dmScript::Ref(L, LUA_REGISTRYINDEX);

    g_Facerec.m_Detector = dlib::get_frontal_face_detector();

    uint8_t* data = 0;
    uint32_t datasize = 0;
    dmBuffer::GetBytes(buffer->m_Buffer, (void**)&data, &datasize);

    imemstream stream( (char const*)data, (size_t)datasize );
    dlib::deserialize(g_Facerec.m_Predictor, stream);

    return 0;
}

static int FacerecStop(lua_State* L)
{
    dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facerec.m_TrainingDataLuaRef); // We want it destroyed by the GC
    return 0;
}

static void FaceRecPushPoint(lua_State* L, int index, int x, int y)
{
    lua_pushnumber(L, index);
    lua_newtable(L);
        lua_pushstring(L, "x");
        lua_pushnumber(L, x);
        lua_rawset(L, -3);
        lua_pushstring(L, "y");
        lua_pushnumber(L, y);
        lua_rawset(L, -3);
    lua_rawset(L, -3);
}

static int FacerecAnalyze(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    int width = luaL_checkint(L, 1);
    int height = luaL_checkint(L, 2);
    dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 3);

    uint8_t* data = 0;
    uint32_t datasize = 0;
    dmBuffer::GetBytes(buffer->m_Buffer, (void**)&data, &datasize);

    dlib::array2d<dlib::rgb_pixel> img;

    /*
    // yes...
    img.set_size(height, width);
    for( int y = 0; y < height; ++y)
    {
        for( int x = 0; x < width; ++x)
        {
            dlib::rgb_pixel p;
            p.red   = data[y*width*3 + x*3 + 0];
            p.green = data[y*width*3 + x*3 + 1];
            p.blue  = data[y*width*3 + x*3 + 2];
            dlib::assign_pixel(img[height-y-1][x],p);
        }
    }*/

    // Poor mans' downscale
    int downscale = 1;
    img.set_size(height>>downscale, width>>downscale);
    for( int y = 0; y < height; ++y)
    {
        for( int x = 0; x < width; ++x)
        {
            dlib::rgb_pixel p;
            p.red   = data[y*width*3 + x*3 + 0];
            p.green = data[y*width*3 + x*3 + 1];
            p.blue  = data[y*width*3 + x*3 + 2];
            int ty = (height-y-1)>>downscale;
            int tx = x >> downscale;
            dlib::assign_pixel(img[ty][tx],p);
        }
    }

    // static int count = 0;
    // count++;
    // if ( count == 5 )
    // {
    //     std::ofstream myfile;
    //     const char* path = "/Users/mathiaswesterdahl/work/test.bmp";
    //     myfile.open(path);
    //     dlib::save_bmp(img, myfile);
    //     printf("\n\nSAVED FILE: %s\n\n", path);
    // }

    std::vector<dlib::rectangle> faces = g_Facerec.m_Detector(img);

    lua_newtable(L);

    for(unsigned long f = 0; f < faces.size(); ++f)
    {
        dlib::full_object_detection shape = g_Facerec.m_Predictor(img, faces[f]);

        lua_pushnumber(L, f + 1);
        lua_newtable(L);

        int arrayindex = 0;

        //const char* names[] = {};

        lua_pushstring(L, "chin");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 0; i <= 16; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "eye_left");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 36; i <= 41; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "eye_right");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 42; i <= 47; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);


        lua_pushstring(L, "lips_outer");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 48; i <= 59; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "lips_inner");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 60; i <= 67; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);


        lua_pushstring(L, "eyebrow_left");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 17; i <= 21; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "eyebrow_right");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 22; i <= 26; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);


        lua_pushstring(L, "nose_bottom");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 30; i <= 35; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "nose_ridge");
        lua_newtable(L);
        arrayindex = 0;
        for (unsigned long i = 27; i <= 30; ++i, ++arrayindex)
        {
            FaceRecPushPoint(L, arrayindex+1, shape.part(i).x()<<downscale, height - (shape.part(i).y()<<downscale));
        }
        lua_rawset(L, -3);

        // face
        lua_rawset(L, -3);
    }


    return 1;
}

static const luaL_reg Module_methods[] =
{
    {"start", FacerecStart},
    {"stop", FacerecStop},
    {"analyze", FacerecAnalyze},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeExtension(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

#else // unsupported platforms


static dmExtension::Result AppInitializeExtension(dmExtension::AppParams* params)
{
    dmLogInfo("Registered %s (null) Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

#endif // platforms


DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeExtension, AppFinalizeExtension, InitializeExtension, 0, 0, FinalizeExtension)
