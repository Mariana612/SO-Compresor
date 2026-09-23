# SO-Compresor

## Compresor Huffman + MD5

El programa comprime todos los archivos regulares que están directamente dentro
de un directorio (ignora subdirectorios y archivos `.huff`) y los guarda en **un
solo archivo `.huff`**. Por cada archivo se calcula su firma MD5 antes de
comprimirlo y se guarda en la tabla de metadatos del `.huff`, junto con su nombre,
tamaños y tabla de frecuencias Huffman. Al descomprimir, se expande el `.huff` en
el directorio indicado, se recalcula el MD5 de cada archivo y se compara con el
guardado. MD5 detecta cambios accidentales; no es un mecanismo de seguridad
criptográfica.

## Requisitos

Se requiere un compilador C11 (`gcc`), `make`, las bibliotecas POSIX de Linux y
OpenSSL `libcrypto` para generar MD5 mediante su biblioteca, sin implementar el
algoritmo manualmente.
```bash
sudo apt-get install build-essential
sudo apt-get install libssl-dev
```

## Estructura

```
src/common/    código compartido (Huffman, MD5, formato .huff, CLI, estadísticas)
src/serial/    variante serial
src/fork/      variante paralela con fork() + pipes
src/pthread/   variante concurrente con pthread + memoria compartida
src/gui/       interfaz gráfica (GTK 4)
scripts/       descarga de los libros y limpieza de .huff (ver scripts/README.md)
data/gutenberg/  los 100 libros .txt del top de Gutenberg (últimos 30 días)
bin/           binarios generados por make (no se versiona)
```

## Compilar

Desde la raíz del proyecto:

```bash
make            # las tres variantes en bin/: huffman-serial, huffman-fork, huffman-pthread
make fork       # solo una variante (serial, fork o pthread)
make gui        # interfaz gráfica en bin/interfaz (requiere libgtk-4-dev)
make clean
```

## Uso

Las tres variantes aceptan los mismos argumentos y generan exactamente el mismo
`.huff`, así que lo que comprime una lo puede descomprimir cualquiera de las otras.

```bash
bin/huffman-serial c <directorio> [archivo.huff]
bin/huffman-serial d <archivo.huff> <directorio_salida>
```

Si no se indica el nombre del `.huff`, se usa `<directorio>.huff` al lado del
directorio. El directorio de salida se crea si no existe.

```bash
bin/huffman-serial c data/gutenberg           # crea data/gutenberg.huff
bin/huffman-serial d data/gutenberg.huff salida_serial
```

`python3 scripts/limpiar_huff.py` borra los `.huff` generados dentro de `data/`.

## Estadísticas

Lo único que el programa escribe en `stdout` es una línea `clave=valor` con las
estadísticas de la corrida, pensada para que la lea la interfaz (los errores van
a `stderr`):

```
variante=fork operacion=c archivos=100 original=82287268 comprimido=48232800 tiempo=0.311995
variante=fork operacion=d archivos=100 verificados=100 salud=100.00 original=82287268 comprimido=48232800 tiempo=0.619989
```

- `salud`: firmas MD5 verificadas / cantidad de archivos, en % (solo al descomprimir).
  Si alguna firma no coincide la descompresión sigue con los demás archivos, la
  salud baja y el programa termina con código 1.
- `tiempo`: segundos totales de la corrida.
- `original` / `comprimido`: bytes originales y tamaño del `.huff`.

La aceleración respecto a la serial necesita los tiempos de dos corridas, así que
se calcula con `stats_speedup_percent(tiempo_serial, tiempo)` de
`src/common/Stats.h`: `(Ts / T - 1) x 100` (el doble de rápido = 100 %).

## Formato del archivo `.huff` (HUF3)

```
| "HUF3" | cantidad N | tabla de N entradas | datos comprimidos de cada archivo |
entrada: nombre (256 B) | tamaño original | tamaño comprimido | offset de datos | MD5 (16 B) | frecuencias (256 x 8 B)
```

El tamaño comprimido de cada archivo se calcula antes de codificarlo
(`Σ frecuencia × largo del código`, redondeado a bytes). Por eso la compresión
tiene dos fases:

1. **Análisis:** MD5, frecuencias y tamaño comprimido de cada archivo.
2. **Codificación:** con los offsets ya asignados y la tabla escrita al inicio del
   `.huff`, cada archivo se escribe en su propia región del `.huff`.

Como las regiones no se solapan, cada fase se puede repartir entre procesos o
hilos. Al descomprimir, cada trabajador lee la región de su archivo, lo expande
y verifica su MD5.

## Código compartido

`src/common` contiene todo lo común:
- `HuffmanTree`: árbol y códigos de Huffman.
- `MD5Utils`: MD5 con la API EVP de OpenSSL.
- `FileList`: listado de los archivos de un directorio.
- `Codec`: formato HUF3 y compresión/descompresión de cada archivo.
- `Cli`: argumentos y medición de tiempo.
- `Stats`: salud, aceleración e impresión de las estadísticas.

Las carpetas `serial`, `fork` y `pthread` solo contienen su forma de repartir
el trabajo.

## Variante serial

`src/serial` procesa los archivos uno por uno.

## Variante paralela con fork

`src/fork/ProcessPool.c` crea un proceso hijo por archivo, con a lo sumo un
hijo por procesador (máximo 64) trabajando a la vez. El padre recoge con
`waitpid(-1)` al primer hijo que termine, así que un archivo grande no frena el
lanzamiento de nuevos hijos.

La comunicación entre procesos (IPC) se hace con una **pipe por hijo**:
- En la fase de análisis, el hijo le manda al padre por la pipe la entrada
  completa de su archivo (MD5, frecuencias y tamaños). Con eso el padre arma la
  tabla del `.huff`.
- En la codificación y en la descompresión, el hijo manda por la pipe si su
  archivo se procesó y verificó correctamente; el padre cuenta las firmas
  verificadas para la salud.

## Variante concurrente con pthread

`src/pthread/ThreadPool.c` crea un hilo por procesador (máximo 64). Los hilos
toman trabajo de una cola ubicada en una **región de memoria compartida creada con
`mmap`** y protegida por un mutex de `pthread`. En la compresión, la tabla de
metadatos también vive en memoria compartida: cada hilo escribe ahí la entrada de
su archivo y el hilo principal la usa para escribir la cabecera del `.huff`.
