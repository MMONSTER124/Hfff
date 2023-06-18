#include "common/printing.h"
#include "common/jsonconfig.h"
#include "common/networking.h"
#include "modules/publicip/publicip.h"

#define FF_PUBLICIP_DISPLAY_NAME "Public IP"
#define FF_PUBLICIP_NUM_FORMAT_ARGS 1

static FFNetworkingState state;
static int status = -1;

void ffPreparePublicIp(FFPublicIpOptions* options)
{
    if (options->url.length == 0)
        status = ffNetworkingSendHttpRequest(&state, "ipinfo.io", "/ip", NULL);
    else
    {
        FF_STRBUF_AUTO_DESTROY host = ffStrbufCreateCopy(&options->url);
        ffStrbufSubstrAfterFirstS(&host, "://");
        uint32_t pathStartIndex = ffStrbufFirstIndexC(&host, '/');

        FF_STRBUF_AUTO_DESTROY path = ffStrbufCreate();
        if(pathStartIndex != host.length)
        {
            ffStrbufAppendNS(&path, pathStartIndex, host.chars + (host.length - pathStartIndex));
            host.length = pathStartIndex;
            host.chars[pathStartIndex] = '\0';
        }

        status = ffNetworkingSendHttpRequest(&state, host.chars, path.length == 0 ? "/" : path.chars, NULL);
    }
}

void ffPrintPublicIp(FFinstance* instance, FFPublicIpOptions* options)
{
    if (status == -1)
        ffPreparePublicIp(options);

    if (status == 0)
    {
        ffPrintError(instance, FF_PUBLICIP_DISPLAY_NAME, 0, &options->moduleArgs, "Failed to connect to an IP detection server");
        return;
    }

    FF_STRBUF_AUTO_DESTROY result = ffStrbufCreateA(4096);
    bool success = ffNetworkingRecvHttpResponse(&state, &result, options->timeout);
    if (success) ffStrbufSubstrAfterFirstS(&result, "\r\n\r\n");

    if (!success || result.length == 0)
    {
        ffPrintError(instance, FF_PUBLICIP_DISPLAY_NAME, 0, &options->moduleArgs, "Failed to receive the server response");
        return;
    }

    if (options->moduleArgs.outputFormat.length == 0)
    {
        ffPrintLogoAndKey(instance, FF_PUBLICIP_DISPLAY_NAME, 0, &options->moduleArgs.key, &options->moduleArgs.keyColor);
        ffStrbufPutTo(&result, stdout);
    }
    else
    {
        ffPrintFormat(instance, FF_PUBLICIP_DISPLAY_NAME, 0, &options->moduleArgs, FF_PUBLICIP_NUM_FORMAT_ARGS, (FFformatarg[]) {
            {FF_FORMAT_ARG_TYPE_STRBUF, &result}
        });
    }
}

void ffInitPublicIpOptions(FFPublicIpOptions* options)
{
    options->moduleName = FF_PUBLICIP_MODULE_NAME;
    ffOptionInitModuleArg(&options->moduleArgs);

    ffStrbufInit(&options->url);
    options->timeout = 0;
}

bool ffParsePublicIpCommandOptions(FFPublicIpOptions* options, const char* key, const char* value)
{
    const char* subKey = ffOptionTestPrefix(key, FF_PUBLICIP_MODULE_NAME);
    if (!subKey) return false;
    if (ffOptionParseModuleArgs(key, subKey, value, &options->moduleArgs))
        return true;

    if (strcasecmp(subKey, "url") == 0)
    {
        ffOptionParseString(key, value, &options->url);
        return true;
    }

    if (strcasecmp(subKey, "timeout") == 0)
    {
        options->timeout = ffOptionParseUInt32(key, value);
        return true;
    }

    return false;
}

void ffDestroyPublicIpOptions(FFPublicIpOptions* options)
{
    ffOptionDestroyModuleArg(&options->moduleArgs);

    ffStrbufDestroy(&options->url);
}

void ffParsePublicIpJsonObject(FFinstance* instance, yyjson_val* module)
{
    FFPublicIpOptions __attribute__((__cleanup__(ffDestroyPublicIpOptions))) options;
    ffInitPublicIpOptions(&options);

    if (module)
    {
        yyjson_val *key_, *val;
        size_t idx, max;
        yyjson_obj_foreach(module, idx, max, key_, val)
        {
            const char* key = yyjson_get_str(key_);
            if(strcasecmp(key, "type") == 0)
                continue;

            if (ffJsonConfigParseModuleArgs(key, val, &options.moduleArgs))
                continue;

            if (strcasecmp(key, "url") == 0)
            {
                ffStrbufSetS(&options.url, yyjson_get_str(val));
                continue;
            }

            if (strcasecmp(key, "timeout") == 0)
            {
                options.timeout = (uint32_t) yyjson_get_uint(val);
                continue;
            }

            ffPrintError(instance, FF_PUBLICIP_MODULE_NAME, 0, &options.moduleArgs, "Unknown JSON key %s", key);
        }
    }

    ffPrintPublicIp(instance, &options);
}
