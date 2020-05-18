// on Pi: gcc opengltest.c -lGLESv2 -lEGL -lX11 -o opengltest
// on Ubuntu: gcc opengltest.c -lGL -lGLEW -lX11 -o opengltest

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ARM_EABI__) || defined(__ARMEL__)
#define kUsePI 1
#else // defined(__ARM_EABI__) || defined(__ARMEL__)
#define kUsePI 0
#endif // defined(__ARM_EABI__) || defined(__ARMEL__)

#if kUsePI
#include <GLES2/gl2.h>
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>
#define kUseEGL 1
#else // kUsePI
#include <GL/glew.h>
#define kUseEGL 0
#endif // kUsePI

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#if kUseEGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#else // kUseEGL
#include <GL/glx.h>
#endif // kUseEGL

Display *mDisplay = 0;
int mScreenWidth = 1920;
int mScreenHeight = 1080;
XVisualInfo *mVisual = 0;
#if kUseEGL
EGLDisplay mEGLDisplay;
EGLSurface mSurface;
EGLContext mContext;
#else // kUseEGL
GLXContext mContext;
#endif // kUseEGL
Colormap mColormap;
Window mWindow;

GLuint mProgram;
GLuint mVertexShader;
GLuint mFragmentShader;

void
InitX11 ()
{
    mDisplay = XOpenDisplay (0);
    if (mDisplay == 0) {
        printf ("XOpenDisplay failed\n");
        exit (1);
    }
    Window theDefault = DefaultRootWindow (mDisplay);
    if (theDefault >= 0) {
            XWindowAttributes theAttr;
            XGetWindowAttributes (mDisplay, theDefault, &theAttr);
            mScreenWidth = theAttr.width;
            mScreenHeight = theAttr.height;
            printf ("screen %dx%d\n", mScreenWidth, mScreenHeight);
    }
#if kUseEGL
    if ((mEGLDisplay = eglGetDisplay (mDisplay)) == EGL_NO_DISPLAY) {
        printf ("eglGetDisplay failed\n");
        exit (3);
    }
    EGLBoolean result;
    EGLint theConfigCount;
    static const EGLint sAttributeList[] =
    {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_DEPTH_SIZE, 16,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLint theEglMajor = 0;
    EGLint theEglMinor = 0;
    if ((result = eglInitialize(mEGLDisplay, &theEglMajor, &theEglMinor)) == EGL_FALSE) {
        printf ("eglInitialize failed\n");
        exit (4);
    }
    // get an appropriate EGL frame buffer configuration
    EGLConfig theConfig;

    if ((result = eglChooseConfig (mEGLDisplay, sAttributeList, &theConfig, 1, &theConfigCount)) == EGL_FALSE || theConfigCount == 0) {
        printf ("eglChooseConfig failed\n");
        exit (5);
    }
    
    eglBindAPI (EGL_OPENGL_ES_API);
    
    static const EGLint sContextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    if ((mContext = eglCreateContext(mEGLDisplay, theConfig, EGL_NO_CONTEXT, sContextAttributes)) == EGL_NO_CONTEXT) {
        printf ("eglCreateContext failed %d\n", eglGetError ());
        exit (6);
    }
    EGLint theVisualId;
    if (!eglGetConfigAttrib (mEGLDisplay, theConfig, EGL_NATIVE_VISUAL_ID, &theVisualId)) {
        printf("eglGetConfigAttrib() failed\n");
        exit (7);
    }
    // The X window visual must match the EGL config
    XVisualInfo theVisualTemplate;
    theVisualTemplate.visualid = theVisualId;
    int theNumVisuals = 0;
    mVisual = XGetVisualInfo (mDisplay, VisualIDMask, &theVisualTemplate, &theNumVisuals);
    if (!mVisual) {
	printf("XGetVisualInfo failed\n");
        exit (8);
    }
    XMatchVisualInfo (mDisplay, XDefaultScreen(mDisplay), 24, TrueColor, mVisual);
#else // kUseEGL
    static int sAttribList2[] = {
        GLX_TRANSPARENT_TYPE, GLX_NONE,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 0,
        GLX_DEPTH_SIZE, 16,
        None};
    int theNumFBConfigs = 0;
    GLXFBConfig *theFBConfigs = glXChooseFBConfig (mDisplay, DefaultScreen (mDisplay), sAttribList2, &theNumFBConfigs);
    for (int i = 0 ; i < theNumFBConfigs ; i++) {
        mVisual = (XVisualInfo *)glXGetVisualFromFBConfig (mDisplay, theFBConfigs[i]);
        if (mVisual != 0) {
            break;
        }
    }
    if (theFBConfigs != 0) {
        XFree (theFBConfigs);
        theFBConfigs = 0;
    }
    mContext = glXCreateContext (mDisplay, mVisual, None, True);
    if (mContext == 0) {
        printf ("glXCreateContext failed\n");
        exit (2);
    }
#endif // kUseEGL
    mColormap = XCreateColormap (mDisplay, RootWindow(mDisplay, mVisual->screen), mVisual->visual, AllocNone);
    XSetWindowAttributes theSWA;
    memset (&theSWA, 0, sizeof (theSWA));
    theSWA.colormap = mColormap;
    theSWA.override_redirect = True; // mFullScreen ? True : False; // False; // True;
    mWindow = XCreateWindow (
            mDisplay,
            XDefaultRootWindow(mDisplay),
            0, 0, mScreenWidth, mScreenHeight, 0, mVisual->depth, InputOutput, mVisual->visual,
            CWBorderPixel | CWBackPixel | CWColormap | CWOverrideRedirect | CWEventMask, &theSWA
    );
    static XClassHint theClassHint;
    theClassHint.res_name = (char *)"opengltest";
    theClassHint.res_class = (char *)"opengltest";
    XSetClassHint (mDisplay, mWindow, &theClassHint);

    static char* sDummy[] = {0};
    XSetStandardProperties (mDisplay, mWindow, "opengltest", "opengltest", None, sDummy, 0, NULL);

    Atom theAtomState = XInternAtom (mDisplay, "_NET_WM_STATE", True);
    Atom theAtomStateFullscreen = XInternAtom (mDisplay, "_NET_WM_STATE_FULLSCREEN", True);
    XChangeProperty (mDisplay, mWindow, theAtomState, XA_ATOM, 32, PropModeReplace, (unsigned char *)&theAtomStateFullscreen, 1);

    XMapWindow (mDisplay, mWindow);
#if kUseEGL
    static const EGLint theWindowAttributeList[] = {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE
    };
    if ((mSurface = eglCreateWindowSurface(mEGLDisplay, theConfig, (EGLNativeWindowType)mWindow, theWindowAttributeList)) == EGL_NO_SURFACE) {
        printf ("eglCreateWindowSurface failed\n");
        exit (11);
    }
    EGLint theWidth, theHeight;	
    eglQuerySurface (mEGLDisplay, mSurface, EGL_WIDTH, &theWidth);
    eglQuerySurface (mEGLDisplay, mSurface, EGL_HEIGHT, &theHeight);
    printf ("eglQuerySurface: %dx%d\n", theWidth, theHeight);
    if (eglMakeCurrent(mEGLDisplay, mSurface, mSurface, mContext) == EGL_FALSE) {
        printf ("eglMakeCurrent failed\n");
        exit (9);
    }
#else // kUseEGL
    glXMakeCurrent (mDisplay, mWindow, mContext);
#endif // kUseEGL
}

int main(void)
{
    InitX11 ();

#if kUseEGL
#else // kUseEGL
    glewInit ();
#endif // kUseEGL

    float mNearPlane = mScreenHeight*18.0f/14.7f;
    float mFarPlane = 100*mNearPlane;
    float theProjectionMatrix[16] = {2*mNearPlane / mScreenWidth, 0, 0, 0, 0, 2*mNearPlane / mScreenHeight, 0, 0, 0, 0, -(mFarPlane + mNearPlane) / (mFarPlane - mNearPlane), -1, 0, 0, -(2 * mFarPlane * mNearPlane) / (mFarPlane - mNearPlane), 0};
    float theModelViewMatrix[16] = {1.0f, 0, 0, 0, 0, -1.0f, 0, 0, 0, 0, -1.0, 0, -mScreenWidth/2.0f, mScreenHeight/2.0f, -mNearPlane - 0.1f, 1.0f};
    float theModelViewProjectionMatrix[16] = {0};
    for (int i = 0 ; i < 4 ; i++) {
        for (int j = 0 ; j < 4 ; j++) {
#define M(X,Y) (4*Y+X)
            // matrix is stored as
            // ( 0  4  8 12)
            // ( 1  5  9 13)
            // ( 2  6 10 14)
            // ( 3  7 11 15)
            theModelViewProjectionMatrix[M(i,j)] = 0.0f;
            for (int k = 0 ; k < 4 ; k++) {
                    theModelViewProjectionMatrix[M(i,j)] += theProjectionMatrix[M(i,k)]*theModelViewMatrix[M(k,j)];
            }
        }
    }

    glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);

    GLuint theVertexBuffer;
    glGenBuffers (1, &theVertexBuffer);

    GLfloat theVertexCoordinates[6*3];

    static const GLfloat theCoordinates[6*2] = {
        0, 0,
        0, 1,
        1, 0,
        1, 0,
        0, 1,
        1, 1,
    };
    unsigned int theColor = 0xff0000ff;
    for (int i = 0; i < 6 ; i++) {
        theVertexCoordinates[(6*0+i)*3+0] = 100 + (mScreenWidth-2*100)*theCoordinates[i*2+0];
        theVertexCoordinates[(6*0+i)*3+1] = 100 + (mScreenHeight-2*100)*theCoordinates[i*2+1];
        theVertexCoordinates[(6*0+i)*3+2] = 0;
    }

    glBindBuffer (GL_ARRAY_BUFFER, theVertexBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof(GLfloat)*6*3, theVertexCoordinates, GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    mVertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char *sVertexShaderSource = "#version 100\nuniform mat4 uMVPMatrix;\nattribute vec4 aPosition;\nvoid main() {\ngl_Position = uMVPMatrix * aPosition;\n}\n";
    glShaderSource (mVertexShader, 1, &sVertexShaderSource, NULL);
    glCompileShader (mVertexShader);
    mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char *sFragmentShaderSource = "#version 100\nprecision lowp float;\nuniform vec4 uColor;\nvoid main() {\ngl_FragColor = uColor;\n}\n";
    glShaderSource (mFragmentShader, 1, &sFragmentShaderSource, NULL);
    glCompileShader (mFragmentShader);
    mProgram = glCreateProgram();
    glAttachShader (mProgram, mVertexShader);
    glAttachShader (mProgram, mFragmentShader);
    glLinkProgram (mProgram);

    GLint mMVPMatrixHandle = glGetUniformLocation (mProgram, "uMVPMatrix");
    GLint mColorHandle = glGetUniformLocation (mProgram, "uColor");
    GLint mPositionHandle = glGetAttribLocation (mProgram, "aPosition");

    for (;;) {
        static int sClearTint = 0;
        sClearTint++;
        glClearColor (0.0f, (sClearTint&0xff)/255.0f, 1.0f-(sClearTint&0xff)/255.0f, 1.0f);
        glDisable (GL_CULL_FACE);
        glDisable (GL_DEPTH_TEST);
        glEnable (GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram (mProgram);
        glUniformMatrix4fv (mMVPMatrixHandle, 1, GL_FALSE, theModelViewProjectionMatrix);
        glUniform4f (mColorHandle, 1.0f, 0.0f, 0.0f, 1.0f); // red
        glBindBuffer (GL_ARRAY_BUFFER, theVertexBuffer);
        glVertexAttribPointer (mPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray (mPositionHandle);
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        glDrawArrays (GL_TRIANGLES, 0, 6);

#if kUseEGL
        eglSwapBuffers (mEGLDisplay, mSurface);
#else // kUseEGL
        glXSwapBuffers (mDisplay, mWindow);
#endif // kUseEGL
    }
    return 0;
}
