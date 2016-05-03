//
// Created by dttv on 16-5-3.
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#include <android/log.h>
#define  LOG_TAG    "gl2_yuv"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#include "gl_util.h"
#include "gl_yuv.h"

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static const char gVertexShader[] =
        "attribute vec4 vPosition;    \n"
        "attribute vec2 a_texCoord;   \n"
        "varying vec2 tc;     \n"
        "void main()                  \n"
        "{                            \n"
        "   gl_Position = vPosition;  \n"
        "   tc = a_texCoord;  \n"
        "}                            \n";

static const char gFragmentShader[] =
        "varying lowp vec2 tc;\n"
        "uniform sampler2D SamplerY;\n"
        "uniform sampler2D SamplerU;\n"
        "uniform sampler2D SamplerV;\n"
        "void main(void)\n"
        "{\n"
            "mediump vec3 yuv;\n"
            "lowp vec3 rgb;\n"
            "yuv.x = texture2D(SamplerY, tc).r;\n"
            "yuv.y = texture2D(SamplerU, tc).r - 0.5;\n"
            "yuv.z = texture2D(SamplerV, tc).r - 0.5;\n"
            "rgb = mat3( 1,   1,   1,\n"
                        "0, -0.39465, 2.03211,\n"
                        "1.13983, -0.58060, 0) * yuv;\n"
            "gl_FragColor = vec4(rgb, 1);\n"
        "}\n";

static GLuint gProgram;
static GLuint gvPositionHandle;

static GLuint g_texYId;
static GLuint g_texUId;
static GLuint g_texVId;

bool yuv_setupGraphics(int w, int h) {
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    LOGI("setupGraphics(%d, %d)", w, h);
    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram) {
        LOGE("Could not create program.");
        return false;
    }
    gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
    checkGlError("glGetAttribLocation");
    LOGI("glGetAttribLocation(\"vPosition\") = %d\n", gvPositionHandle);

    /* texture */
    glGenTextures(1, &g_texYId);
    checkGlError("glGenTextures");
    glGenTextures(1, &g_texUId);
    glGenTextures(1, &g_texVId);
    checkGlError("glGenTextures");

    glViewport(0, 0, w, h);
    checkGlError("glViewport");

    LOGI("YUV setup graphics ok");
    return true;
}

static GLuint bindTexture(GLuint texture, const uint8_t *buffer, GLuint w , GLuint h)
{
    glBindTexture ( GL_TEXTURE_2D, texture);
    glTexImage2D ( GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, buffer);
    checkGlError("glTexImage2D");
#if 1
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
#endif
    return texture;
}

enum {
    ATTRIB_VERTEX,
    ATTRIB_TEXTURE,
};
static void draw()
{
    static GLfloat squareVertices[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f,  1.0f,
            1.0f,  1.0f,
    };

    static GLfloat coordVertices[] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f,  0.0f,
            1.0f,  0.0f,
    };

    glClearColor(0.5f, 0.5f, 0.5f, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gProgram);
    checkGlError("glUseProgram");

    GLint tex_y = glGetUniformLocation(gProgram, "SamplerY");
    GLint tex_u = glGetUniformLocation(gProgram, "SamplerU");
    GLint tex_v = glGetUniformLocation(gProgram, "SamplerV");
    checkGlError("glGetUniformLocation");

    glBindAttribLocation(gProgram, ATTRIB_VERTEX, "vPosition");
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);

    glBindAttribLocation(gProgram, ATTRIB_TEXTURE, "a_texCoord");
    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, coordVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTURE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texYId);
    glUniform1i(tex_y, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_texUId);
    glUniform1i(tex_u, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_texVId);
    glUniform1i(tex_v, 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    checkGlError("glDrawArrays");
    LOGI("YUV setup graphics ok");
}

#define FRAME_PATH "/data/frame_1024_576.yuv"
void yuv_renderFrame() {

    uint8_t *data;
    int width;
    int height;
#define GL_TEST_YUV 1
#ifdef GL_TEST_YUV
    width = 1024;
    height = 576;
    int fd = open(FRAME_PATH, O_RDONLY);
    if(fd < 0) {
        LOGI("%s open failed", FRAME_PATH);
        return;
    }
    struct stat state;
    if(stat(FRAME_PATH, &state) < 0){
        LOGI("%s stat failed", FRAME_PATH);
        return;;
    }
    int64_t filesize = state.st_size;
    data = (uint8_t *)malloc(filesize);
    read(fd, data, filesize);

#endif

    bindTexture(g_texYId, data, width, height);
    bindTexture(g_texUId, data + width * height, width/2, height/2);
    bindTexture(g_texVId, data + width * height * 5 / 4, width/2, height/2);

    draw();
}