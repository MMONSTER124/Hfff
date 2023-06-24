#include "bios.h"
#include "util/windows/registry.h"

const char* ffDetectBios(FFBiosResult* bios)
{
    FF_HKEY_AUTO_DESTROY hKey = NULL;
    if(!ffRegOpenKeyForRead(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", &hKey, NULL))
        return "ffRegOpenKeyForRead(HKEY_LOCAL_MACHINE, L\"HARDWARE\\DESCRIPTION\\System\\BIOS\", &hKey, NULL) failed";

    if(!ffRegReadStrbuf(hKey, L"BIOSVersion", &bios->version, NULL))
        return "\"HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS\\BIOSVersion\" doesn't exist";

    ffRegReadStrbuf(hKey, L"BIOSVendor", &bios->vendor, NULL);
    ffRegReadStrbuf(hKey, L"BIOSReleaseDate", &bios->date, NULL);

    uint32_t major, minor;
    if(
        ffRegReadUint(hKey, L"BiosMajorRelease", &major, NULL) &&
        ffRegReadUint(hKey, L"BiosMinorRelease", &minor, NULL)
    )
        ffStrbufAppendF(&bios->biosRelease, "%u.%u", (unsigned)major, (unsigned)minor);

    return NULL;
}
