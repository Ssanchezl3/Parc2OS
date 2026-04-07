/**
 * lib_io.c
 * Implementación de la librería auxiliar de I/O.
 * Funciones compartidas por syscall_io.c y syscall_buffered.c
 */

#define _POSIX_C_SOURCE 199309L

#include "lib_io.h"

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <stdio.h>

/* ── io_strerror ──────────────────────────────────────────────── */
const char *io_strerror(int code)
{
    switch (code) {
        case IO_OK:         return "Operacion exitosa";
        case IO_ERR_SRC:    return "No se pudo abrir el archivo origen";
        case IO_ERR_DST:    return "No se pudo crear el archivo destino";
        case IO_ERR_READ:   return "Error de lectura";
        case IO_ERR_WRITE:  return "Error de escritura";
        case IO_ERR_PERM:   return "Permiso denegado";
        case IO_ERR_SPACE:  return "Espacio en disco insuficiente";
        case IO_ERR_PATH:   return "Ruta nula o vacia";
        default:            return "Error desconocido";
    }
}

/* ── io_file_size ─────────────────────────────────────────────── */
ssize_t io_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;
    return (ssize_t)st.st_size;
}

/* ── io_has_space ─────────────────────────────────────────────── */
int io_has_space(const char *path, size_t needed)
{
    struct statvfs vfs;
    /* Intenta en la ruta dada; si no existe, usa el directorio actual */
    if (statvfs(path, &vfs) != 0 && statvfs(".", &vfs) != 0)
        return -1;

    unsigned long long free_bytes =
        (unsigned long long)vfs.f_bavail * (unsigned long long)vfs.f_bsize;

    return (free_bytes >= (unsigned long long)needed) ? 1 : 0;
}

/* ── io_now_sec ───────────────────────────────────────────────── */
double io_now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
