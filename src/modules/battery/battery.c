#include "common/printing.h"
#include "common/jsonconfig.h"
#include "common/bar.h"
#include "detection/battery/battery.h"
#include "modules/battery/battery.h"
#include "util/stringUtils.h"

#define FF_BATTERY_NUM_FORMAT_ARGS 5

static void printBattery(FFinstance* instance, FFBatteryOptions* options, BatteryResult* result, uint8_t index)
{
    if(instance->config.battery.moduleArgs.outputFormat.length == 0)
    {
        ffPrintLogoAndKey(instance, FF_BATTERY_MODULE_NAME, index, &options->moduleArgs.key, &options->moduleArgs.keyColor);

        bool showStatus =
            !(instance->config.percentType & FF_PERCENTAGE_TYPE_HIDE_OTHERS_BIT) &&
            result->status.length > 0 &&
            ffStrbufIgnCaseCompS(&result->status, "Unknown") != 0;

        FF_STRBUF_AUTO_DESTROY str = ffStrbufCreate();

        if(result->capacity >= 0)
        {
            if(instance->config.percentType & FF_PERCENTAGE_TYPE_BAR_BIT)
            {
                if(result->capacity <= 20)
                    ffAppendPercentBar(instance, &str, (uint8_t)result->capacity, 10, 10, 0);
                else if(result->capacity <= 50)
                    ffAppendPercentBar(instance, &str, (uint8_t)result->capacity, 10, 0, 10);
                else
                    ffAppendPercentBar(instance, &str, (uint8_t)result->capacity, 0, 10, 10);
            }

            if(instance->config.percentType & FF_PERCENTAGE_TYPE_NUM_BIT)
            {
                if(str.length > 0)
                    ffStrbufAppendC(&str, ' ');

                ffAppendPercentNum(instance, &str, (uint8_t) result->capacity, 51, 21, str.length > 0);
            }
        }

        if(showStatus)
        {
            if(str.length > 0)
                ffStrbufAppendF(&str, " [%s]", result->status.chars);
            else
                ffStrbufAppend(&str, &result->status);
        }

        if(result->temperature == result->temperature) //FF_BATTERY_TEMP_UNSET
        {
            if(str.length > 0)
                ffStrbufAppendS(&str, " - ");

            ffStrbufAppendF(&str, "%.1f°C", result->temperature);
        }

        ffStrbufPutTo(&str, stdout);
    }
    else
    {
        ffPrintFormat(instance, FF_BATTERY_MODULE_NAME, index, &options->moduleArgs, FF_BATTERY_NUM_FORMAT_ARGS, (FFformatarg[]){
            {FF_FORMAT_ARG_TYPE_STRBUF, &result->manufacturer},
            {FF_FORMAT_ARG_TYPE_STRBUF, &result->modelName},
            {FF_FORMAT_ARG_TYPE_STRBUF, &result->technology},
            {FF_FORMAT_ARG_TYPE_DOUBLE, &result->capacity},
            {FF_FORMAT_ARG_TYPE_STRBUF, &result->status},
            {FF_FORMAT_ARG_TYPE_DOUBLE, &result->temperature},
        });
    }
}

void ffPrintBattery(FFinstance* instance, FFBatteryOptions* options)
{
    FFlist results;
    ffListInitA(&results, sizeof(BatteryResult), 0);

    const char* error = ffDetectBattery(instance, options, &results);

    if (error)
    {
        ffPrintError(instance, FF_BATTERY_MODULE_NAME, 0, &options->moduleArgs, "%s", error);
    }
    else
    {
        for(uint8_t i = 0; i < (uint8_t) results.length; i++)
        {
            BatteryResult* result = ffListGet(&results, i);
            printBattery(instance, options, result, i);

            ffStrbufDestroy(&result->manufacturer);
            ffStrbufDestroy(&result->modelName);
            ffStrbufDestroy(&result->technology);
            ffStrbufDestroy(&result->status);
        }
        if(results.length == 0)
            ffPrintError(instance, FF_BATTERY_MODULE_NAME, 0, &options->moduleArgs, "No batteries found");
    }

    ffListDestroy(&results);
}

void ffInitBatteryOptions(FFBatteryOptions* options)
{
    options->moduleName = FF_BATTERY_MODULE_NAME;
    ffOptionInitModuleArg(&options->moduleArgs);
    options->temp = false;

    #ifdef __linux__
        ffStrbufInit(&options->dir);
    #endif
}

bool ffParseBatteryCommandOptions(FFBatteryOptions* options, const char* key, const char* value)
{
    const char* subKey = ffOptionTestPrefix(key, FF_BATTERY_MODULE_NAME);
    if (!subKey) return false;
    if (ffOptionParseModuleArgs(key, subKey, value, &options->moduleArgs))
        return true;

    if (ffStrEqualsIgnCase(subKey, "temp"))
    {
        options->temp = ffOptionParseBoolean(value);
        return true;
    }

    #ifdef __linux__
        if (ffStrEqualsIgnCase(subKey, "dir"))
        {
            ffOptionParseString(key, value, &options->dir);
            return true;
        }
    #endif

    return false;
}

void ffDestroyBatteryOptions(FFBatteryOptions* options)
{
    ffOptionDestroyModuleArg(&options->moduleArgs);

    #ifdef __linux__
        ffStrbufDestroy(&options->dir);
    #endif
}

void ffParseBatteryJsonObject(FFinstance* instance, yyjson_val* module)
{
    FFBatteryOptions __attribute__((__cleanup__(ffDestroyBatteryOptions))) options;
    ffInitBatteryOptions(&options);

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

            #ifdef __linux__
            if (ffStrEqualsIgnCase(key, "dir"))
            {
                ffStrbufSetS(&options.dir, yyjson_get_str(val));
                continue;
            }
            #endif

            if (ffStrEqualsIgnCase(key, "temp"))
            {
                options.temp = yyjson_get_bool(val);
                continue;
            }

            ffPrintError(instance, FF_BATTERY_MODULE_NAME, 0, &options.moduleArgs, "Unknown JSON key %s", key);
        }
    }

    ffPrintBattery(instance, &options);
}
