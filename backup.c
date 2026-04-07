/**
 * backup.c
 * Interfaz de usuario del Smart Backup Kernel-Space Utility.
 *
 * Modos de uso:
 *   ./backup <origen> <destino>          — copia con copy_buffered
 *   ./backup --benchmark                 — tabla comparativa 3 métodos
 *   ./backup --help
 *
 * Los tres métodos comparados son:
 *   1. copy_syscall  : syscalls puras, buffer 1 byte  (máximo overhead)
 *   2. copy_buffered : syscalls con buffer 4 KB       (óptimo para kernel)
 *   3. copy_stdlib   : fread/fwrite con buffer libc   (referencia estándar)
 */

#define _POSIX_C_SOURCE 199309L

#include "lib_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* ── Declaraciones externas ─────────────────────────────────────── */
int copy_syscall (const char *src, const char *dst, io_stats_t *stats);
int copy_buffered(const char *src, const char *dst, io_stats_t *stats);
int copy_stdlib  (const char *src, const char *dst, io_stats_t *stats);

/* ── Configuración del benchmark ────────────────────────────────── */
#define BENCH_RUNS   3

static const struct { const char *label; size_t size; } SIZES[] = {
    { "1 KB",   1024UL                   },
    { "1 MB",   1024UL * 1024UL          },
    { "10 MB",  10UL  * 1024UL * 1024UL  },
    { "100 MB", 100UL * 1024UL * 1024UL  },
};
#define N_SIZES (sizeof(SIZES)/sizeof(SIZES[0]))

/* ── Helpers ─────────────────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "\nSmart Backup Kernel-Space Utility\n"
        "Uso:\n"
        "  %s <origen> <destino>   Copia un archivo (método buffered)\n"
        "  %s --benchmark          Tabla comparativa de los 3 métodos\n"
        "  %s --help               Muestra esta ayuda\n\n",
        prog, prog, prog);
}

/** Genera un archivo de tamaño exacto con datos pseudoaleatorios. */
static int make_test_file(const char *path, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { perror(path); return -1; }

    unsigned char buf[PAGE_SIZE];
    for (int i = 0; i < PAGE_SIZE; i++) buf[i] = (unsigned char)(i & 0xFF);

    size_t rem = size;
    while (rem > 0) {
        size_t chunk = rem > PAGE_SIZE ? PAGE_SIZE : rem;
        if (fwrite(buf, 1, chunk, fp) != chunk) {
            fclose(fp); return -1;
        }
        rem -= chunk;
    }
    fclose(fp);
    return 0;
}

/** Throughput en MB/s. */
static double mbps(size_t bytes, double secs)
{
    return (secs > 0) ? (bytes / (1024.0 * 1024.0)) / secs : 0.0;
}

/* ── Benchmark ───────────────────────────────────────────────────── */

static void run_benchmark(void)
{
    printf("\n");
    printf("============================================================"
           "===================\n");
    printf("  BENCHMARK — 3 métodos de copia | Buffer: %d B | Runs: %d\n",
           PAGE_SIZE, BENCH_RUNS);
    printf("============================================================"
           "===================\n\n");

    /* Nota: copy_syscall (1 byte) es muy lento para >1KB en este entorno,
       así que solo lo corremos en 1KB para mostrar el overhead educativo. */
    printf("%-10s | %-22s | %-22s | %-22s\n",
           "Tamaño",
           "syscall pura (1B buf)",
           "syscall buffered (4KB)",
           "stdlib fread/fwrite");
    printf("%-10s-+-%-22s-+-%-22s-+-%-22s\n",
           "----------",
           "----------------------",
           "----------------------",
           "----------------------");

    char src[64], dst[64];

    for (size_t i = 0; i < N_SIZES; i++) {
        const char *label = SIZES[i].label;
        size_t       sz   = SIZES[i].size;

        snprintf(src, sizeof(src), "/tmp/bk_src_%zu", sz);
        snprintf(dst, sizeof(dst), "/tmp/bk_dst_%zu", sz);

        if (make_test_file(src, sz) != 0) {
            printf("%-10s | [ERROR generando archivo]\n", label);
            continue;
        }

        double tot_raw = 0, tot_buf = 0, tot_std = 0;
        io_stats_t st;
        int rc;

        /* copy_syscall solo en 1 KB (muy lento para más) */
        int do_raw = (sz <= 1024);

        if (do_raw) {
            for (int r = 0; r < BENCH_RUNS; r++) {
                unlink(dst);
                rc = copy_syscall(src, dst, &st);
                if (rc != IO_OK) { tot_raw = -1; break; }
                tot_raw += st.elapsed_sec;
            }
        }

        /* copy_buffered */
        for (int r = 0; r < BENCH_RUNS; r++) {
            unlink(dst);
            rc = copy_buffered(src, dst, &st);
            if (rc != IO_OK) { tot_buf = -1; break; }
            tot_buf += st.elapsed_sec;
        }

        /* copy_stdlib */
        for (int r = 0; r < BENCH_RUNS; r++) {
            unlink(dst);
            rc = copy_stdlib(src, dst, &st);
            if (rc != IO_OK) { tot_std = -1; break; }
            tot_std += st.elapsed_sec;
        }

        /* Promedios */
        double avg_raw = do_raw ? tot_raw / BENCH_RUNS : -1;
        double avg_buf = tot_buf / BENCH_RUNS;
        double avg_std = tot_std / BENCH_RUNS;

        /* Imprimir fila */
        char col_raw[24], col_buf[24], col_std[24];

        if (avg_raw < 0)
            snprintf(col_raw, sizeof(col_raw), "%-22s", "(omitido)");
        else
            snprintf(col_raw, sizeof(col_raw), "%.6fs %7.2fMB/s",
                     avg_raw, mbps(sz, avg_raw));

        snprintf(col_buf, sizeof(col_buf), "%.6fs %7.2fMB/s",
                 avg_buf, mbps(sz, avg_buf));
        snprintf(col_std, sizeof(col_std), "%.6fs %7.2fMB/s",
                 avg_std, mbps(sz, avg_std));

        printf("%-10s | %-22s | %-22s | %-22s\n",
               label, col_raw, col_buf, col_std);

        unlink(src);
        unlink(dst);
    }

    printf("\n");
    printf("Análisis:\n");
    printf("  • syscall pura (1 B): cada byte = 2 context switches. "
           "Costo O(n).\n");
    printf("  • syscall buffered (4 KB): amortiza switches, throughput "
           "cercano al hardware.\n");
    printf("  • stdlib: buffer interno de libc (~8 KB) reduce switches "
           "para archivos pequeños;\n");
    printf("    para archivos grandes, el costo de copia extra en espacio "
           "de usuario lo equilibra.\n\n");
}

/* ── main ────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "--benchmark") == 0) {
        run_benchmark();
        return EXIT_SUCCESS;
    }

    /* ---- Copia simple ---- */
    if (argc < 3) {
        fprintf(stderr, "Error: se necesita <origen> y <destino>.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    printf("\nCopiando: %s -> %s\n", src, dst);

    io_stats_t stats;
    int rc = copy_buffered(src, dst, &stats);

    if (rc != IO_OK) {
        fprintf(stderr, "[ERROR] %s (errno: %s)\n",
                io_strerror(rc), strerror(errno));
        return EXIT_FAILURE;
    }

    printf("[OK] %zu bytes | %.6f s | %.2f MB/s\n",
           stats.bytes_copied,
           stats.elapsed_sec,
           mbps(stats.bytes_copied, stats.elapsed_sec));

    return EXIT_SUCCESS;
}
