#include "displayserver.h"
#include "detection/os/os.h"
#include "util/windows/unicode.h"

#include <dwmapi.h>
#include <WinUser.h>
#include <wchar.h>

static CALLBACK BOOL MonitorEnumProc(
  HMONITOR hMonitor,
  FF_MAYBE_UNUSED HDC hdc,
  FF_MAYBE_UNUSED LPRECT lpRect,
  LPARAM lParam
)
{
    FFlist* monitors = (FFlist*) lParam;
    MONITORINFOEXW* newMonitor = ffListAdd(monitors);
    newMonitor->cbSize = sizeof(*newMonitor);
    return GetMonitorInfoW(hMonitor, (MONITORINFO*) newMonitor);
}

static void detectDisplays(FFDisplayServerResult* ds)
{
    FF_LIST_AUTO_DESTROY monitors = ffListCreate(sizeof(MONITORINFOEXW));
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM) &monitors);

    DISPLAYCONFIG_PATH_INFO paths[128];
    uint32_t pathCount = sizeof(paths) / sizeof(paths[0]);
    DISPLAYCONFIG_MODE_INFO modes[256];
    uint32_t modeCount = sizeof(modes) / sizeof(modes[0]);

    if (QueryDisplayConfig(
        QDC_ONLY_ACTIVE_PATHS,
        &pathCount,
        paths,
        &modeCount,
        modes,
        NULL) == ERROR_SUCCESS)
    {
        for (uint32_t i = 0; i < pathCount; ++i)
        {
            DISPLAYCONFIG_PATH_INFO* path = &paths[i];

            DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {
                .header = {
                    .type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,
                    .size = sizeof(sourceName),
                    .adapterId = path->sourceInfo.adapterId,
                    .id = path->sourceInfo.id,
                },
            };

            MONITORINFOEXW* monitorInfo = NULL;
            if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
            {
                FF_LIST_FOR_EACH(MONITORINFOEXW, item, monitors)
                {
                    if (wcsncmp(item->szDevice, sourceName.viewGdiDeviceName, sizeof(sourceName.viewGdiDeviceName) / sizeof(wchar_t)) == 0)
                    {
                        monitorInfo = item;
                        break;
                    }
                }
            }
            if (!monitorInfo) continue;

            FF_STRBUF_AUTO_DESTROY name = ffStrbufCreate();

            DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {
                .header = {
                    .type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME,
                    .size = sizeof(targetName),
                    .adapterId = path->targetInfo.adapterId,
                    .id = path->targetInfo.id,
                },
            };
            if(DisplayConfigGetDeviceInfo(&targetName.header) == ERROR_SUCCESS)
            {
                if (targetName.flags.friendlyNameFromEdid)
                    ffStrbufSetWS(&name, targetName.monitorFriendlyDeviceName);
                else
                {
                    ffStrbufSetWS(&name, targetName.monitorDevicePath);
                    ffStrbufSubstrAfterFirstC(&name, '#');
                    ffStrbufSubstrBeforeFirstC(&name, '#');
                }
            }

            uint32_t width = modes[path->sourceInfo.modeInfoIdx].sourceMode.width;
            uint32_t height = modes[path->sourceInfo.modeInfoIdx].sourceMode.height;
            if (path->targetInfo.rotation == DISPLAYCONFIG_ROTATION_ROTATE90 ||
                path->targetInfo.rotation == DISPLAYCONFIG_ROTATION_ROTATE270)
            {
                uint32_t temp = width;
                width = height;
                height = temp;
            }

            uint32_t rotation;
            switch (path->targetInfo.rotation)
            {
                case DISPLAYCONFIG_ROTATION_ROTATE90:
                    rotation = 90;
                    break;
                case DISPLAYCONFIG_ROTATION_ROTATE180:
                    rotation = 180;
                    break;
                case DISPLAYCONFIG_ROTATION_ROTATE270:
                    rotation = 270;
                    break;
                default:
                    rotation = 0;
                    break;
            }

            ffdsAppendDisplay(ds,
                width,
                height,
                path->targetInfo.refreshRate.Numerator / (double) path->targetInfo.refreshRate.Denominator,
                (uint32_t) (monitorInfo->rcMonitor.right - monitorInfo->rcMonitor.left),
                (uint32_t) (monitorInfo->rcMonitor.bottom - monitorInfo->rcMonitor.top),
                rotation,
                &name,
                path->targetInfo.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL ||
                path->targetInfo.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED ||
                    path->targetInfo.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED
                    ? FF_DISPLAY_TYPE_BUILTIN : FF_DISPLAY_TYPE_EXTERNAL,
                !!(monitorInfo->dwFlags & MONITORINFOF_PRIMARY)
            );
        }
    }
}

void ffConnectDisplayServerImpl(FFDisplayServerResult* ds, const FFinstance* instance)
{
    BOOL enabled;
    if(SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && enabled == TRUE)
    {
        ffStrbufInitS(&ds->wmProcessName, "dwm.exe");
        ffStrbufInitS(&ds->wmPrettyName, "Desktop Window Manager");
    }
    else
    {
        ffStrbufInitS(&ds->wmProcessName, "internal");
        ffStrbufInitS(&ds->wmPrettyName, "internal");
    }
    ffStrbufInit(&ds->wmProtocolName);
    ffStrbufInit(&ds->deProcessName);
    ffStrbufInit(&ds->dePrettyName);
    ffStrbufInit(&ds->deVersion);
    ffListInit(&ds->displays, sizeof(FFDisplayResult));

    detectDisplays(ds);

    //https://github.com/hykilpikonna/hyfetch/blob/master/neofetch#L2067
    const FFOSResult* os = ffDetectOS(instance);
    if(
        ffStrbufEqualS(&os->version, "11") ||
        ffStrbufEqualS(&os->version, "10") ||
        ffStrbufEqualS(&os->version, "2022") ||
        ffStrbufEqualS(&os->version, "2019") ||
        ffStrbufEqualS(&os->version, "2016")
    ) ffStrbufSetS(&ds->dePrettyName, "Fluent");
    else if(
        ffStrbufEqualS(&os->version, "8") ||
        ffStrbufEqualS(&os->version, "8.1") ||
        ffStrbufEqualS(&os->version, "2012 R2") ||
        ffStrbufEqualS(&os->version, "2012")
    ) ffStrbufSetS(&ds->dePrettyName, "Metro");
    else
        ffStrbufSetS(&ds->dePrettyName, "Aero");
}
