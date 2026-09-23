from pathlib import Path


def eliminar_archivos_huff():
	carpeta = Path(__file__).parent / "gutenberg_txt"
	archivos_eliminados = 0

	for archivo in carpeta.glob("*.huff"):
		if archivo.is_file():
			archivo.unlink()
			archivos_eliminados += 1

	print(f"Archivos .huff eliminados: {archivos_eliminados}")


if __name__ == "__main__":
	eliminar_archivos_huff()
