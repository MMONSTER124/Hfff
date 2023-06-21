#include "displayserver_linux.h"
#include "common/io/io.h"
#include "common/properties.h"
#include "common/parsing.h"
#include "common/processing.h"
#include "util/stringUtils.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static const char* parseEnv()
{
    const char* env;

    env = getenv("XDG_CURRENT_DESKTOP");
    if(ffStrSet(env))
        return env;

    env = getenv("XDG_SESSION_DESKTOP");
    if(ffStrSet(env))
        return env;

    env = getenv("CURRENT_DESKTOP");
    if(ffStrSet(env))
        return env;

    env = getenv("SESSION_DESKTOP");
    if(ffStrSet(env))
        return env;

    env = getenv("DESKTOP_SESSION");
    if(ffStrSet(env))
        return env;

    if(getenv("KDE_FULL_SESSION") != NULL || getenv("KDE_SESSION_UID") != NULL || getenv("KDE_SESSION_VERSION") != NULL)
        return "KDE";

    if(getenv("GNOME_DESKTOP_SESSION_ID") != NULL)
        return "Gnome";

    if(getenv("MATE_DESKTOP_SESSION_ID") != NULL)
        return "Mate";

    if(getenv("TDE_FULL_SESSION") != NULL)
        return "Trinity";

    if(
        getenv("WAYLAND_DISPLAY") != NULL &&
        ffPathExists("/mnt/wslg/", FF_PATHTYPE_DIRECTORY)
    ) return "WSLg";

    return NULL;
}

static void applyPrettyNameIfWM(FFDisplayServerResult* result, const char* name)
{
    if(!ffStrSet(name))
        return;

    if(strcasestr(name, "kwin") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_KWIN);
    else if(
        strcasecmp(name, "gnome-shell") == 0 ||
        strcasecmp(name, "gnome shell") == 0 ||
        strcasecmp(name, "gnome-session-binary") == 0 ||
        strcasestr(name, "mutter") != NULL
    )
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_MUTTER);
    else if(
        strcasecmp(name, "cinnamon-session") == 0 ||
        strcasestr(name, "muffin") != NULL
    )
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_MUFFIN);
    else if(strcasestr(name, "sway") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_SWAY);
    else if(strcasestr(name, "weston") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_WESTON);
    else if(strcasestr(name, "wayfire") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_WAYFIRE);
    else if(strcasestr(name, "openbox") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_OPENBOX);
    else if(strcasestr(name, "xfwm4") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_XFWM4);
    else if(strcasestr(name, "marco") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_MARCO);
    else if(strcasestr(name, "xmonad") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_XMONAD);
    else if(strcasestr(name, "wslg") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_WSLG);
    else if(strcasestr(name, "dwm") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_DWM);
    else if(strcasestr(name, "bspwm") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_BSPWM);
    else if(strcasestr(name, "tinywm") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_TINYWM);
    else if(strcasestr(name, "qtile") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_QTILE);
    else if(strcasestr(name, "herbstluftwm") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_HERBSTLUFTWM);
    else if(strcasestr(name, "icewm") != NULL)
        ffStrbufSetS(&result->wmPrettyName, FF_WM_PRETTY_ICEWM);
}

static void applyNameIfWM(FFDisplayServerResult* result, const char* processName)
{
    applyPrettyNameIfWM(result, processName);
    if(result->wmPrettyName.length > 0)
        ffStrbufSetS(&result->wmProcessName, processName);
}

static void applyBetterWM(FFDisplayServerResult* result, const char* processName)
{
    if(!ffStrSet(processName))
        return;

    ffStrbufSetS(&result->wmProcessName, processName);

    //If it is a known wm, this will set the pretty name
    applyPrettyNameIfWM(result, processName);

    //If it isn't a known wm, set the pretty name to the process name
    if(result->wmPrettyName.length == 0)
        ffStrbufAppend(&result->wmPrettyName, &result->wmProcessName);
}

static void getKDE(const FFinstance* instance, FFDisplayServerResult* result)
{
    ffStrbufSetS(&result->deProcessName, "plasmashell");
    ffStrbufSetS(&result->dePrettyName, FF_DE_PRETTY_PLASMA);

    ffParsePropFileValues("/usr/share/xsessions/plasmax11.desktop", 1, (FFpropquery[]) {
        {"X-KDE-PluginInfo-Version =", &result->deVersion}
    });
    if(result->deVersion.length == 0)
        ffParsePropFileData(instance, "xsessions/plasma.desktop", "X-KDE-PluginInfo-Version =", &result->deVersion);
    if(result->deVersion.length == 0)
        ffParsePropFileData(instance, "xsessions/plasma5.desktop", "X-KDE-PluginInfo-Version =", &result->deVersion);
    if(result->deVersion.length == 0)
    {
        ffParsePropFileValues("/usr/share/wayland-sessions/plasma.desktop", 1, (FFpropquery[]) {
            {"X-KDE-PluginInfo-Version =", &result->deVersion}
        });
    }
    if(result->deVersion.length == 0)
        ffParsePropFileData(instance, "wayland-sessions/plasmawayland.desktop", "X-KDE-PluginInfo-Version =", &result->deVersion);
    if(result->deVersion.length == 0)
        ffParsePropFileData(instance, "wayland-sessions/plasmawayland5.desktop", "X-KDE-PluginInfo-Version =", &result->deVersion);

    if(result->deVersion.length == 0 && instance->config.allowSlowOperations)
    {
        if (ffProcessAppendStdOut(&result->deVersion, (char* const[]){
            "plasmashell",
            "--version",
            NULL
        }) == NULL) // plasmashell 5.27.5
            ffStrbufSubstrAfterLastC(&result->deVersion, ' ');
    }


    applyBetterWM(result, getenv("KDEWM"));
}

static void getGnome(const FFinstance* instance, FFDisplayServerResult* result)
{
    ffStrbufSetS(&result->deProcessName, "gnome-shell");
    const char* sessionMode = getenv("GNOME_SHELL_SESSION_MODE");
    if (sessionMode && ffStrEquals(sessionMode, "classic"))
        ffStrbufSetS(&result->dePrettyName, FF_DE_PRETTY_GNOME_CLASSIC);
    else
        ffStrbufSetS(&result->dePrettyName, FF_DE_PRETTY_GNOME);

    ffParsePropFileData(instance, "gnome-shell/org.gnome.Extensions", "version :", &result->deVersion);

    if (result->deVersion.length == 0)
    {
        if (ffProcessAppendStdOut(&result->deVersion, (char* const[]){
            "gnome-shell",
            "--version",
            NULL
        }) == NULL) // GNOME Shell 44.1
            ffStrbufSubstrAfterLastC(&result->deVersion, ' ');
    }
}

static void getCinnamon(const FFinstance* instance, FFDisplayServerResult* result)
{
    ffStrbufSetS(&result->deProcessName, "cinnamon");
    ffStrbufSetS(&result->dePrettyName, FF_DE_PRETTY_CINNAMON);
    ffParsePropFileData(instance, "applications/cinnamon.desktop", "X-GNOME-Bugzilla-Version =", &result->deVersion);
}

static void getMate(const FFinstance* instance, FFDisplayServerResult* result)
{
    ffStrbufSetS(&result->deProcessName, "mate-session");
    ffStrbufSetS(&result->dePrettyName, FF_DE_PRETTY_MATE);

    FF_STRBUF_AUTO_DESTROY major = ffStrbufCreate();
    FF_STRBUF_AUTO_DESTROY minor = ffStrbufCreate();
    FF_STRBUF_AUTO_DESTROY micro = ffStrbufCreate();

    ffParsePropFileDataValues(instance, "mate-about/mate-version.xml", 3, (FFpropquery[]) {
        {"<platform>", &major},
        {"<minor>", &minor},
        {"<micro>", &micro}
    });

    ffParseSemver(&result->deVersion, &major, &minor, &micro);

    if(result->deVersion.length == 0 && instance->config.allowSlowOperations)
    {
        ffProcessAppendStdOut(&result->deVersion, (char* const[]){
            "mate-session",
            "--version",
            NULL
        });

        ffStrbufSubstrAfterFirstC(&result->deVersion, ' ');
        ffStrbufTrim(&result->deVersion, ' ');
    }
}

static void getXFCE4(const FFinstance* instance, FFDisplayServerResult* result)
{
    ffStrbufSetS(&result->deProcessName, "xfce4-session");
    ffStrbufSetS(&result->dePrettyName, FF_DE_PRETTY_XFCE4);
    ffParsePropFileData(instance, "gtk-doc/html/libxfce4ui/index.html", "<div><p class=\"releaseinfo\">Version", &result->deVersion);

    if(result->deVersion.length == 0 && instance->config.allowSlowOperations)
    {
        //This is somewhat slow
        ffProcessAppendStdOut(&result->deVersion, (char* const[]){
            "xfce4-session",
            "--version",
            NULL
        });

        ffStrbufSubstrBeforeFirstC(&result->deVersion, '(');
        ffStrbufSubstrAfterFirstC(&result->deVersion, ' ');
        ffStrbufTrim(&result->deVersion, ' ');
    }
}

static void getLXQt(const FFinstance* instance, FFDisplayServerResult* result)
{
    ffStrbufSetS(&result->deProcessName, "lxqt-session");
    ffStrbufSetS(&result->dePrettyName, FF_DE_PRETTY_LXQT);
    ffParsePropFileData(instance, "gconfig/lxqt.pc", "Version:", &result->deVersion);

    if(result->deVersion.length == 0)
        ffParsePropFileData(instance, "cmake/lxqt/lxqt-config.cmake", "set ( LXQT_VERSION", &result->deVersion);
    if(result->deVersion.length == 0)
        ffParsePropFileData(instance, "cmake/lxqt/lxqt-config-version.cmake", "set ( PACKAGE_VERSION", &result->deVersion);

    if(result->deVersion.length == 0 && instance->config.allowSlowOperations)
    {
        //This is really, really, really slow. Thank you, LXQt developers
        ffProcessAppendStdOut(&result->deVersion, (char* const[]){
            "lxqt-session",
            "-v",
            NULL
        });

        result->deVersion.length = 0; //don't set '\0' byte
        ffParsePropLines(result->deVersion.chars , "liblxqt", &result->deVersion);
    }

    FF_STRBUF_AUTO_DESTROY wmProcessNameBuffer = ffStrbufCreate();

    ffParsePropFileConfig(instance, "lxqt/session.conf", "window_manager =", &wmProcessNameBuffer);
    applyBetterWM(result, wmProcessNameBuffer.chars);
}

static void getBudgie(const FFinstance* instance, FFDisplayServerResult* result)
{
    ffStrbufSetS(&result->deProcessName, "budgie-desktop");
    ffStrbufSetS(&result->dePrettyName, FF_DE_PRETTY_BUDGIE);
    ffParsePropFileData(instance, "budgie/budgie-version.xml", "<str>", &result->deVersion);
}

static void applyPrettyNameIfDE(const FFinstance* instance, FFDisplayServerResult* result, const char* name)
{
    if(!ffStrSet(name))
        return;
    else if(strcasestr(name, "plasma") != NULL || strcasestr(name, "kde") != NULL)
        getKDE(instance, result);
    else if(strcasestr(name, "gnome") != NULL)
        getGnome(instance, result);
    else if(strcasestr(name, "cinnamon") != NULL)
        getCinnamon(instance, result);
    else if(strcasestr(name, "xfce") != NULL)
        getXFCE4(instance, result);
    else if(strcasestr(name, "mate") != NULL)
        getMate(instance, result);
    else if(strcasestr(name, "lxqt") != NULL)
        getLXQt(instance, result);
    else if(strcasestr(name, "budgie") != NULL)
        getBudgie(instance, result);
}

static void getWMProtocolNameFromEnv(FFDisplayServerResult* result)
{
    //This is only called if all connection attempts to a display server failed
    //We don't need to check for wayland here, as the wayland code will always set the protocol name to wayland

    const char* env = getenv("XDG_SESSION_TYPE");
    if(ffStrSet(env))
    {
        if(strcasecmp(env, "x11") == 0)
            ffStrbufSetS(&result->wmProtocolName, FF_WM_PROTOCOL_X11);
        else if(strcasecmp(env, "tty") == 0)
            ffStrbufSetS(&result->wmProtocolName, FF_WM_PROTOCOL_TTY);
        else
            ffStrbufSetS(&result->wmProtocolName, env);

        return;
    }

    env = getenv("DISPLAY");
    if(ffStrSet(env))
    {
        ffStrbufSetS(&result->wmProtocolName, FF_WM_PROTOCOL_X11);
        return;
    }

    env = getenv("TERM");
    if(ffStrSet(env) && strcasecmp(env, "linux") == 0)
    {
        ffStrbufSetS(&result->wmProtocolName, FF_WM_PROTOCOL_TTY);
        return;
    }
}

static void getFromProcDir(const FFinstance* instance, FFDisplayServerResult* result)
{
    DIR* proc = opendir("/proc");
    if(proc == NULL)
        return;

    FF_STRBUF_AUTO_DESTROY procPath = ffStrbufCreateA(64);
    ffStrbufAppendS(&procPath, "/proc/");

    uint32_t procPathLength = procPath.length;

    FF_STRBUF_AUTO_DESTROY userID = ffStrbufCreateF("%i", getuid());
    FF_STRBUF_AUTO_DESTROY loginuid = ffStrbufCreate();
    FF_STRBUF_AUTO_DESTROY processName = ffStrbufCreateA(256); //Some processes have large command lines (looking at you chrome)

    struct dirent* dirent;
    while((dirent = readdir(proc)) != NULL)
    {
        //Match only folders starting with a number (the pid folders)
        if(dirent->d_type != DT_DIR || !isdigit(dirent->d_name[0]))
            continue;

        ffStrbufAppendS(&procPath, dirent->d_name);
        uint32_t procFolderPathLength = procPath.length;

        //Don't check for processes not owend by the current user.
        ffStrbufAppendS(&procPath, "/loginuid");
        ffReadFileBuffer(procPath.chars, &loginuid);
        if(ffStrbufComp(&userID, &loginuid) != 0)
        {
            ffStrbufSubstrBefore(&procPath, procPathLength);
            continue;
        }

        ffStrbufSubstrBefore(&procPath, procFolderPathLength);

        //We check the cmdline for the process name, because it is not trimmed.
        ffStrbufAppendS(&procPath, "/cmdline");
        ffReadFileBuffer(procPath.chars, &processName);
        ffStrbufSubstrBeforeFirstC(&processName, '\0'); //Trim the arguments
        ffStrbufSubstrAfterLastC(&processName, '/');

        ffStrbufSubstrBefore(&procPath, procPathLength);

        if(result->dePrettyName.length == 0)
            applyPrettyNameIfDE(instance, result, processName.chars);

        if(result->wmPrettyName.length == 0)
            applyNameIfWM(result, processName.chars);

        if(result->dePrettyName.length > 0 && result->wmPrettyName.length > 0)
            break;
    }

    closedir(proc);
}

void ffdsDetectWMDE(const FFinstance* instance, FFDisplayServerResult* result)
{
    //If all connections failed, use the environment variables to detect protocol name
    if(result->wmProtocolName.length == 0)
        getWMProtocolNameFromEnv(result);

    //We don't want to detect anything in TTY
    //This can't happen if a connection succeeded, so we don't need to clear wmProcessName
    if(ffStrbufIgnCaseCompS(&result->wmProtocolName, FF_WM_PROTOCOL_TTY) == 0)
        return;

    const char* env = parseEnv();

    if(result->wmProcessName.length > 0)
    {
        //If we found the processName via display server, use it.
        //This will set the pretty name if it is a known WM, otherwise the prettyName to the processName
        applyPrettyNameIfWM(result, result->wmProcessName.chars);
        if(result->wmPrettyName.length == 0)
            ffStrbufSet(&result->wmPrettyName, &result->wmProcessName);
    }
    else
    {
        //if env is a known WM, use it
        applyNameIfWM(result, env);
    }

    //Connecting to a display server only gives WM results, not DE results.
    //If we find it in the environment, use that.
    applyPrettyNameIfDE(instance, result, env);

    //If WM was found by connection to the sever, and DE in the environment, we can return
    //This way we never call getFromProcDir(), which has slow initalization time
    if(result->dePrettyName.length > 0 && result->wmPrettyName.length > 0)
        return;

    //Get missing WM / DE from processes.
    getFromProcDir(instance, result);

    //Return if both wm and de are set, or if env doesn't contain anything
    if(
        (result->wmPrettyName.length > 0 && result->dePrettyName.length > 0) ||
        !ffStrSet(env)
    ) return;

    //If nothing is set, use env as WM
    else if(result->wmPrettyName.length == 0 && result->dePrettyName.length == 0)
    {
        ffStrbufSetS(&result->wmProcessName, env);
        ffStrbufSetS(&result->wmPrettyName, env);
    }

    //If only WM is not set, and DE doesn't equal env, use env as WM
    else if(
        result->wmPrettyName.length == 0 &&
        ffStrbufIgnCaseCompS(&result->deProcessName, env) != 0 &&
        ffStrbufIgnCaseCompS(&result->dePrettyName, env) != 0
    ) {
        ffStrbufSetS(&result->wmProcessName, env);
        ffStrbufSetS(&result->wmPrettyName, env);
    }

    //If only DE is not set, and WM doesn't equal env, use env as DE
    else if(
        result->dePrettyName.length == 0 &&
        ffStrbufIgnCaseCompS(&result->wmProcessName, env) != 0 &&
        ffStrbufIgnCaseCompS(&result->wmPrettyName, env) != 0
    ) {
        ffStrbufSetS(&result->deProcessName, env);
        ffStrbufSetS(&result->dePrettyName, env);
    }
}
