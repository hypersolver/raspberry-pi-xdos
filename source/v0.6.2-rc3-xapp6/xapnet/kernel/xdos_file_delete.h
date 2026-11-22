#ifndef XDOS_FILE_DELETE_H
#define XDOS_FILE_DELETE_H

#include <fatfs/ff.h>
#include <string.h>
#include <strings.h>

// Accept explicit application-data paths; the FAT boot root is outside this API.
inline FRESULT XdosDeleteFile(const char *path)
{
    if (!path || strlen(path) >= 512) return FR_INVALID_NAME;
    const char *part;
    if (!strncasecmp(path, "SD:/XDOS/", 9)) part = path + 9;
    else if (!strncasecmp(path, "USB:/", 5)) part = path + 5;
    else return FR_INVALID_NAME;

    for (;;) {
        const char *end = part;
        while (*end && *end != '/') {
            if (static_cast<unsigned char>(*end) < 32 || *end == '\\' ||
                *end == ':' || *end == '*' || *end == '?') return FR_INVALID_NAME;
            ++end;
        }
        if (end == part || end[-1] == '.' || end[-1] == ' ') return FR_INVALID_NAME;
        if (!*end) break;
        part = end + 1;
    }

    FILINFO info{};
    const FRESULT state = f_stat(path, &info);
    if (state != FR_OK) return state;
    if (info.fattrib & AM_DIR) return FR_DENIED;
    return f_unlink(path);
}
#endif
