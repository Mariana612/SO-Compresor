# SO-Compresor

## Compresor Huffman + MD5

El programa comprime todos los archivos regulares que están directamente dentro
de un directorio  y los guarda en **un solo archivo `.huff`**. Por cada archivo se calcula su firma MD5 antes de
comprimirlo y se guarda en la tabla de metadatos del `.huff`, junto con su nombre,
tamaños y tabla de frecuencias Huffman. Al descomprimir, se expande el `.huff` en
el directorio indicado, se recalcula el MD5 de cada archivo y se compara con el
guardado. MD5 detecta cambios accidentales; no es un mecanismo de seguridad
criptográfica.

## Requisitos

Se requiere las bibliotecas POSIX de Linux y
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
