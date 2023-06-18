#include "common/printing.h"
#include "common/jsonconfig.h"
#include "detection/brightness/brightness.h"
#include "modules/brightness/brightness.h"

#define FF_BRIGHTNESS_NUM_FORMAT_ARGS 2

void ffPrintBrightness(FFinstance* instance, FFBrightnessOptions* options)
{
    FF_LIST_AUTO_DESTROY result = ffListCreate(sizeof(FFBrightnessResult));

    const char* error = ffDetectBrightness(&result);

    if(error)
    {
        ffPrintError(instance, FF_BRIGHTNESS_MODULE_NAME, 0, &options->moduleArgs, "%s", error);
        return;
    }

    if(result.length == 0)
    {
        ffPrintError(instance, FF_BRIGHTNESS_MODULE_NAME, 0, &options->moduleArgs, "No result is detected.");
        return;
    }

    FF_STRBUF_AUTO_DESTROY key = ffStrbufCreate();

    FF_LIST_FOR_EACH(FFBrightnessResult, item, result)
    {
        if(options->moduleArgs.key.length == 0)
        {
            ffStrbufAppendF(&key, "%s (%s)", FF_BRIGHTNESS_MODULE_NAME, item->name.chars);
        }
        else
        {
            ffParseFormatString(&key, &options->moduleArgs.key, 1, (FFformatarg[]){
                {FF_FORMAT_ARG_TYPE_STRBUF, &item->name}
            });
        }

        if(options->moduleArgs.outputFormat.length == 0)
        {
            ffPrintLogoAndKey(instance, key.chars, 0, NULL, &options->moduleArgs.keyColor);
            printf("%.0f%%\n", item->value);
        }
        else
        {
            ffPrintFormatString(instance, key.chars, 0, NULL, &options->moduleArgs.keyColor, &options->moduleArgs.outputFormat, FF_BRIGHTNESS_NUM_FORMAT_ARGS, (FFformatarg[]) {
                {FF_FORMAT_ARG_TYPE_STRBUF, &item->value},
                {FF_FORMAT_ARG_TYPE_FLOAT, &item->name}
            });
        }

        ffStrbufClear(&key);
        ffStrbufDestroy(&item->name);
    }
}

void ffInitBrightnessOptions(FFBrightnessOptions* options)
{
    options->moduleName = FF_BRIGHTNESS_MODULE_NAME;
    ffOptionInitModuleArg(&options->moduleArgs);
}

bool ffParseBrightnessCommandOptions(FFBrightnessOptions* options, const char* key, const char* value)
{
    const char* subKey = ffOptionTestPrefix(key, FF_BRIGHTNESS_MODULE_NAME);
    if (!subKey) return false;
    if (ffOptionParseModuleArgs(key, subKey, value, &options->moduleArgs))
        return true;

    return false;
}

void ffDestroyBrightnessOptions(FFBrightnessOptions* options)
{
    ffOptionDestroyModuleArg(&options->moduleArgs);
}

void ffParseBrightnessJsonObject(FFinstance* instance, yyjson_val* module)
{
    FFBrightnessOptions __attribute__((__cleanup__(ffDestroyBrightnessOptions))) options;
    ffInitBrightnessOptions(&options);

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

            ffPrintError(instance, FF_BRIGHTNESS_MODULE_NAME, 0, &options.moduleArgs, "Unknown JSON key %s", key);
        }
    }

    ffPrintBrightness(instance, &options);
}
