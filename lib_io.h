/**
 * lib_io.h
 * Librería auxiliar de I/O para el Smart Backup Utility.
 * Provee utilidades compartidas por syscall_io.c y syscall_buffered.c
 */

#ifndef LIB_IO_H
#define LIB_IO_H

#include <stddef.h>
#include <sys/types.h>

/* ── Constantes ─────────────────────────────────────────────── */
#define PAGE_SIZE       4096        /* Tamaño de página del kernel  */
#define MAX_PATH        4096        /* Máxima longitud de ruta      */

/* Códigos de retorno */
#define IO_OK            0
#define IO_ERR_SRC      -1
#define IO_ERR_DST      -2
#define IO_ERR_READ     -3
#define IO_ERR_WRITE    -4
#define IO_ERR_PERM     -5
#define IO_ERR_SPACE    -6
#define IO_ERR_PATH     -7

/* ── Estructura de métricas ─────────────────────────────────── */
typedef struct {
    size_t   bytes_copied;
    unsigned read_calls;
    unsigned write_calls;
    double   elapsed_sec;
} io_stats_t;

/* ── Funciones utilitarias ──────────────────────────────────── */

/**
 * io_strerror  — Traduce código IO_ERR_* a mensaje legible.
 */
const char *io_strerror(int code);

/**
 * io_file_size — Devuelve el tamaño en bytes de un archivo.
 *                Retorna -1 si no existe o no es accesible.
 */
ssize_t io_file_size(const char *path);

/**
 * io_has_space  — Verifica que haya al menos 'needed' bytes libres
 *                 en el sistema de ficheros de 'path'.
 *                 Retorna 1 si hay espacio, 0 si no, -1 en error.
 */
int io_has_space(const char *path, size_t needed);

/**
 * io_now_sec   — Tiempo monotónico en segundos (resolución nanosegundo).
 */
double io_now_sec(void);

#endif /* LIB_IO_H */
