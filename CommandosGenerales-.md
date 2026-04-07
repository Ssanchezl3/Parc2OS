Commands Generales
# Ver ayuda
./backup --help

# Copiar un archivo
./backup <origen> <destino>

# Benchmark completo (tabla de 3 métodos)
./backup --benchmark

Make
make          # compilar
make test     # copia de prueba con /etc/hostname
make bench    # benchmark
make clean    # eliminar .o y binario

bash run.sh         # compilar + pruebas + benchmark
bash run.sh bench   # solo benchmark
bash run.sh clean   # limpiar


Ejemplos concretos: Como clonar un archivo, el mas basico siendo un .txt
bash./backup /etc/passwd /tmp/passwd_backup
./backup /etc/hostname /tmp/hostname_backup
./backup archivo_grande.iso /tmp/copia.iso

PS: Para verificar que la informacion si haya sido copiada, asegurarse de correr (Differentes nombres o differentes archivos.)
cat archivo_original.txt (y que si tenga informacion ya sea dada or hello world)
