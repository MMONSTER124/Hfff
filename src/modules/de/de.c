#include "common/printing.h"
#include "common/jsonconfig.h"
#include "detection/displayserver/displayserver.h"
#include "modules/de/de.h"
#include "util/stringUtils.h"

#define FF_DE_NUM_FORMAT_ARGS 3

void ffPrintDE(FFinstance* instance, FFDEOptions* options)
{
    const FFDisplayServerResult* result = ffConnectDisplayServer(instance);

    if(result->dePrettyName.length == 0)
    {
        ffPrintError(instance, FF_DE_MODULE_NAME, 0, &options->moduleArgs, "No DE found");
        return;
    }

    if(options->moduleArgs.outputFormat.length == 0)
    {
        ffPrintLogoAndKey(instance, FF_DE_MODULE_NAME, 0, &options->moduleArgs.key, &options->moduleArgs.keyColor);

        ffStrbufWriteTo(&result->dePrettyName, stdout);

        if(result->deVersion.length > 0)
        {
            putchar(' ');
            ffStrbufWriteTo(&result->deVersion, stdout);
        }

        putchar('\n');
    }
    else
    {
        ffPrintFormat(instance, FF_DE_MODULE_NAME, 0, &options->moduleArgs, FF_DE_NUM_FORMAT_ARGS, (FFformatarg[]){
            {FF_FORMAT_ARG_TYPE_STRBUF, &result->deProcessName},
            {FF_FORMAT_ARG_TYPE_STRBUF, &result->dePrettyName},
            {FF_FORMAT_ARG_TYPE_STRBUF, &result->deVersion}
        });
    }
}

void ffInitDEOptions(FFDEOptions* options)
{
    options->moduleName = FF_DE_MODULE_NAME;
    ffOptionInitModuleArg(&options->moduleArgs);
}

bool ffParseDECommandOptions(FFDEOptions* options, const char* key, const char* value)
{
    const char* subKey = ffOptionTestPrefix(key, FF_DE_MODULE_NAME);
    if (!subKey) return false;
    if (ffOptionParseModuleArgs(key, subKey, value, &options->moduleArgs))
        return true;

    return false;
}

void ffDestroyDEOptions(FFDEOptions* options)
{
    ffOptionDestroyModuleArg(&options->moduleArgs);
}

void ffParseDEJsonObject(FFinstance* instance, yyjson_val* module)
{
    FFDEOptions __attribute__((__cleanup__(ffDestroyDEOptions))) options;
    ffInitDEOptions(&options);

    if (module)
    {
        yyjson_val *key_, *val;
        size_t idx, max;
        yyjson_obj_foreach(module, idx, max, key_, val)
        {
            const char* key = yyjson_get_str(key_);
            if(ffStrEqualsIgnCase(key, "type"))
                continue;

            if (ffJsonConfigParseModuleArgs(key, val, &options.moduleArgs))
                continue;

            ffPrintError(instance, FF_DE_MODULE_NAME, 0, &options.moduleArgs, "Unknown JSON key %s", key);
        }
    }

    ffPrintDE(instance, &options);
}
