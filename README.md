# SO-Compresor

## Compresor Huffman + MD5

El programa comprime todos los archivos regulares directamente dentro de un
directorio (excepto los `.huff` ya comprimidos). Cada `.huff` conserva el tamaño, la tabla de frecuencias Huffman,
los bits comprimidos y la firma MD5 del contenido original. MD5 detecta cambios
accidentales; no es un mecanismo de seguridad criptográfica.

## Requisitos

Se requiere un compilador C11 (`gcc`), las bibliotecas POSIX de Linux y
OpenSSL `libcrypto` para generar MD5 mediante su biblioteca, sin implementar el
algoritmo manualmente. No es necesario crear un usuario nuevo.
```bash
sudo apt-get install build-essential
sudo apt-get install libssl-dev
```

## Compilar

Desde la raíz del proyecto, para la variante serial:

```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic \
	-I Code/Commons -o huffman-serial Code/Serial/main.c \
	Code/Serial/Compressor.c Code/Serial/Decompressor.c \
	Code/Commons/Codec.c Code/Commons/HuffmanTree.c \
	Code/Commons/MD5Utils.c Code/Commons/FileList.c -lcrypto
```

## Uso

```bash
./huffman-serial c Eliminar/gutenberg_txt
mkdir -p Eliminar/gutenberg_txt_serial
./huffman-serial d Eliminar/gutenberg_txt Eliminar/gutenberg_txt_serial
```

La carpeta `Eliminar/gutenberg_txt` contiene los 100 documentos `.txt` y el
programa los procesa uno por uno. Ignora subdirectorios y archivos `.huff`.
Cada `.huff` se escribe junto a su archivo original.

En las tres variantes, `d` acepta un directorio de archivos `.huff` o un único
archivo `.huff`, y verifica el MD5 de cada resultado:

```bash
./huffman-serial d <directorio_huff | archivo.huff> <directorio_salida>
```

## Código compartido

`Code/Commons` contiene la implementación única del formato HUF2: árbol
Huffman, MD5 (API EVP de OpenSSL), listado de archivos de un directorio y
compresión/descompresión de un archivo. Las carpetas `Serial`,
`Fork` y `Pthread` solo contienen sus respectivos recorridos o estrategias de
concurrencia y se enlazan contra esos módulos comunes.

## Variante paralela con fork

La implementación paralela está en `Code/Fork`. El padre crea hijos para
procesar archivos independientes y recibe el resultado de cada hijo mediante
una pipe. El padre recoge con `waitpid(-1)` al primer hijo que termine, de modo
que un archivo grande no bloquea el lanzamiento de nuevos hijos. La cantidad
máxima de hijos se ajusta al número de procesadores
disponibles, con un límite de 64.

```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic \
	-I Code/Commons -o huffman-fork Code/Fork/main.c Code/Fork/Compressor.c \
	Code/Fork/Decompressor.c Code/Commons/Codec.c \
	Code/Commons/HuffmanTree.c Code/Commons/MD5Utils.c \
	Code/Commons/FileList.c -lcrypto
./huffman-fork c Eliminar/gutenberg_txt
mkdir -p Eliminar/gutenberg_txt_descomprimido
./huffman-fork d Eliminar/gutenberg_txt Eliminar/gutenberg_txt_descomprimido
```

Con un directorio, los archivos se descomprimen en paralelo y se verifica el MD5
de cada resultado.

## Variante concurrente con pthread

La implementación con hilos está en `Code/Pthread`. Usa una región de memoria
compartida creada con `mmap` para mantener la cola de trabajos y el estado
global, protegidos por un mutex de `pthread`.

```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic \
	-I Code/Commons -o huffman-pthread Code/Pthread/main.c \
	Code/Pthread/Compressor.c Code/Pthread/Decompressor.c \
	Code/Commons/Codec.c Code/Commons/HuffmanTree.c \
	Code/Commons/MD5Utils.c Code/Commons/FileList.c -lcrypto -pthread
./huffman-pthread c Eliminar/gutenberg_txt
mkdir -p Eliminar/gutenberg_txt_pthread
./huffman-pthread d Eliminar/gutenberg_txt Eliminar/gutenberg_txt_pthread
```

La compresión y la descompresión procesan todos los archivos compatibles del
directorio usando los hilos disponibles, con un máximo de 64 trabajadores.

#