/**
 * syscall_buffered.c
 * Copia de archivos usando syscalls POSIX con buffer de PAGE_SIZE (4 KB).
 *
 * Al agrupar las lecturas/escrituras en bloques de 4 KB se reduce el
 * número de context switches en un factor de 4096x respecto a la versión
 * de 1 byte (syscall_io.c), maximizando el throughput.
 *
 * También implementa copy_stdlib() usando fread/fwrite para comparación.
 *
 * Funciones exportadas:
 *   int copy_buffered(const char *src, const char *dst, io_stats_t *stats)
 *   int copy_stdlib (const char *src, const char *dst, io_stats_t *stats)
 */

#include "lib_io.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>      /* solo para copy_stdlib */
#include <stddef.h>

/* ══════════════════════════════════════════════════════════════════
 * copy_buffered  —  syscall con buffer de 4 KB
 * ══════════════════════════════════════════════════════════════════ */

/**
 * copy_buffered
 *
 * Usa open/read/write con un buffer estático de PAGE_SIZE bytes.
 * El buffer está declarado static para no presionar el stack en
 * archivos grandes y para reutilizarse entre llamadas.
 *
 * Garantías:
 *  - Escritura completa aunque write() devuelva parcial (bucle interno).
 *  - Cierre de descriptores siempre, incluso ante error.
 *  - Registro de la operación en syslog.
 *  - Preservación del modo (permisos) del archivo origen.
 *
 * @param src    Ruta del archivo origen.
 * @param dst    Ruta del archivo destino.
 * @param stats  Puntero a io_stats_t (puede ser NULL).
 * @return IO_OK o código IO_ERR_* negativo.
 */
int copy_buffered(const char *src, const char *dst, io_stats_t *stats)
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

    /* ---- Metadatos del origen ---- */
    struct stat st;
    if (fstat(fd_src, &st) != 0) {
        close(fd_src);
        return IO_ERR_SRC;
    }

    /* ---- Verificar espacio disponible ---- */
    if (io_has_space(dst, (size_t)st.st_size) == 0) {
        close(fd_src);
        errno = ENOSPC;
        return IO_ERR_SPACE;
    }

    /* ---- Crear destino ---- */
    int fd_dst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dst < 0) {
        close(fd_src);
        return (errno == EACCES || errno == EPERM) ? IO_ERR_PERM : IO_ERR_DST;
    }

    /* ---- Buffer de página (4 KB) ---- */
    static char buf[PAGE_SIZE];

    double  t0  = io_now_sec();
    ssize_t nr;
    int     ret = IO_OK;

    while ((nr = read(fd_src, buf, PAGE_SIZE)) > 0) {
        if (stats) stats->read_calls++;

        /* Escritura garantizada: write() puede ser parcial */
        ssize_t written   = 0;
        ssize_t remaining = nr;

        while (remaining > 0) {
            ssize_t nw = write(fd_dst, buf + written, (size_t)remaining);
            if (nw < 0) {
                ret = (errno == ENOSPC) ? IO_ERR_SPACE : IO_ERR_WRITE;
                goto done;
            }
            if (stats) stats->write_calls++;
            written   += nw;
            remaining -= nw;
        }

        if (stats) stats->bytes_copied += (size_t)nr;
    }

    if (nr < 0)
        ret = IO_ERR_READ;

done:
    close(fd_src);
    close(fd_dst);

    /* Preservar permisos explícitamente (umask puede haberlos alterado) */
    if (ret == IO_OK)
        chmod(dst, st.st_mode);

    /* Registrar en syslog */
    openlog("smart_backup", LOG_PID | LOG_NDELAY, LOG_USER);
    if (ret == IO_OK)
        syslog(LOG_INFO, "copy_buffered OK | %s -> %s | %zu bytes",
               src, dst, stats ? stats->bytes_copied : (size_t)st.st_size);
    else
        syslog(LOG_ERR, "copy_buffered FAIL | %s -> %s | errno=%d",
               src, dst, errno);
    closelog();

    if (stats)
        stats->elapsed_sec = io_now_sec() - t0;

    return ret;
}

/* ══════════════════════════════════════════════════════════════════
 * copy_stdlib  —  implementación con librería estándar C (stdio)
 * Sirve como referencia de comparación para el benchmark.
 * ══════════════════════════════════════════════════════════════════ */

/**
 * copy_stdlib
 *
 * Usa fopen / fread / fwrite / fclose.
 * FILE* mantiene un buffer interno (~8 KB en glibc) que acumula datos
 * en espacio de usuario antes de emitir write(2), reduciendo context
 * switches para archivos pequeños.
 *
 * @param src    Ruta del archivo origen.
 * @param dst    Ruta del archivo destino.
 * @param stats  Puntero a io_stats_t (puede ser NULL).
 * @return IO_OK o código IO_ERR_* negativo.
 */
int copy_stdlib(const char *src, const char *dst, io_stats_t *stats)
{
    if (src == NULL || *src == '\0' || dst == NULL || *dst == '\0') {
        errno = EINVAL;
        return IO_ERR_PATH;
    }

    if (stats) {
        stats->bytes_copied = 0;
        stats->read_calls   = 0;
        stats->write_calls  = 0;
        stats->elapsed_sec  = 0.0;
    }

    FILE *fp_src = fopen(src, "rb");
    if (fp_src == NULL)
        return (errno == EACCES || errno == EPERM) ? IO_ERR_PERM : IO_ERR_SRC;

    FILE *fp_dst = fopen(dst, "wb");
    if (fp_dst == NULL) {
        fclose(fp_src);
        return (errno == EACCES || errno == EPERM) ? IO_ERR_PERM : IO_ERR_DST;
    }

    static char buf[PAGE_SIZE];
    double t0  = io_now_sec();
    size_t nr;
    int    ret = IO_OK;

    while ((nr = fread(buf, 1, PAGE_SIZE, fp_src)) > 0) {
        if (stats) stats->read_calls++;

        size_t nw = fwrite(buf, 1, nr, fp_dst);
        if (stats) stats->write_calls++;

        if (nw != nr) {
            ret = (errno == ENOSPC) ? IO_ERR_SPACE : IO_ERR_WRITE;
            break;
        }
        if (stats) stats->bytes_copied += nr;
    }

    if (ferror(fp_src) && ret == IO_OK)
        ret = IO_ERR_READ;

    fclose(fp_src);
    fclose(fp_dst);

    if (stats)
        stats->elapsed_sec = io_now_sec() - t0;

    return ret;
}
