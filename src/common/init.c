#include "fastfetch.h"
#include "common/caching.h"
#include "common/parsing.h"
#include "detection/qt.h"
#include "detection/gtk.h"
#include "detection/displayserver/displayserver.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>

static bool strbufEqualsAdapter(const void* first, const void* second)
{
    return ffStrbufComp(second, first) == 0;
}

static void initConfigDirs(FFstate* state)
{
    ffListInit(&state->configDirs, sizeof(FFstrbuf));

    const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
    if(ffStrSet(xdgConfigHome))
    {
        FFstrbuf* buffer = (FFstrbuf*) ffListAdd(&state->configDirs);
        ffStrbufInitA(buffer, 64);
        ffStrbufAppendS(buffer, xdgConfigHome);
        ffStrbufEnsureEndsWithC(buffer, '/');
    }

    #define FF_ENSURE_ONLY_ONCE_IN_LIST(element) \
        if(ffListFirstIndexComp(&state->configDirs, element, strbufEqualsAdapter) < state->configDirs.length - 1) \
        { \
            ffStrbufDestroy(ffListGet(&state->configDirs, state->configDirs.length - 1)); \
            --state->configDirs.length; \
        }

    FFstrbuf* userConfigHome = ffListAdd(&state->configDirs);
    ffStrbufInitA(userConfigHome, 64);
    ffStrbufAppendS(userConfigHome, state->passwd->pw_dir);
    ffStrbufAppendS(userConfigHome, "/.config/");
    FF_ENSURE_ONLY_ONCE_IN_LIST(userConfigHome)

    FFstrbuf* userHome = ffListAdd(&state->configDirs);
    ffStrbufInitA(userHome, 64);
    ffStrbufAppendS(userHome, state->passwd->pw_dir);
    ffStrbufEnsureEndsWithC(userHome, '/');
    FF_ENSURE_ONLY_ONCE_IN_LIST(userHome)

    FFstrbuf xdgConfigDirs;
    ffStrbufInitA(&xdgConfigDirs, 64);
    ffStrbufAppendS(&xdgConfigDirs, getenv("XDG_CONFIG_DIRS"));

    uint32_t startIndex = 0;
    while (startIndex < xdgConfigDirs.length)
    {
        uint32_t colonIndex = ffStrbufNextIndexC(&xdgConfigDirs, startIndex, ':');
        xdgConfigDirs.chars[colonIndex] = '\0';

        if(!ffStrSet(xdgConfigDirs.chars + startIndex))
        {
            startIndex = colonIndex + 1;
            continue;
        }

        FFstrbuf* buffer = (FFstrbuf*) ffListAdd(&state->configDirs);
        ffStrbufInitA(buffer, 64);
        ffStrbufAppendS(buffer, xdgConfigDirs.chars + startIndex);
        ffStrbufEnsureEndsWithC(buffer, '/');
        FF_ENSURE_ONLY_ONCE_IN_LIST(buffer);

        startIndex = colonIndex + 1;
    }
    ffStrbufDestroy(&xdgConfigDirs);

    FFstrbuf* systemConfigHome = ffListAdd(&state->configDirs);
    ffStrbufInitA(systemConfigHome, 64);
    ffStrbufAppendS(systemConfigHome, FASTFETCH_TARGET_DIR_ROOT"/etc/xdg/");
    FF_ENSURE_ONLY_ONCE_IN_LIST(systemConfigHome)

    FFstrbuf* systemConfig = ffListAdd(&state->configDirs);
    ffStrbufInitA(systemConfig, 64);
    ffStrbufAppendS(systemConfig, FASTFETCH_TARGET_DIR_ROOT"/etc/");
    FF_ENSURE_ONLY_ONCE_IN_LIST(systemConfig)

    #undef FF_ENSURE_ONLY_ONCE_IN_LIST
}

static void initCacheDir(FFstate* state)
{
    ffStrbufInitA(&state->cacheDir, 64);

    ffStrbufAppendS(&state->cacheDir, getenv("XDG_CACHE_HOME"));

    if(state->cacheDir.length == 0)
    {
        ffStrbufAppendS(&state->cacheDir, state->passwd->pw_dir);
        ffStrbufAppendS(&state->cacheDir, "/.cache/");
    }
    else
        ffStrbufEnsureEndsWithC(&state->cacheDir, '/');

    mkdir(state->cacheDir.chars, S_IRWXU | S_IXGRP | S_IRGRP | S_IXOTH | S_IROTH); //I hope everybody has a cache folder, but who knows

    ffStrbufAppendS(&state->cacheDir, "fastfetch/");
    mkdir(state->cacheDir.chars, S_IRWXU | S_IRGRP | S_IROTH);
}

static void initState(FFstate* state)
{
    state->logoWidth = 0;
    state->logoHeight = 0;
    state->keysHeight = 0;
    state->passwd = getpwuid(getuid());
    uname(&state->utsname);

    #if FF_HAVE_SYSINFO_H
        sysinfo(&state->sysinfo);
    #endif

    initConfigDirs(state);
    initCacheDir(state);
}

#define FF_INIT_STRBUF_CONFIG(field) ffStrbufInit(*(FFstrbuf**)ffListAdd(&instance->config.bufsToFree) = &field)

static void initModuleArg(FFinstance* instance, FFModuleArgs* args)
{
    FF_INIT_STRBUF_CONFIG(args->key);
    FF_INIT_STRBUF_CONFIG(args->outputFormat);
    FF_INIT_STRBUF_CONFIG(args->errorFormat);
}

static void defaultConfig(FFinstance* instance)
{
    ffListInitA(&instance->config.bufsToFree, sizeof(FFstrbuf*), 150);

    FF_INIT_STRBUF_CONFIG(instance->config.logo.source);
    instance->config.logo.type = FF_LOGO_TYPE_AUTO;
    for(uint8_t i = 0; i < (uint8_t) FASTFETCH_LOGO_MAX_COLORS; ++i)
        FF_INIT_STRBUF_CONFIG(instance->config.logo.colors[i]);
    instance->config.logo.width = 0;
    instance->config.logo.height = 0; //preserve aspect ratio
    instance->config.logo.paddingLeft = 0;
    instance->config.logo.paddingRight = 4;
    instance->config.logo.printRemaining = true;

    FF_INIT_STRBUF_CONFIG(instance->config.colorKeys);
    FF_INIT_STRBUF_CONFIG(instance->config.colorTitle);

    FF_INIT_STRBUF_CONFIG(instance->config.separator);
    ffStrbufAppendS(&instance->config.separator, ": ");

    instance->config.showErrors = false;
    instance->config.recache = false;
    instance->config.cacheSave = true;
    instance->config.allowSlowOperations = false;
    instance->config.disableLinewrap = true;
    instance->config.hideCursor = true;
    instance->config.escapeBedrock = true;
    instance->config.binaryPrefixType = FF_BINARY_PREFIX_TYPE_IEC;
    instance->config.glType = FF_GL_TYPE_AUTO;
    instance->config.pipe = false;
    instance->config.multithreading = true;

    initModuleArg(instance, &instance->config.os);
    initModuleArg(instance, &instance->config.host);
    initModuleArg(instance, &instance->config.kernel);
    initModuleArg(instance, &instance->config.uptime);
    initModuleArg(instance, &instance->config.processes);
    initModuleArg(instance, &instance->config.packages);
    initModuleArg(instance, &instance->config.shell);
    initModuleArg(instance, &instance->config.resolution);
    initModuleArg(instance, &instance->config.de);
    initModuleArg(instance, &instance->config.wm);
    initModuleArg(instance, &instance->config.wmTheme);
    initModuleArg(instance, &instance->config.theme);
    initModuleArg(instance, &instance->config.icons);
    initModuleArg(instance, &instance->config.font);
    initModuleArg(instance, &instance->config.cursor);
    initModuleArg(instance, &instance->config.terminal);
    initModuleArg(instance, &instance->config.terminalFont);
    initModuleArg(instance, &instance->config.cpu);
    initModuleArg(instance, &instance->config.cpuUsage);
    initModuleArg(instance, &instance->config.gpu);
    initModuleArg(instance, &instance->config.memory);
    initModuleArg(instance, &instance->config.swap);
    initModuleArg(instance, &instance->config.disk);
    initModuleArg(instance, &instance->config.battery);
    initModuleArg(instance, &instance->config.powerAdapter);
    initModuleArg(instance, &instance->config.locale);
    initModuleArg(instance, &instance->config.localIP);
    initModuleArg(instance, &instance->config.publicIP);
    initModuleArg(instance, &instance->config.weather);
    initModuleArg(instance, &instance->config.player);
    initModuleArg(instance, &instance->config.song);
    initModuleArg(instance, &instance->config.dateTime);
    initModuleArg(instance, &instance->config.date);
    initModuleArg(instance, &instance->config.time);
    initModuleArg(instance, &instance->config.vulkan);
    initModuleArg(instance, &instance->config.openGL);
    initModuleArg(instance, &instance->config.openCL);
    initModuleArg(instance, &instance->config.users);

    FF_INIT_STRBUF_CONFIG(instance->config.libPCI);
    FF_INIT_STRBUF_CONFIG(instance->config.libVulkan);
    FF_INIT_STRBUF_CONFIG(instance->config.libWayland);
    FF_INIT_STRBUF_CONFIG(instance->config.libXcbRandr);
    FF_INIT_STRBUF_CONFIG(instance->config.libXcb);
    FF_INIT_STRBUF_CONFIG(instance->config.libXrandr);
    FF_INIT_STRBUF_CONFIG(instance->config.libX11);
    FF_INIT_STRBUF_CONFIG(instance->config.libGIO);
    FF_INIT_STRBUF_CONFIG(instance->config.libDConf);
    FF_INIT_STRBUF_CONFIG(instance->config.libDBus);
    FF_INIT_STRBUF_CONFIG(instance->config.libXFConf);
    FF_INIT_STRBUF_CONFIG(instance->config.libSQLite3);
    FF_INIT_STRBUF_CONFIG(instance->config.librpm);
    FF_INIT_STRBUF_CONFIG(instance->config.libImageMagick);
    FF_INIT_STRBUF_CONFIG(instance->config.libZ);
    FF_INIT_STRBUF_CONFIG(instance->config.libChafa);
    FF_INIT_STRBUF_CONFIG(instance->config.libEGL);
    FF_INIT_STRBUF_CONFIG(instance->config.libGLX);
    FF_INIT_STRBUF_CONFIG(instance->config.libOSMesa);
    FF_INIT_STRBUF_CONFIG(instance->config.libOpenCL);
    FF_INIT_STRBUF_CONFIG(instance->config.libplist);
    FF_INIT_STRBUF_CONFIG(instance->config.libcJSON);
    FF_INIT_STRBUF_CONFIG(instance->config.libfreetype);

    instance->config.cpuTemp = false;
    instance->config.gpuTemp = false;
    instance->config.batteryTemp = false;

    instance->config.titleFQDN = false;

    FF_INIT_STRBUF_CONFIG(instance->config.diskFolders);
    instance->config.diskRemovable = false;

    FF_INIT_STRBUF_CONFIG(instance->config.batteryDir);

    FF_INIT_STRBUF_CONFIG(instance->config.separatorString);

    instance->config.localIpShowIpV4 = true;
    instance->config.localIpShowIpV6 = false;
    instance->config.localIpShowLoop = false;
    FF_INIT_STRBUF_CONFIG(instance->config.localIpNamePrefix);

    instance->config.publicIpTimeout = 0;
    FF_INIT_STRBUF_CONFIG(instance->config.publicIpUrl);

    instance->config.weatherTimeout = 0;
    FF_INIT_STRBUF_CONFIG(instance->config.weatherOutputFormat);
    ffStrbufAppendS(&instance->config.weatherOutputFormat, "%t+-+%C+(%l)");

    FF_INIT_STRBUF_CONFIG(instance->config.osFile);

    FF_INIT_STRBUF_CONFIG(instance->config.playerName);
}

#undef FF_INIT_STRBUF_CONFIG

void ffInitInstance(FFinstance* instance)
{
    initState(&instance->state);
    defaultConfig(instance);
}

#if !defined(__ANDROID__)

static void* connectDisplayServerThreadMain(void* instance)
{
    ffConnectDisplayServer((FFinstance*)instance);
    return NULL;
}

#if !defined(__APPLE__)

static void* detectPlasmaThreadMain(void* instance)
{
    ffDetectQt((FFinstance*)instance);
    return NULL;
}

static void* detectGTK2ThreadMain(void* instance)
{
    ffDetectGTK2((FFinstance*)instance);
    return NULL;
}

static void* detectGTK3ThreadMain(void* instance)
{
    ffDetectGTK3((FFinstance*)instance);
    return NULL;
}

static void* detectGTK4ThreadMain(void* instance)
{
    ffDetectGTK4((FFinstance*)instance);
    return NULL;
}

static void* startThreadsThreadMain(void* instance)
{
    pthread_t dsThread;
    pthread_create(&dsThread, NULL, connectDisplayServerThreadMain, instance);
    pthread_detach(dsThread);

    pthread_t gtk2Thread;
    pthread_create(&gtk2Thread, NULL, detectGTK2ThreadMain, instance);
    pthread_detach(gtk2Thread);

    pthread_t gtk3Thread;
    pthread_create(&gtk3Thread, NULL, detectGTK3ThreadMain, instance);
    pthread_detach(gtk3Thread);

    pthread_t gtk4Thread;
    pthread_create(&gtk4Thread, NULL, detectGTK4ThreadMain, instance);
    pthread_detach(gtk4Thread);

    pthread_t plasmaThread;
    pthread_create(&plasmaThread, NULL, detectPlasmaThreadMain, instance);
    pthread_detach(plasmaThread);

    return NULL;
}

void startDetectionThreads(FFinstance* instance)
{
    pthread_t startThreadsThread;
    pthread_create(&startThreadsThread, NULL, startThreadsThreadMain, instance);
    pthread_detach(startThreadsThread);
}

#else // !__APPLE__
void startDetectionThreads(FFinstance* instance)
{
    pthread_t startThreadsThread;
    pthread_create(&startThreadsThread, NULL, connectDisplayServerThreadMain, instance);
    pthread_detach(startThreadsThread);
}
#endif // __APPLE__

#else // !__ANDROID__
void startDetectionThreads(FFinstance* instance)
{
    FF_UNUSED(instance);
}
#endif // __ANDROID__

static volatile bool ffDisableLinewrap = true;
static volatile bool ffHideCursor = true;

static void resetConsole()
{
    if(ffDisableLinewrap)
        fputs("\033[?7h", stdout);

    if(ffHideCursor)
        fputs("\033[?25h", stdout);
}

static void exitSignalHandler(int signal)
{
    FF_UNUSED(signal);
    resetConsole();
    exit(0);
}

void ffStart(FFinstance* instance)
{
    if(instance->config.multithreading)
        startDetectionThreads(instance);

    ffDisableLinewrap = instance->config.disableLinewrap && !instance->config.pipe;
    ffHideCursor = instance->config.hideCursor && !instance->config.pipe;

    struct sigaction action = {};
    action.sa_handler = exitSignalHandler;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);

    //We do the cache validation here, so we can skip it if --recache is given
    if(!instance->config.recache)
        ffCacheValidate(instance);

    //reset everything to default before we start printing
    if(!instance->config.pipe)
        fputs(FASTFETCH_TEXT_MODIFIER_RESET, stdout);

    if(ffHideCursor)
        fputs("\033[?25l", stdout);

    if(ffDisableLinewrap)
        fputs("\033[?7l", stdout);

    ffLogoPrint(instance);
}

void ffFinish(FFinstance* instance)
{
    if(instance->config.logo.printRemaining)
        ffLogoPrintRemaining(instance);

    resetConsole();
}

void ffDestroyInstance(FFinstance* instance)
{
    for(uint32_t i = 0; i < instance->state.configDirs.length; ++i)
        ffStrbufDestroy((FFstrbuf*)ffListGet(&instance->state.configDirs, i));
    ffListDestroy(&instance->state.configDirs);

    ffStrbufDestroy(&instance->state.cacheDir);

    for(uint32_t i = 0; i < instance->config.bufsToFree.length; ++i)
        ffStrbufDestroy(*(FFstrbuf**)ffListGet(&instance->config.bufsToFree, i));
    ffListDestroy(&instance->config.bufsToFree);
}

//Must be in a file compiled with the libfastfetch target, because the FF_HAVE* macros are not defined for the executable targets
void ffListFeatures()
{
    fputs(
        #ifdef FF_HAVE_LIBPCI
            "libpci\n"
        #endif
        #ifdef FF_HAVE_VULKAN
            "vulkan\n"
        #endif
        #ifdef FF_HAVE_WAYLAND
            "wayland\n"
        #endif
        #ifdef FF_HAVE_XCB_RANDR
            "xcb-randr\n"
        #endif
        #ifdef FF_HAVE_XCB
            "xcb\n"
        #endif
        #ifdef FF_HAVE_XRANDR
            "xrandr\n"
        #endif
        #ifdef FF_HAVE_X11
            "x11\n"
        #endif
        #ifdef FF_HAVE_GIO
            "gio\n"
        #endif
        #ifdef FF_HAVE_DCONF
            "dconf\n"
        #endif
        #ifdef FF_HAVE_DBUS
            "dbus\n"
        #endif
        #ifdef FF_HAVE_IMAGEMAGICK7
            "imagemagick7\n"
        #endif
        #ifdef FF_HAVE_IMAGEMAGICK6
            "imagemagick6\n"
        #endif
        #ifdef FF_HAVE_CHAFA
            "chafa\n"
        #endif
        #ifdef FF_HAVE_ZLIB
            "zlib\n"
        #endif
        #ifdef FF_HAVE_XFCONF
            "xfconf\n"
        #endif
        #ifdef FF_HAVE_SQLITE3
            "sqlite3\n"
        #endif
        #ifdef FF_HAVE_RPM
            "rpm\n"
        #endif
        #ifdef FF_HAVE_EGL
            "egl\n"
        #endif
        #ifdef FF_HAVE_GLX
            "glx\n"
        #endif
        #ifdef FF_HAVE_OSMESA
            "osmesa\n"
        #endif
        #ifdef FF_HAVE_OPENCL
            "opencl\n"
        #endif
        #ifdef FF_HAVE_LIBPLIST
            "libplist\n"
        #endif
        #ifdef FF_HAVE_LIBCJSON
            "libcjson\n"
        #endif
        #ifdef FF_HAVE_FREETYPE
            "freetype"
        #endif
        ""
    , stdout);
}
