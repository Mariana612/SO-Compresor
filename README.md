# SO-Compresor

## Compresor Huffman + MD5

`HuffmanCode.c` comprime todos los archivos `.txt` que estén directamente
dentro de un directorio. El archivo `.huff` guarda, para cada documento, su
nombre, tamaño, tabla de frecuencias Huffman, bits comprimidos y firma MD5 del
contenido original. MD5 detecta cambios accidentales; no es un mecanismo de
seguridad criptográfica.

## Requisitos

Se requiere un compilador C11 (`gcc`), las bibliotecas POSIX de Linux y
OpenSSL `libcrypto` para generar MD5 mediante su biblioteca, sin implementar el
algoritmo manualmente. No es necesario crear un usuario nuevo.

En este entorno se instaló el paquete del sistema `build-essential`, que incluye
`gcc`, con:

```bash
sudo apt-get install build-essential
```

Si `gcc` ya está instalado, este paso no es necesario.

El paquete `libssl-dev`, que proporciona los headers y la biblioteca `libcrypto`,
se instala con:

```bash
sudo apt-get install libssl-dev
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

Esta versión implementa compresión y almacenamiento de MD5; todavía no incluye
un comando de descompresión.

#