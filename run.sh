#!/bin/bash
# run.sh
# Script de pruebas y benchmark para Smart Backup Kernel-Space Utility.
#
# Uso:
#   ./run.sh            — compila y ejecuta todas las pruebas
#   ./run.sh bench      — solo benchmark de rendimiento
#   ./run.sh clean      — limpia binarios y archivos temporales

set -e  # Salir ante cualquier error

BIN="./backup"
SRC_TEST="/etc/hostname"
DST_TEST="/tmp/backup_test_output.txt"

GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
NC="\033[0m"  # Sin color

ok()   { echo -e "${GREEN}[OK]${NC} $*"; }
fail() { echo -e "${RED}[FAIL]${NC} $*"; exit 1; }
info() { echo -e "${YELLOW}[INFO]${NC} $*"; }

# ── Compilar ─────────────────────────────────────────────────────────
compile() {
    info "Compilando..."
    make clean -s 2>/dev/null || true
    make 2>&1
    ok "Compilacion exitosa"
}

# ── Pruebas de correctitud ────────────────────────────────────────────
test_correctness() {
    echo ""
    info "=== PRUEBAS DE CORRECTITUD ==="

    # 1. Copia básica
    info "Prueba 1: Copia basica de $SRC_TEST"
    $BIN "$SRC_TEST" "$DST_TEST"
    if diff -q "$SRC_TEST" "$DST_TEST" > /dev/null 2>&1; then
        ok "Archivos identicos (diff OK)"
    else
        fail "Los archivos difieren"
    fi
    rm -f "$DST_TEST"

    # 2. Archivo inexistente
    info "Prueba 2: Archivo origen inexistente"
    if $BIN "/no/existe/archivo.txt" "$DST_TEST" 2>&1 | grep -q "ERROR"; then
        ok "Error detectado correctamente"
    else
        fail "Deberia haber reportado error"
    fi

    # 3. Sin permisos de escritura en destino
    info "Prueba 3: Destino sin permisos (/root/test.txt)"
    if $BIN "$SRC_TEST" "/root/sin_permiso.txt" 2>&1 | grep -q "ERROR"; then
        ok "Permiso denegado detectado correctamente"
    else
        info "(Saltando — necesita ejecutarse sin root para validar)"
    fi

    # 4. Verificar que el binario existe y es ejecutable
    info "Prueba 4: Binario ejecutable"
    if [ -x "$BIN" ]; then
        ok "Binario listo: $BIN"
    else
        fail "No se encontro el binario $BIN"
    fi

    echo ""
}

# ── Benchmark ─────────────────────────────────────────────────────────
run_benchmark() {
    echo ""
    info "=== BENCHMARK DE RENDIMIENTO ==="
    $BIN --benchmark
}

# ── Limpieza ──────────────────────────────────────────────────────────
clean() {
    make clean -s 2>/dev/null || true
    rm -f "$DST_TEST" /tmp/bk_src_* /tmp/bk_dst_*
    ok "Limpieza completada"
}

# ── Dispatcher ────────────────────────────────────────────────────────
case "${1:-all}" in
    bench)
        compile
        run_benchmark
        ;;
    clean)
        clean
        ;;
    all|*)
        compile
        test_correctness
        run_benchmark
        ;;
esac
