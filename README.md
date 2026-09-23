# SO-Compresor

## Compresor Huffman + MD5

`HuffmanCode.c` comprime todos los archivos `.txt` que estén directamente
dentro de un directorio. El archivo `.huff` guarda, para cada documento, su
nombre, tamaño, tabla de frecuencias Huffman, bits comprimidos y firma MD5 del
contenido original. MD5 detecta cambios accidentales; no es un mecanismo de
seguridad criptográfica.
El programa comprime todos los archivos `.txt` que estén directamente dentro
de un directorio. El archivo `.huff` guarda, para cada documento, su tamaño,
tabla de frecuencias Huffman, bits comprimidos y firma MD5 del contenido
original. MD5 detecta cambios accidentales; no es un mecanismo de seguridad
criptográfica.

## Requisitos

Se requiere un compilador C11 (`gcc`), las bibliotecas POSIX de Linux y
OpenSSL `libcrypto` para generar MD5 mediante su biblioteca, sin implementar el
algoritmo manualmente. No es necesario crear un usuario nuevo.
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic \
	-o huffman-md5 Code/main.c Code/Compressor.c Code/Decompressor.c \
	Code/HuffmanTree.c Code/MD5Utils.c -lcrypto
`gcc`, con:

```bash
sudo apt-get install build-essential
./huffman-md5 c Eliminar/gutenberg_txt
./huffman-md5 d Eliminar/gutenberg_txt/pg11.txt.huff Eliminar/gutenberg_txt

Si `gcc` ya está instalado, este paso no es necesario.

El paquete `libssl-dev`, que proporciona los headers y la biblioteca `libcrypto`,
se instala con:

La descompresión verifica el MD5 y elimina el archivo generado si la
verificación falla.
```

## Compilar

Desde la raíz del proyecto:

```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic \
	-o huffman-md5 Eliminar/Code/HuffmanCode.c -lcrypto
```

## Uso

```bash
./huffman-md5 Eliminar/gutenberg_txt gutenberg.huff
```

La carpeta `Eliminar/gutenberg_txt` contiene los 100 documentos `.txt` y el
programa los procesa uno por uno. Ignora subdirectorios, enlaces simbólicos y
archivos que no terminen en `.txt`. El archivo de salida debe estar fuera del
directorio de entrada.

La implementación serial también permite descomprimir todos los archivos
`.huff` de un directorio y verifica el MD5 de cada resultado:

```bash
./huffman-serial d <directorio_huff> <directorio_salida>
```

## Variante paralela con fork

La implementación paralela está en `Code/Fork`. El padre crea hijos para
procesar archivos independientes y recibe el resultado de cada hijo mediante
una pipe. La cantidad máxima de hijos se ajusta al número de procesadores
disponibles, con un límite de 64.

```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic \
	-I Code/Fork -o huffman-fork Code/Fork/main.c Code/Fork/Compressor.c \
	Code/Fork/Decompressor.c Code/Fork/HuffmanTree.c Code/Fork/MD5Utils.c \
	-lcrypto
./huffman-fork c Eliminar/gutenberg_txt
mkdir -p Eliminar/gutenberg_txt_descomprimido
./huffman-fork d Eliminar/gutenberg_txt Eliminar/gutenberg_txt_descomprimido
```

En esta variante, `d` recibe un directorio de archivos `.huff`; los archivos se
descomprimen en paralelo y se verifica el MD5 de cada resultado.

## Variante concurrente con pthread

La implementación con hilos está en `Code/Pthread`. Usa una región de memoria
compartida creada con `mmap` para mantener la cola de trabajos y el estado
global, protegidos por un mutex de `pthread`.

```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic \
	-I Code/Pthread -o huffman-pthread Code/Pthread/main.c \
	Code/Pthread/Compressor.c Code/Pthread/Decompressor.c \
	Code/Pthread/HuffmanTree.c Code/Pthread/MD5Utils.c -lcrypto -pthread
./huffman-pthread c Eliminar/gutenberg_txt
mkdir -p Eliminar/gutenberg_txt_pthread
./huffman-pthread d Eliminar/gutenberg_txt Eliminar/gutenberg_txt_pthread
```

La compresión y la descompresión procesan todos los archivos compatibles del
directorio usando los hilos disponibles, con un máximo de 64 trabajadores.

#