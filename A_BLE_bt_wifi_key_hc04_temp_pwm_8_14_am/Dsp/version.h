#ifndef __VERSION_H
#define __VERSION_H

#include <stdint.h>

/* Version info magic: 'VER1' (0x56455231) - identifies valid version block */
#define VERSION_MAGIC  0x56455231u

/* Each program defines its own g_version in main.c */
typedef struct {
    uint32_t magic;        /* magic = VERSION_MAGIC */
    uint16_t ver_major;    /* major version number */
    uint16_t ver_minor;    /* minor version number */
    uint32_t build_date;   /* build date as YYYYMMDD */
    const char *name;      /* program name string */
} version_info_t;

extern const version_info_t g_version;

#endif /* __VERSION_H */
