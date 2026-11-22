#ifndef XDOS_DIRECTORY_H
#define XDOS_DIRECTORY_H

#ifdef __cplusplus
extern "C" {
#endif

enum { XDOS_DIRECTORY_LIMIT = 128 };
typedef void (*XdosDirectoryVisitor)(void *context, const char *name, int directory);

/* Names are borrowed until the callback returns. The callback runs on Core 0
 * and must not change the directory while it is being read. */
int xdos_directory_visit(const char *path, unsigned offset, unsigned capacity,
                         int directoriesOnly, XdosDirectoryVisitor visitor, void *context,
                         unsigned *emitted, int *more);

#ifdef __cplusplus
}
#endif
#endif
