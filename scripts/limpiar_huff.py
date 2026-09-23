from pathlib import Path


def eliminar_archivos_huff():
	datos = Path(__file__).resolve().parent.parent / "data"
	carpeta = datos / "gutenberg"
	archivos_eliminados = 0

	# .huff sueltos dentro de la carpeta (formato viejo) y el .huff del directorio
	archivos = list(carpeta.glob("*.huff")) + [datos / "gutenberg.huff"]
	for archivo in archivos:
		if archivo.is_file():
			archivo.unlink()
			archivos_eliminados += 1

	print(f"Archivos .huff eliminados: {archivos_eliminados}")


if __name__ == "__main__":
	eliminar_archivos_huff()
