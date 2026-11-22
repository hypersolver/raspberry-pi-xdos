
#include "xdos_directory.h"

extern "C" int xdos_directory_visit(const char *path, unsigned offset, unsigned capacity,
                                    int directoriesOnly, XdosDirectoryVisitor visitor,
                                    void *context, unsigned *emitted, int *more)
{
    if (!path || !visitor || !emitted || !more || !capacity) return FR_INVALID_PARAMETER;
    *emitted=0;
    *more=0;
    if (capacity>XDOS_DIRECTORY_LIMIT) capacity=XDOS_DIRECTORY_LIMIT;
    DIR dir;
    auto result=f_opendir(&dir,path);
    if (result!=FR_OK) return result;
    FILINFO entry;
    for (;;) {
        result=f_readdir(&dir,&entry);
        if (result!=FR_OK || !entry.fname[0]) break;
        const bool directory=(entry.fattrib&AM_DIR)!=0;
        if (directoriesOnly && !directory) continue;
        if (offset) {
            --offset;
            continue;
        }
        if (*emitted==capacity) {
            *more=1;
            break;
        }
        visitor(context,entry.fname,directory);
        ++*emitted;
    }
    const auto closed=f_closedir(&dir);
    return result==FR_OK ? closed : result;
}
