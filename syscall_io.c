/**
 * syscall_io.c
 * Copia de archivos usando syscalls POSIX puras: open / read / write / close.
 *
 * NO usa ningún buffer en espacio de usuario adicional al de la propia
 * llamada al sistema.  Cada read() y write() produce un context switch
 * user-mode → kernel-mode.
 *
 * Función exportada:
 *   int copy_syscall(const char *src, const char *dst, io_stats_t *stats)
 */

#include "lib_io.h"

#include <fcntl.h>      /* open, O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC */
#include <unistd.h>     /* read, write, close                          */
#include <sys/stat.h>   /* fstat, chmod                                */
#include <errno.h>
#include <stddef.h>

/* ── copy_syscall ─────────────────────────────────────────────────────────── */

/**
 * copy_syscall
 *
 * Copia src → dst usando read(2) y write(2) con un buffer de 1 byte.
 * Esto maximiza la visibilidad del overhead de context switch:
 * cada byte requiere dos transiciones user↔kernel.
 *
 * En la práctica educativa se usa un buffer de PAGE_SIZE para comparar
 * con la versión buffered; aquí intencionalmente usamos 1 byte para
 * mostrar el costo real de las syscalls sin amortización.
 *
 * @param src    Ruta del archivo origen.
 * @param dst    Ruta del archivo destino.
 * @param stats  Puntero a io_stats_t (puede ser NULL).
 * @return IO_OK o código IO_ERR_* negativo.
 */
int copy_syscall(const char *src, const char *dst, io_stats_t *stats)
{
    /* ---- Validar rutas ---- */
    if (src == NULL || *src == '\0' || dst == NULL || *dst == '\0') {
        errno = EINVAL;
        return IO_ERR_PATH;
    }

    /* ---- Inicializar stats ---- */
    if (stats) {
        stats->bytes_copied = 0;
        stats->read_calls   = 0;
        stats->write_calls  = 0;
        stats->elapsed_sec  = 0.0;
    }

    /* ---- Abrir origen ---- */
    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) {
        return (errno == EACCES || errno == EPERM) ? IO_ERR_PERM : IO_ERR_SRC;
    }

    /* ---- Obtener permisos del origen ---- */
    struct stat st;
    if (fstat(fd_src, &st) != 0) {
        close(fd_src);
        return IO_ERR_SRC;
    }

    /* ---- Crear destino ---- */
    int fd_dst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dst < 0) {
        close(fd_src);
        return (errno == EACCES || errno == EPERM) ? IO_ERR_PERM : IO_ERR_DST;
    }

    /* ---- Buffer de 1 byte: máximo overhead de syscalls ---- */
    char byte;
    ssize_t n;
    int ret = IO_OK;

    double t0 = io_now_sec();

    while ((n = read(fd_src, &byte, 1)) > 0) {
        if (stats) stats->read_calls++;

        ssize_t w = write(fd_dst, &byte, 1);
        if (w < 0) {
            ret = (errno == ENOSPC) ? IO_ERR_SPACE : IO_ERR_WRITE;
            goto done;
        }
        if (stats) {
            stats->write_calls++;
            stats->bytes_copied++;
        }
    }

    if (n < 0)
        ret = IO_ERR_READ;

done:
    close(fd_src);
    close(fd_dst);

    if (stats)
        stats->elapsed_sec = io_now_sec() - t0;

    return ret;
}
