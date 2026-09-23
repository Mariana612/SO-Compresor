# Descargar el top 100 de Gutenberg

El script consulta el ranking de libros más descargados de los últimos 30 días y descarga únicamente los que ofrecen formato `Plain Text UTF-8`.

```bash
python3 scripts/descargar_gutenberg.py
```

Los archivos se guardan en `data/gutenberg/`. Para elegir otro directorio o descargar una cantidad menor:

```bash
python3 scripts/descargar_gutenberg.py --output libros --limit 20
```

Para borrar los `.huff` generados dentro de `data/` (incluido `data/gutenberg.huff`):

```bash
python3 scripts/limpiar_huff.py
```
