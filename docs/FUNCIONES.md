# Referencia de funciones

Este documento describe **todas las funciones** de los archivos `.c` y `.h` del
proyecto, módulo por módulo: su firma, qué hace, sus parámetros, qué devuelve y
los detalles de implementación que importan para entender o modificar el código.

Convención general del proyecto: las funciones que pueden fallar devuelven
**`1` si salió bien y `0` si falló**, y los mensajes de error se escriben en
`stderr`. Lo único que va a `stdout` es la línea de estadísticas (ver
[`stats_print`](#stats_print)).

Las funciones marcadas como **`static`** son privadas de su archivo `.c` (no
aparecen en el `.h`).

## Índice

- [Visión general del flujo](#visión-general-del-flujo)
- [src/common — código compartido](#srccommon--código-compartido)
  - [HuffmanTree.h / HuffmanTree.c](#huffmantreeh--huffmantreec)
  - [MD5Utils.h / MD5Utils.c](#md5utilsh--md5utilsc)
  - [FileList.h / FileList.c](#filelisth--filelistc)
  - [Codec.h / Codec.c](#codech--codecc)
  - [Stats.h / Stats.c](#statsh--statsc)
  - [Cli.h / Cli.c](#clih--clic)
- [src/serial — variante serial](#srcserial--variante-serial)
- [src/fork — variante con procesos](#srcfork--variante-con-procesos)
  - [ProcessPool.h / ProcessPool.c](#processpoolh--processpoolc)
  - [Compressor.h / Compressor.c (fork)](#compressorh--compressorc-fork)
  - [Decompressor.h / Decompressor.c (fork)](#decompressorh--decompressorc-fork)
  - [main.c (fork)](#mainc-fork)
- [src/pthread — variante con hilos](#srcpthread--variante-con-hilos)
  - [ThreadPool.h / ThreadPool.c](#threadpoolh--threadpoolc)
  - [Compressor.h / Compressor.c (pthread)](#compressorh--compressorc-pthread)
  - [Decompressor.h / Decompressor.c (pthread)](#decompressorh--decompressorc-pthread)
  - [main.c (pthread)](#mainc-pthread)
- [src/gui — interfaz gráfica](#srcgui--interfaz-gráfica)

---

## Visión general del flujo

Las tres variantes (`serial`, `fork`, `pthread`) comparten el mismo `main`
mínimo, que delega todo en [`cli_run`](#cli_run) pasándole sus propias
funciones `compress_directory` y `decompress_archive`.

**Compresión** (dos fases):

1. `file_list_load` lista los archivos regulares del directorio (sin `.huff`).
2. **Fase 1 — análisis:** `codec_analyze_file` por cada archivo (MD5,
   frecuencias, tamaño comprimido exacto). Se puede paralelizar.
3. `codec_total_size`, `codec_assign_offsets` y `codec_write_header` escriben la
   cabecera del `.huff` con la tabla completa.
4. **Fase 2 — codificación:** `codec_encode_file` por cada archivo, escribiendo
   en su propia región del `.huff`. Se puede paralelizar porque las regiones no
   se solapan.

**Descompresión:**

1. `codec_read_header` lee y valida la tabla.
2. `codec_decode_entry` por cada entrada (se puede paralelizar): reconstruye el
   árbol, decodifica y verifica el MD5.
3. Se cuentan las firmas verificadas para calcular la *salud*.

---

## src/common — código compartido

### HuffmanTree.h / HuffmanTree.c

Construcción del árbol de Huffman y generación de los códigos binarios.

#### Constantes y tipos (HuffmanTree.h)

| Nombre | Descripción |
|---|---|
| `HUFFMAN_SYMBOLS` (256) | Cantidad de símbolos posibles: un byte vale de 0 a 255. |
| `HUFFMAN_MAX_CODE_LENGTH` (256) | Largo máximo de un código. Con 256 símbolos el árbol tiene como mucho 255 niveles. |
| `HuffmanNode` | Nodo del árbol: `symbol` (solo válido en hojas), `frequency` (apariciones del símbolo o suma de los hijos), `left` (bit 0) y `right` (bit 1). |
| `HuffmanCode` | Código de un símbolo: `bits[0..length-1]` con valores 0/1 y su `length`. |

#### `is_leaf`

```c
int is_leaf(const HuffmanNode *node);
```

[HuffmanTree.c:8](../src/common/HuffmanTree.c#L8)

Indica si un nodo es hoja.

- **Parámetros:** `node` — nodo a revisar (puede ser `NULL`).
- **Devuelve:** `1` si `node` no es `NULL` y no tiene hijos; `0` en otro caso.
- **Notas:** es pública porque `Codec.c` la usa al decodificar para saber cuándo
  llegó a un símbolo.

#### `clean_tree_exception` *(static)*

```c
static void clean_tree_exception(HuffmanNode *pending[], int count);
```

[HuffmanTree.c:14](../src/common/HuffmanTree.c#L14)

Libera todos los subárboles que todavía están en la lista de pendientes. Se usa
solo en las rutas de error de `huffman_build_tree` (cuando falla un `malloc`)
para no perder memoria.

- **Parámetros:** `pending` — arreglo de raíces de subárboles; `count` — cuántos
  hay.

#### `new_node` *(static)*

```c
static HuffmanNode *new_node(unsigned char symbol, uint64_t frequency,
                             HuffmanNode *left, HuffmanNode *right);
```

[HuffmanTree.c:26](../src/common/HuffmanTree.c#L26)

Reserva memoria para un nodo y le asigna sus campos.

- **Parámetros:** `symbol` — byte que representa (en nodos internos se pasa 0);
  `frequency` — frecuencia; `left`/`right` — hijos (`NULL` para hojas).
- **Devuelve:** el nodo nuevo, o `NULL` si `malloc` falla.

#### `clean_tree`

```c
void clean_tree(HuffmanNode *root);
```

[HuffmanTree.c:41](../src/common/HuffmanTree.c#L41)

Libera recursivamente el árbol completo (recorrido post-orden: primero los
hijos, después el nodo).

- **Parámetros:** `root` — raíz del árbol. Acepta `NULL` (no hace nada), lo que
  permite llamarla sin revisar antes, por ejemplo con archivos vacíos.

#### `remove_smallest` *(static)*

```c
static HuffmanNode *remove_smallest(HuffmanNode *pending[], int *count);
```

[HuffmanTree.c:52](../src/common/HuffmanTree.c#L52)

Busca linealmente el nodo de menor frecuencia en `pending`, lo saca de la lista
y lo devuelve. Para sacarlo en O(1) pone en su lugar el último elemento y
decrementa `*count`.

- **Parámetros:** `pending` — lista de nodos pendientes; `count` — puntero a la
  cantidad (se modifica).
- **Devuelve:** el nodo con menor frecuencia.
- **Notas:** ante empates elige el de **menor índice**. El algoritmo es
  determinista: las mismas frecuencias producen siempre el mismo árbol. Esto es
  fundamental, porque el árbol se reconstruye tres veces (análisis, codificación
  y decodificación) a partir de la tabla de frecuencias y los códigos tienen que
  coincidir. La búsqueda es O(n) con n ≤ 256, por lo que no hace falta un heap.

#### `huffman_build_tree`

```c
HuffmanNode *huffman_build_tree(const uint64_t frequencies[HUFFMAN_SYMBOLS]);
```

[HuffmanTree.c:75](../src/common/HuffmanTree.c#L75)

Construye el árbol de Huffman a partir de la tabla de frecuencias.

1. Crea una hoja por cada símbolo con frecuencia > 0.
2. Mientras quede más de un nodo, saca los dos de menor frecuencia
   (`remove_smallest`) y los une bajo un padre cuya frecuencia es la suma. El
   primero extraído va a la izquierda (bit 0) y el segundo a la derecha (bit 1).
3. El nodo que queda es la raíz.

- **Parámetros:** `frequencies` — veces que aparece cada byte.
- **Devuelve:** la raíz del árbol (hay que liberarla con `clean_tree`), o `NULL`
  si todas las frecuencias son 0 o si falla la memoria (en ese caso libera todo
  lo que alcanzó a crear).
- **Caso especial:** si solo hay un símbolo distinto, la raíz es una hoja y su
  código tiene largo 0 (ver `write_decoded_data`).

#### `save_codes` *(static)*

```c
static void save_codes(const HuffmanNode *node, unsigned char path[], int depth,
                       HuffmanCode codes[HUFFMAN_SYMBOLS]);
```

[HuffmanTree.c:119](../src/common/HuffmanTree.c#L119)

Recorre el árbol en profundidad acumulando en `path` el camino (0 = izquierda,
1 = derecha). Al llegar a una hoja copia los primeros `depth` bits del camino al
código de ese símbolo.

- **Parámetros:** `node` — nodo actual; `path` — buffer temporal con el camino;
  `depth` — profundidad actual (= largo del camino); `codes` — tabla de salida.

#### `huffman_generate_codes`

```c
void huffman_generate_codes(const HuffmanNode *root, HuffmanCode codes[HUFFMAN_SYMBOLS]);
```

[HuffmanTree.c:140](../src/common/HuffmanTree.c#L140)

Genera la tabla de códigos de todos los símbolos. Primero pone toda la tabla en
cero (los símbolos que no aparecen quedan con `length = 0`) y después llama a
`save_codes` desde la raíz.

- **Parámetros:** `root` — raíz del árbol; `codes` — arreglo de 256 códigos que
  se rellena.

---

### MD5Utils.h / MD5Utils.c

Firma MD5 de archivos usando la API EVP de OpenSSL (`libcrypto`).

#### `calculate_md5`

```c
int calculate_md5(const char *filename, unsigned char md5_out[MD5_DIGEST_LENGTH]);
```

[MD5Utils.c:9](../src/common/MD5Utils.c#L9)

Calcula el MD5 de un archivo leyéndolo por bloques de 64 KiB
(`MD5_BLOCK_SIZE`), de modo que no se carga entero en memoria. Usa un contexto
`EVP_MD_CTX`: `EVP_DigestInit_ex` → `EVP_DigestUpdate` por cada bloque →
`EVP_DigestFinal_ex`.

- **Parámetros:** `filename` — ruta del archivo; `md5_out` — buffer de 16 bytes
  donde queda la firma.
- **Devuelve:** `1` si se calculó; `0` si no se pudo abrir el archivo (imprime
  error) o crear el contexto.

#### `verify_md5`

```c
int verify_md5(const char *filename, const unsigned char expected_md5[MD5_DIGEST_LENGTH]);
```

[MD5Utils.c:42](../src/common/MD5Utils.c#L42)

Recalcula el MD5 del archivo y lo compara byte a byte (`memcmp`) con la firma
esperada.

- **Parámetros:** `filename` — archivo a verificar; `expected_md5` — firma
  guardada en el `.huff`.
- **Devuelve:** `1` si coinciden; `0` si no coinciden o no se pudo calcular.

---

### FileList.h / FileList.c

Listado de los archivos de un directorio.

#### Tipo `FileList` (FileList.h)

| Campo | Descripción |
|---|---|
| `char **paths` | Arreglo dinámico de rutas completas, ej. `"libros/pg11.txt"`. |
| `size_t count` | Cantidad de rutas. |

#### `is_huff` *(static)*

```c
static int is_huff(const char *path);
```

[FileList.c:13](../src/common/FileList.c#L13)

- **Devuelve:** `1` si `path` termina en `".huff"`; `0` en otro caso (incluido
  si el nombre tiene menos de 5 caracteres).

#### `add_path` *(static)*

```c
static int add_path(FileList *list, const char *path);
```

[FileList.c:23](../src/common/FileList.c#L23)

Agranda el arreglo `paths` en una posición con `realloc` y guarda una copia
propia de `path` (con `malloc` + `strcpy`).

- **Devuelve:** `1` si se agregó; `0` si falló la memoria.
- **Notas:** crece de a una posición por archivo (O(n²) en copias en el peor
  caso), suficiente para los ~100 archivos del proyecto.

#### `file_list_load`

```c
int file_list_load(const char *directory, int compressed, FileList *list);
```

[FileList.c:43](../src/common/FileList.c#L43)

Recorre el directorio con `opendir`/`readdir` y guarda las rutas completas de
los archivos que sirven.

- Salta `.` y `..`.
- Solo acepta archivos regulares (`S_ISREG`); se ignoran subdirectorios,
  enlaces a directorios, etc. **No es recursiva.**
- Filtro según `compressed`:
  - `0` → se **excluyen** los `.huff` (modo compresión).
  - `1` → se incluyen **solo** los `.huff`.
- **Parámetros:** `directory` — carpeta a recorrer; `compressed` — filtro
  anterior; `list` — salida (se inicializa vacía al entrar).
- **Devuelve:** `1` si se pudo leer el directorio (la lista puede quedar vacía);
  `0` si no se pudo abrir o faltó memoria (en ese caso libera la lista).
- **Notas:** el orden de los archivos es el que devuelve `readdir`, no
  alfabético.

#### `file_list_free`

```c
void file_list_free(FileList *list);
```

[FileList.c:90](../src/common/FileList.c#L90)

Libera cada ruta, después el arreglo, y deja la lista en `{NULL, 0}` para que
sea seguro volver a usarla o liberarla dos veces.

---

### Codec.h / Codec.c

Formato del archivo `.huff` (HUF3) y compresión/descompresión de cada archivo.

#### Formato HUF3

```
| "HUF3" | cantidad N | tabla de N entradas | datos comprimidos de cada archivo |
| 4 B    | 8 B        | N x ENTRY_SIZE      | resto del archivo                 |

entrada:
| nombre | tamaño original | tamaño comprimido | offset de datos | MD5  | frecuencias |
| 256 B  | 8 B             | 8 B               | 8 B             | 16 B | 256 x 8 B   |
```

Constantes internas: `BLOCK_SIZE` (64 KiB de lectura), `MAGIC` (`"HUF3"`),
`ENTRY_SIZE` (2344 bytes por entrada) y la macro `HEADER_SIZE(count)`.

Los enteros se escriben en el orden de bytes de la máquina (sin conversión a
*endianness* fija), así que un `.huff` es portable entre máquinas de la misma
arquitectura.

#### Tipo `ArchiveEntry` (Codec.h)

| Campo | Descripción |
|---|---|
| `name[ARCHIVE_NAME_MAX]` | Nombre original sin carpeta (máx. 255 caracteres + `'\0'`). |
| `original_size` | Bytes del archivo original. |
| `compressed_size` | Bytes de sus datos comprimidos. |
| `data_offset` | Posición donde empiezan sus datos dentro del `.huff`. |
| `md5[16]` | Firma MD5 del original. |
| `frequencies[256]` | Tabla de frecuencias, para reconstruir el árbol. |

#### Escritura de bits

Tipo interno `BitWriter`: `file` (destino), `current_byte` (byte que se está
armando) y `bit_count` (bits que ya tiene).

##### `bitwriter_init` *(static)*

```c
static void bitwriter_init(BitWriter *writer, FILE *file);
```

[Codec.c:51](../src/common/Codec.c#L51)

Asocia el escritor al archivo y lo deja con el byte vacío.

##### `write_bit` *(static)*

```c
static void write_bit(BitWriter *writer, int bit);
```

[Codec.c:59](../src/common/Codec.c#L59)

Agrega un bit por la derecha al byte actual (desplazando a la izquierda). Cuando
el byte junta 8 bits lo escribe con `fputc` y empieza uno nuevo. Los bits quedan
ordenados del más significativo al menos significativo.

##### `bitwriter_finish` *(static)*

```c
static void bitwriter_finish(BitWriter *writer);
```

[Codec.c:72](../src/common/Codec.c#L72)

Si quedó un byte incompleto, lo rellena con ceros a la derecha y lo escribe. Por
eso el tamaño comprimido es `ceil(bits / 8)`.

#### Lectura de bits

Tipo interno `BitReader`: `file` (origen), `current_byte` (último byte leído) y
`bits_left` (bits que faltan entregar de ese byte).

##### `bitreader_init` *(static)*

```c
static void bitreader_init(BitReader *reader, FILE *file);
```

[Codec.c:96](../src/common/Codec.c#L96)

Asocia el lector al archivo y lo deja sin bits pendientes.

##### `read_bit` *(static)*

```c
static int read_bit(BitReader *reader);
```

[Codec.c:104](../src/common/Codec.c#L104)

Entrega el siguiente bit. Si no quedan bits pendientes lee un byte nuevo con
`fgetc`. Entrega siempre el bit más significativo y desplaza el byte a la
izquierda (el orden inverso al de `write_bit`).

- **Devuelve:** `0` o `1`; `-1` si se llegó al fin del archivo.

#### Funciones auxiliares

##### `count_frequencies` *(static)*

```c
static int count_frequencies(const char *filename, uint64_t frequencies[HUFFMAN_SYMBOLS],
                             uint64_t *file_size);
```

[Codec.c:131](../src/common/Codec.c#L131)

Lee el archivo por bloques y cuenta cuántas veces aparece cada byte. De paso
suma el tamaño total.

- **Parámetros:** `filename` — archivo a leer; `frequencies` — tabla de salida
  (se pone en cero antes); `file_size` — tamaño en bytes (salida).
- **Devuelve:** `1` si se leyó; `0` si no se pudo abrir.

##### `write_entry` *(static)*

```c
static void write_entry(FILE *output, const ArchiveEntry *entry);
```

[Codec.c:155](../src/common/Codec.c#L155)

Escribe los campos de una entrada **uno por uno** con `fwrite`, en el orden del
formato. Se escribe campo por campo (y no la estructura entera) para no incluir
bytes de relleno (*padding*) del compilador.

##### `read_entry` *(static)*

```c
static int read_entry(FILE *input, ArchiveEntry *entry);
```

[Codec.c:166](../src/common/Codec.c#L166)

Lee una entrada en el mismo orden que `write_entry`. Fuerza el `'\0'` final del
nombre para que siempre sea una cadena válida, aunque el `.huff` esté dañado.

- **Devuelve:** `1` si se leyó completa; `0` si el archivo terminó antes o hubo
  error de lectura.

##### `is_safe_name` *(static)*

```c
static int is_safe_name(const char *name);
```

[Codec.c:183](../src/common/Codec.c#L183)

Medida de seguridad: evita que un `.huff` manipulado escriba fuera del
directorio de salida (*path traversal*).

- **Devuelve:** `1` si el nombre no está vacío, no contiene `/` y no es `.` ni
  `..`; `0` en otro caso.

##### `write_encoded_data` *(static)*

```c
static int write_encoded_data(const char *filename, FILE *output,
                              const HuffmanCode codes[HUFFMAN_SYMBOLS]);
```

[Codec.c:190](../src/common/Codec.c#L190)

Abre el archivo original, lo lee por bloques y por cada byte escribe los bits de
su código Huffman en `output` (a partir de la posición actual de `output`). Al
final completa el último byte con `bitwriter_finish`.

- **Devuelve:** `1` si no hubo error de escritura; `0` si no se pudo abrir el
  original o falló la escritura.

##### `write_decoded_data` *(static)*

```c
static int write_decoded_data(FILE *input, FILE *output, const HuffmanNode *root,
                              uint64_t original_size);
```

[Codec.c:217](../src/common/Codec.c#L217)

Decodifica recorriendo el árbol: parte de la raíz, avanza a la izquierda con bit
0 o a la derecha con bit 1 y, al llegar a una hoja, escribe su símbolo y vuelve
a la raíz. Se detiene al escribir exactamente `original_size` bytes, lo que
descarta los bits de relleno del último byte.

- **Caso especial:** si la raíz es una hoja (el archivo tiene un solo byte
  distinto, código de largo 0) no lee bits y escribe ese símbolo
  `original_size` veces.
- **Devuelve:** `1` si se escribieron todos los bytes; `0` si los datos
  comprimidos se acabaron antes (archivo truncado) o falló la escritura.

#### Funciones públicas — compresión

##### `codec_analyze_file`

```c
int codec_analyze_file(const char *path, ArchiveEntry *entry);
```

[Codec.c:258](../src/common/Codec.c#L258)

**Fase 1** de la compresión. Deja la entrada completa, salvo `data_offset`.

1. Toma el nombre sin la carpeta (lo que viene después del último `/`) y
   rechaza nombres de 256 caracteres o más.
2. Calcula el MD5 (`calculate_md5`).
3. Cuenta frecuencias y tamaño original (`count_frequencies`).
4. Si el archivo está vacío termina (tamaño comprimido 0).
5. Construye el árbol, genera los códigos y calcula el tamaño comprimido
   **exacto**: `Σ frecuencia[s] × largo(código[s])` bits, redondeado hacia
   arriba a bytes.

- **Devuelve:** `1` si se analizó; `0` ante cualquier error.
- **Notas:** es segura para ejecutarse en paralelo sobre archivos distintos
  (solo toca su propia `entry`). Lee el archivo dos veces (MD5 y frecuencias).

##### `codec_assign_offsets`

```c
void codec_assign_offsets(ArchiveEntry entries[], size_t count);
```

[Codec.c:297](../src/common/Codec.c#L297)

Asigna a cada entrada su `data_offset`: el primero empieza justo después de la
cabecera (`HEADER_SIZE(count)`) y cada uno sigue inmediatamente al anterior.
Como los tamaños comprimidos ya se conocen, las regiones quedan contiguas y sin
solaparse.

##### `codec_total_size`

```c
uint64_t codec_total_size(const ArchiveEntry entries[], size_t count);
```

[Codec.c:309](../src/common/Codec.c#L309)

- **Devuelve:** la suma de los `original_size` de todas las entradas (se usa
  para la estadística `original`).

##### `codec_write_header`

```c
int codec_write_header(const char *archive, const ArchiveEntry entries[], size_t count);
```

[Codec.c:320](../src/common/Codec.c#L320)

Crea (o trunca) el `.huff` y escribe el identificador `"HUF3"`, la cantidad de
archivos y la tabla completa de entradas. Debe llamarse **después** de
`codec_assign_offsets` y **antes** de `codec_encode_file`.

- **Devuelve:** `1` si se escribió y cerró bien; `0` si no se pudo crear o hubo
  error de escritura.

##### `codec_encode_file`

```c
int codec_encode_file(const char *path, const char *archive, const ArchiveEntry *entry);
```

[Codec.c:344](../src/common/Codec.c#L344)

**Fase 2** de la compresión. Escribe los datos comprimidos de un archivo en su
región del `.huff`.

1. Reconstruye el mismo árbol y los mismos códigos que en el análisis (salen de
   las mismas frecuencias).
2. Abre el `.huff` con su **propio** `FILE*` en modo `"r+b"` (sin truncar) y se
   posiciona en `entry->data_offset` con `fseeko`.
3. Codifica con `write_encoded_data`.
4. Comprueba que lo escrito mida exactamente `entry->compressed_size`. Si no
   coincide (por ejemplo, porque el archivo cambió entre las dos fases) lo
   considera un error, ya que habría invadido la región del siguiente archivo.

- **Devuelve:** `1` si salió bien (o si el archivo está vacío, caso en que no
  hace nada); `0` ante cualquier error.
- **Notas:** como cada llamada usa su propio descriptor y una región disjunta,
  varios hilos o procesos pueden ejecutarla a la vez sin sincronización.

#### Funciones públicas — descompresión

##### `codec_read_header`

```c
int codec_read_header(const char *archive, ArchiveEntry **entries, size_t *count);
```

[Codec.c:387](../src/common/Codec.c#L387)

Lee y valida la cabecera de un `.huff`:

- Comprueba el identificador `"HUF3"`.
- Comprueba que `N` entradas quepan en el tamaño real del archivo (evita
  reservar memoria enorme con un `.huff` dañado).
- Por cada entrada, comprueba que `data_offset` y
  `data_offset + compressed_size` no se salgan del archivo.

- **Parámetros:** `archive` — ruta del `.huff`; `entries` — salida, arreglo
  reservado con `malloc` (lo libera quien llama con `free`); `count` — salida,
  cantidad de entradas.
- **Devuelve:** `1` si la cabecera es válida; `0` si no (en ese caso
  `*entries = NULL` y `*count = 0`).

##### `codec_decode_entry`

```c
int codec_decode_entry(const char *archive, const ArchiveEntry *entry,
                       const char *output_directory);
```

[Codec.c:441](../src/common/Codec.c#L441)

Expande un archivo del `.huff` y verifica su firma.

1. Valida el nombre con `is_safe_name` y arma la ruta
   `output_directory/nombre`.
2. Reconstruye el árbol desde las frecuencias (si el archivo no está vacío).
3. Abre el `.huff` con su propio `FILE*`, se posiciona en `data_offset` y
   decodifica con `write_decoded_data`.
4. Verifica el MD5 del archivo generado con `verify_md5`.
5. Si algo falla, **borra** el archivo de salida para no dejar un resultado
   corrupto.

- **Devuelve:** `1` **solo si la firma MD5 coincide**; `0` en cualquier otro
  caso. El valor de retorno es lo que usan las variantes para contar los
  archivos verificados (la *salud*).
- **Notas:** segura en paralelo sobre entradas distintas.

---

### Stats.h / Stats.c

Estadísticas de una corrida.

#### Tipo `RunStats` (Stats.h)

| Campo | Descripción |
|---|---|
| `files` | Archivos del directorio (compresión) o del `.huff` (descompresión). |
| `verified` | Firmas MD5 verificadas (solo al descomprimir). |
| `original_bytes` | Suma de los tamaños originales. |
| `compressed_bytes` | Tamaño del `.huff`. |
| `seconds` | Tiempo total de la corrida. |

#### `stats_health_percent`

```c
double stats_health_percent(const RunStats *stats);
```

[Stats.c:9](../src/common/Stats.c#L9)

- **Devuelve:** `verified / files × 100`. Si no hay archivos devuelve `100.0`
  (nada falló).

#### `stats_speedup_percent`

```c
double stats_speedup_percent(double serial_seconds, double seconds);
```

[Stats.c:17](../src/common/Stats.c#L17)

Aceleración respecto a la versión serial: `(Ts / T − 1) × 100`. Ejemplo: serial
2 s y paralela 1 s → 100 % (el doble de rápido); 0 % significa igual de rápido y
un valor negativo, más lento.

- **Devuelve:** el porcentaje, o `0.0` si algún tiempo es ≤ 0.
- **Notas:** no se usa dentro de los binarios de consola (cada uno mide solo su
  propia corrida); está disponible para quien compare dos corridas, como la
  interfaz.

#### `stats_print`

```c
void stats_print(const char *variant, char mode, const RunStats *stats);
```

[Stats.c:27](../src/common/Stats.c#L27)

Imprime en `stdout` **una sola línea** `clave=valor` y hace `fflush`:

```
variante=fork operacion=c archivos=100 original=82287268 comprimido=48232800 tiempo=0.311995
variante=fork operacion=d archivos=100 verificados=100 salud=100.00 original=... comprimido=... tiempo=...
```

- **Parámetros:** `variant` — `"serial"`, `"fork"` o `"pthread"`; `mode` — `'c'`
  (compresión) o `'d'` (descompresión; agrega `verificados` y `salud`);
  `stats` — datos de la corrida.

---

### Cli.h / Cli.c

Punto de entrada común a las tres variantes: interpreta argumentos, mide el
tiempo e imprime estadísticas.

#### Tipos (Cli.h)

```c
typedef int (*CompressFunction)(const char *directory, const char *archive, RunStats *stats);
typedef int (*DecompressFunction)(const char *archive, const char *output_directory, RunStats *stats);
```

Punteros a las funciones de cada variante. Deben devolver `1` si la corrida se
pudo hacer y rellenar en `stats` la cantidad de archivos, los tamaños originales
y, al descomprimir, cuántos se verificaron.

#### `now_seconds` *(static)*

```c
static double now_seconds(void);
```

[Cli.c:14](../src/common/Cli.c#L14)

- **Devuelve:** segundos (con decimales) según `CLOCK_MONOTONIC`. Se usa un
  reloj monotónico porque no se ve afectado por cambios de la hora del sistema;
  solo sirve para medir diferencias.

#### `default_archive_name` *(static)*

```c
static void default_archive_name(const char *directory, char archive[PATH_MAX]);
```

[Cli.c:23](../src/common/Cli.c#L23)

Genera el nombre por defecto del `.huff`: quita las `/` finales del directorio y
agrega `.huff` (`"libros/"` → `"libros.huff"`, al lado de la carpeta).

#### `prepare_output_directory` *(static)*

```c
static int prepare_output_directory(const char *directory);
```

[Cli.c:33](../src/common/Cli.c#L33)

Crea el directorio de salida con permisos `0755` si no existe (no crea
directorios intermedios).

- **Devuelve:** `1` si al final existe y es un directorio; `0` en otro caso.

#### `usage` *(static)*

```c
static int usage(const char *program);
```

[Cli.c:42](../src/common/Cli.c#L42)

Imprime en `stderr` la forma de uso del programa.

- **Devuelve:** siempre `EXIT_FAILURE`, para poder hacer `return usage(...)`.

#### `archive_size` *(static)*

```c
static uint64_t archive_size(const char *archive);
```

[Cli.c:52](../src/common/Cli.c#L52)

- **Devuelve:** el tamaño en bytes del `.huff` (con `stat`), o `0` si no existe.

#### `cli_run`

```c
int cli_run(int argc, char *argv[], const char *variant,
            CompressFunction compress, DecompressFunction decompress);
```

[Cli.c:64](../src/common/Cli.c#L64)

Lógica completa de la línea de comandos.

**Modo compresión** — `programa c <directorio> [archivo.huff]`:

1. Verifica que `<directorio>` exista y sea un directorio.
2. Usa el `.huff` indicado o el nombre por defecto.
3. Mide el tiempo de `compress(...)`.
4. Completa `compressed_bytes` con el tamaño real del `.huff` e imprime las
   estadísticas.

**Modo descompresión** — `programa d <archivo.huff> <directorio_salida>`:

1. Verifica que el `.huff` sea un archivo regular.
2. Prepara el directorio de salida.
3. Mide el tiempo de `decompress(...)`.
4. Imprime las estadísticas **aunque falle alguna firma**, para que la salud
   muestre cuántas se verificaron.

- **Parámetros:** `argc`/`argv` — los de `main`; `variant` — nombre de la
  variante para las estadísticas; `compress`/`decompress` — implementación de
  la variante.
- **Devuelve:** `EXIT_SUCCESS` si todo salió bien; `EXIT_FAILURE` si los
  argumentos son inválidos, la operación falló o, al descomprimir, **alguna
  firma MD5 no se verificó**.

---

## src/serial — variante serial

Procesa los archivos uno por uno, en el hilo principal. Sirve como referencia
para medir la aceleración de las otras dos.

### `compress_directory` (serial)

```c
int compress_directory(const char *directory, const char *archive, RunStats *stats);
```

Declarada en [serial/Compressor.h](../src/serial/Compressor.h), implementada en
[serial/Compressor.c:7](../src/serial/Compressor.c#L7).

1. Lista los archivos (`file_list_load` con `compressed = 0`).
2. Reserva la tabla de entradas con `calloc` (al menos 1 elemento, para no
   pedir 0 bytes).
3. **Fase 1:** `codec_analyze_file` para cada archivo, en orden; se detiene en
   el primer error.
4. Rellena `stats->files` y `stats->original_bytes`, asigna offsets y escribe
   la cabecera.
5. **Fase 2:** `codec_encode_file` para cada archivo; se detiene en el primer
   error.
6. Libera la tabla y la lista.

- **Devuelve:** `1` si todos los archivos se comprimieron; `0` si alguno falló.

### `decompress_archive` (serial)

```c
int decompress_archive(const char *archive, const char *output_directory, RunStats *stats);
```

Declarada en [serial/Decompressor.h](../src/serial/Decompressor.h),
implementada en [serial/Decompressor.c:6](../src/serial/Decompressor.c#L6).

Lee la cabecera, rellena `files` y `original_bytes` y descomprime **todas** las
entradas aunque alguna falle, sumando en `stats->verified` las que pasaron la
verificación MD5.

- **Devuelve:** `0` solo si la cabecera no es válida; si no, `1` (los fallos
  individuales se reflejan en `verified`).

### `main` (serial)

[serial/main.c:5](../src/serial/main.c#L5)

```c
return cli_run(argc, argv, "serial", compress_directory, decompress_archive);
```

---

## src/fork — variante con procesos

Reparte el trabajo entre procesos hijos creados con `fork()`. La comunicación
hijo → padre se hace con **una pipe por hijo**.

### ProcessPool.h / ProcessPool.c

Pool genérico de procesos: ejecuta una tarea por índice, cada una en un hijo.

#### Tipo `ProcessTask` (ProcessPool.h)

```c
typedef int (*ProcessTask)(size_t index, void *context, void *result);
```

Trabajo que ejecuta un hijo sobre el elemento `index`. `context` son los datos
compartidos (el hijo tiene una **copia** heredada del `fork`); `result` apunta a
un buffer de `result_size` bytes que el hijo rellena y que se le envía al padre
por la pipe (puede ser `NULL`). Devuelve `1` si salió bien.

#### Tipo interno `Child`

| Campo | Descripción |
|---|---|
| `pid` | PID del hijo, para reconocerlo al terminar. |
| `pipe_read` | Extremo de lectura de su pipe (lo usa el padre). |
| `index` | Elemento que está procesando, para saber dónde guardar su resultado. |

`MAX_CHILDREN` (64) limita la cantidad de hijos simultáneos.

#### `get_children_limit` *(static)*

```c
static int get_children_limit(void);
```

[ProcessPool.c:30](../src/fork/ProcessPool.c#L30)

- **Devuelve:** la cantidad de núcleos en línea
  (`sysconf(_SC_NPROCESSORS_ONLN)`), acotada entre 1 y `MAX_CHILDREN`.

#### `write_full` *(static)*

```c
static int write_full(int fd, const void *buffer, size_t size);
```

[ProcessPool.c:42](../src/fork/ProcessPool.c#L42)

Escribe exactamente `size` bytes en `fd`, repitiendo `write()` porque este puede
escribir menos de lo pedido. Reintenta si una señal interrumpe la llamada
(`EINTR`).

- **Devuelve:** `1` si escribió todo; `0` si hubo error.

#### `read_full` *(static)*

```c
static int read_full(int fd, void *buffer, size_t size);
```

[ProcessPool.c:60](../src/fork/ProcessPool.c#L60)

Lee exactamente `size` bytes de `fd`, repitiendo `read()` y reintentando ante
`EINTR`.

- **Devuelve:** `1` si leyó todo; `0` si hubo error o se llegó al fin de la
  pipe antes (el hijo terminó sin mandar todo, por ejemplo porque se cayó).

#### `start_child` *(static)*

```c
static int start_child(Child children[], int *active, size_t index, ProcessTask task,
                       void *context, void *results, size_t result_size);
```

[ProcessPool.c:79](../src/fork/ProcessPool.c#L79)

Crea una pipe y un proceso hijo para el elemento `index`.

- Antes del `fork` hace `fflush(stdout)` para que el hijo no duplique salida
  pendiente del padre.
- **En el hijo:** cierra el extremo de lectura de su pipe y los de lectura de
  todos sus hermanos (heredados), ejecuta `task`, y envía por la pipe primero un
  `int` (1 = bien, 0 = error) y después los `result_size` bytes del resultado.
  Termina con `_exit` (no `exit`) para no ejecutar los `atexit` ni vaciar
  buffers heredados del padre.
- **En el padre:** cierra el extremo de escritura y agrega el hijo a
  `children`, incrementando `*active`.

- **Devuelve (en el padre):** `1` si se creó el hijo; `0` si fallaron `pipe` o
  `fork`.

#### `wait_for_child` *(static)*

```c
static int wait_for_child(Child children[], int *active, void *results, size_t result_size);
```

[ProcessPool.c:131](../src/fork/ProcessPool.c#L131)

Espera al **primer hijo que termine** (`waitpid(-1, ...)`), lo busca en la
lista, lee de su pipe el `int` de estado y, si fue exitoso, el resultado en
`results + index * result_size`. Cierra la pipe y lo saca de la lista poniendo
el último en su lugar.

- **Devuelve:** `1` si el hijo reportó éxito y se leyó todo; `0` en otro caso.
- **Notas:** leer después de `waitpid` es seguro porque el resultado cabe en el
  buffer de la pipe (lo garantiza `process_pool_run`), así que el hijo pudo
  escribirlo y terminar sin quedar bloqueado. Si `waitpid` devuelve un PID
  desconocido, cierra todas las pipes y vacía la lista.

#### `process_pool_run`

```c
size_t process_pool_run(size_t count, ProcessTask task, void *context,
                        void *results, size_t result_size);
```

[ProcessPool.c:178](../src/fork/ProcessPool.c#L178)

Ejecuta `task` para los índices `0..count-1`, cada uno en un hijo nuevo, con a
lo sumo un hijo por núcleo trabajando a la vez.

- Verifica primero que `sizeof(int) + result_size ≤ PIPE_BUF`; si no, aborta
  (el hijo no podría escribir todo antes de que el padre lea).
- Bucle: mientras haya lugar y queden índices, lanza hijos; cuando no hay
  lugar, espera a cualquiera que termine. Así un archivo grande no frena el
  lanzamiento de los siguientes.
- Si no se puede crear un hijo y no hay ninguno activo, aborta; los índices sin
  repartir cuentan como fallidos.

- **Parámetros:** `results` — arreglo de `count × result_size` bytes donde
  quedan los resultados (puede ser `NULL` si `result_size` es 0).
- **Devuelve:** cuántos hijos terminaron bien (`count` si no falló ninguno).

### Compressor.h / Compressor.c (fork)

Tipo interno `CompressJob`: `paths` (rutas), `archive` (ruta del `.huff`) y
`entries` (tabla). Cada hijo recibe una copia al hacer `fork()`.

#### `analyze_task` *(static)*

```c
static int analyze_task(size_t index, void *context, void *result);
```

[fork/Compressor.c:25](../src/fork/Compressor.c#L25)

Tarea de la fase 1: el hijo ejecuta `codec_analyze_file` escribiendo la entrada
directamente en `result`. El pool luego envía esa `ArchiveEntry` completa al
padre por la pipe. Es necesario porque la memoria del hijo es una copia: sin la
pipe, el padre nunca vería los metadatos.

#### `encode_task` *(static)*

```c
static int encode_task(size_t index, void *context, void *result);
```

[fork/Compressor.c:32](../src/fork/Compressor.c#L32)

Tarea de la fase 2: el hijo ejecuta `codec_encode_file` con la entrada ya
completada (con offset), heredada del padre. No devuelve datos (solo el estado
por la pipe).

#### `compress_directory` (fork)

```c
int compress_directory(const char *directory, const char *archive, RunStats *stats);
```

[fork/Compressor.c:45](../src/fork/Compressor.c#L45)

1. Lista los archivos y reserva la tabla de entradas.
2. **Fase 1:** `process_pool_run` con `analyze_task`, recibiendo cada
   `ArchiveEntry` por pipe en `job.entries`.
3. El padre calcula estadísticas, asigna offsets y escribe la cabecera.
4. **Fase 2:** `process_pool_run` con `encode_task`. Los hijos se crean
   **después** de asignar los offsets, así que heredan la tabla actualizada.

- **Devuelve:** `1` solo si todos los hijos de ambas fases terminaron bien.

### Decompressor.h / Decompressor.c (fork)

Tipo interno `DecompressJob`: `archive`, `output_directory` y `entries`.

#### `decode_task` *(static)*

```c
static int decode_task(size_t index, void *context, void *result);
```

[fork/Decompressor.c:24](../src/fork/Decompressor.c#L24)

El hijo ejecuta `codec_decode_entry` sobre su entrada. El resultado (1 =
verificado, 0 = error) le llega al padre por la pipe como el `int` de estado.

#### `decompress_archive` (fork)

```c
int decompress_archive(const char *archive, const char *output_directory, RunStats *stats);
```

[fork/Decompressor.c:37](../src/fork/Decompressor.c#L37)

Lee la cabecera, rellena estadísticas y reparte las entradas entre hijos con
`process_pool_run`. El número de hijos exitosos es directamente la cantidad de
firmas verificadas (`stats->verified`).

- **Devuelve:** `0` solo si la cabecera no es válida; si no, `1`.

### main.c (fork)

[fork/main.c:5](../src/fork/main.c#L5)

```c
return cli_run(argc, argv, "fork", compress_directory, decompress_archive);
```

---

## src/pthread — variante con hilos

Reparte el trabajo entre hilos POSIX que toman índices de una cola compartida
protegida con un mutex.

### ThreadPool.h / ThreadPool.c

#### Tipo `ThreadTask` (ThreadPool.h)

```c
typedef int (*ThreadTask)(size_t index, void *context);
```

Trabajo de un hilo sobre el elemento `index`. A diferencia de `ProcessTask`, no
necesita buffer de resultado: los hilos comparten memoria y pueden escribir
directamente en `context`. Devuelve `1` si salió bien.

#### Tipo interno `SharedData`

Cola de trabajo compartida por todos los hilos, ubicada en memoria creada con
`mmap`.

| Campo | Descripción |
|---|---|
| `mutex` | Protege `next_index` y `succeeded`. |
| `next_index` | Siguiente índice por repartir. |
| `total` | Cantidad de elementos. |
| `succeeded` | Elementos que salieron bien. |
| `task`, `context` | Trabajo a ejecutar y sus datos. |

`MAX_THREADS` (64) limita la cantidad de hilos.

#### `get_thread_count` *(static)*

```c
static int get_thread_count(void);
```

[ThreadPool.c:33](../src/pthread/ThreadPool.c#L33)

- **Devuelve:** la cantidad de núcleos en línea, acotada entre 1 y
  `MAX_THREADS`.

#### `take_next` *(static)*

```c
static int take_next(SharedData *shared, size_t *index);
```

[ThreadPool.c:45](../src/pthread/ThreadPool.c#L45)

Con el mutex tomado, saca el siguiente índice pendiente de la cola.

- **Devuelve:** `1` y el índice en `*index` si quedaba alguno; `0` si la cola
  está vacía.

#### `mark_success` *(static)*

```c
static void mark_success(SharedData *shared);
```

[ThreadPool.c:61](../src/pthread/ThreadPool.c#L61)

Incrementa `succeeded` con el mutex tomado, para que dos hilos no pierdan un
incremento (condición de carrera).

#### `worker` *(static)*

```c
static void *worker(void *arg);
```

[ThreadPool.c:69](../src/pthread/ThreadPool.c#L69)

Función que ejecuta cada hilo: en bucle toma un índice con `take_next`, ejecuta
la tarea y, si sale bien, llama a `mark_success`. Termina cuando la cola se
vacía. Este reparto dinámico equilibra la carga: un hilo que termina rápido toma
más trabajo.

- **Devuelve:** `NULL`.

#### `shared_memory_create`

```c
void *shared_memory_create(size_t size);
```

[ThreadPool.c:87](../src/pthread/ThreadPool.c#L87)

Crea una región de memoria anónima y compartida con
`mmap(..., MAP_SHARED | MAP_ANONYMOUS, ...)`, con permisos de lectura y
escritura. Si `size` es 0 pide 1 byte (mmap no acepta tamaño 0). La memoria
viene inicializada en cero.

- **Devuelve:** la dirección de la región, o `NULL` si falla.

#### `shared_memory_destroy`

```c
void shared_memory_destroy(void *memory, size_t size);
```

[ThreadPool.c:100](../src/pthread/ThreadPool.c#L100)

Libera la región con `munmap`. Debe recibir el mismo `size` que se usó al
crearla (aplica la misma regla de 0 → 1). Acepta `NULL`.

#### `thread_pool_run`

```c
size_t thread_pool_run(size_t count, ThreadTask task, void *context);
```

[ThreadPool.c:107](../src/pthread/ThreadPool.c#L107)

1. Crea la cola `SharedData` en memoria compartida e inicializa el mutex.
2. Crea un hilo por núcleo, pero nunca más hilos que elementos. Si falla un
   `pthread_create`, sigue con los que se alcanzaron a crear.
3. Espera a todos con `pthread_join`.
4. Lee `succeeded`, destruye el mutex y libera la cola.

- **Devuelve:** cuántos elementos salieron bien (`count` si no falló ninguno).
  Si no se creó ningún hilo devuelve `0`.

### Compressor.h / Compressor.c (pthread)

Tipo interno `CompressJob`: `paths`, `archive` y `entries`. La tabla `entries`
vive en memoria compartida y cada hilo escribe solo en su propia posición, por
lo que no hace falta mutex para ella.

#### `analyze_task` *(static)*

```c
static int analyze_task(size_t index, void *context);
```

[pthread/Compressor.c:24](../src/pthread/Compressor.c#L24)

Fase 1: `codec_analyze_file` escribiendo directamente en
`job->entries[index]`.

#### `encode_task` *(static)*

```c
static int encode_task(size_t index, void *context);
```

[pthread/Compressor.c:31](../src/pthread/Compressor.c#L31)

Fase 2: `codec_encode_file` sobre la región del archivo `index`.

#### `compress_directory` (pthread)

```c
int compress_directory(const char *directory, const char *archive, RunStats *stats);
```

[pthread/Compressor.c:43](../src/pthread/Compressor.c#L43)

Igual que la versión fork, pero la tabla se crea con `shared_memory_create` y
cada fase se ejecuta con `thread_pool_run`. Entre las dos fases, el hilo
principal calcula las estadísticas, asigna los offsets y escribe la cabecera.

- **Devuelve:** `1` solo si todos los archivos salieron bien en ambas fases.

### Decompressor.h / Decompressor.c (pthread)

Tipo interno `DecompressJob`: `archive`, `output_directory` y `entries`. La
tabla solo se lee, así que no necesita mutex.

#### `decode_task` *(static)*

```c
static int decode_task(size_t index, void *context);
```

[pthread/Decompressor.c:23](../src/pthread/Decompressor.c#L23)

Ejecuta `codec_decode_entry` sobre la entrada `index`.

#### `decompress_archive` (pthread)

```c
int decompress_archive(const char *archive, const char *output_directory, RunStats *stats);
```

[pthread/Decompressor.c:35](../src/pthread/Decompressor.c#L35)

Lee la cabecera, rellena estadísticas y reparte las entradas entre hilos. El
contador `succeeded` del pool es la cantidad de firmas verificadas.

- **Devuelve:** `0` solo si la cabecera no es válida; si no, `1`.

### main.c (pthread)

[pthread/main.c:5](../src/pthread/main.c#L5)

```c
return cli_run(argc, argv, "pthread", compress_directory, decompress_archive);
```

---

## src/gui — interfaz gráfica

Interfaz en GTK 4 con tres pestañas: Compresión, Descompresión y Estadísticas.

> **Estado actual:** `interfaz.c` todavía incluye `../Code/Compressor.h` y
> `../Code/Decompressor.h` (carpeta que ya no existe tras la reorganización) y
> llama a `compress_directory(path)` y `decompress_file(entrada, salida)` con
> firmas antiguas. Por eso `make gui` no compila con el código actual. Además,
> solo ejecuta la versión serial; los tiempos, tamaños y la pestaña de
> Estadísticas son marcadores (`"--"`, `"Pendiente"`). La forma prevista de
> integrarla es ejecutar los binarios `bin/huffman-*` y leer la línea de
> [`stats_print`](#stats_print).

### Tipos internos (interfaz.c)

| Tipo | Descripción |
|---|---|
| `CompressionPage` | Widgets de la pestaña Compresión: etiqueta y ruta del directorio elegido, barra de progreso, etiqueta de estado y una etiqueta por variante (serial/procesos/hilos) para tiempo, aceleración, tamaño original, tamaño comprimido y ratio. |
| `DecompressionPage` | Widgets de la pestaña Descompresión: `.huff` elegido, directorio de destino, barra, estado y etiquetas por variante de tiempo, aceleración y salud. |
| `AppWidgets` | Agrupa las dos páginas. Se reserva con `g_new0` y vive toda la ejecución. |

### Funciones de apoyo

#### `create_title` *(static)*

```c
static GtkWidget *create_title(const char *text);
```

[interfaz.c:65](../src/gui/interfaz.c#L65)

Crea una etiqueta en negrita y tamaño 18 pt (con *markup* Pango), alineada a la
izquierda.

- **Notas:** `text` se inserta en el markup sin escapar; no debe contener
  caracteres como `<` o `&`.

#### `create_subtitle` *(static)*

```c
static GtkWidget *create_subtitle(const char *text);
```

[interfaz.c:80](../src/gui/interfaz.c#L80)

Crea una etiqueta de texto normal alineada a la izquierda.

### Selección de rutas (diálogos)

Los diálogos de `GtkFileDialog` son asíncronos: una función `on_..._clicked`
abre el diálogo y GTK llama después a la función `..._selected` con el
resultado.

#### `compression_folder_selected` *(static)*

```c
static void compression_folder_selected(GObject *source_object, GAsyncResult *result,
                                        gpointer user_data);
```

[interfaz.c:89](../src/gui/interfaz.c#L89)

Callback del diálogo de carpeta de compresión. Si el usuario eligió una carpeta,
guarda su ruta en `page->selected_path` (liberando la anterior) y la muestra en
la etiqueta. Si canceló, libera el `GError` y no cambia nada.

#### `on_compression_search_clicked` *(static)*

```c
static void on_compression_search_clicked(GtkButton *button, gpointer user_data);
```

[interfaz.c:115](../src/gui/interfaz.c#L115)

Botón "Buscar": abre un diálogo de selección de carpeta sobre la ventana
principal.

#### `decompression_file_selected` *(static)*

```c
static void decompression_file_selected(GObject *source_object, GAsyncResult *result,
                                        gpointer user_data);
```

[interfaz.c:129](../src/gui/interfaz.c#L129)

Callback del diálogo de archivo: guarda la ruta del `.huff` elegido en
`page->selected_path` y la muestra.

#### `on_decompression_search_clicked` *(static)*

```c
static void on_decompression_search_clicked(GtkButton *button, gpointer user_data);
```

[interfaz.c:155](../src/gui/interfaz.c#L155)

Botón "Buscar archivo": abre un diálogo para elegir el `.huff` (sin filtro de
extensión).

#### `decompression_output_folder_selected` *(static)*

```c
static void decompression_output_folder_selected(GObject *source_object,
                                                 GAsyncResult *result, gpointer user_data);
```

[interfaz.c:168](../src/gui/interfaz.c#L168)

Callback del diálogo de carpeta de destino: guarda la ruta en
`page->output_directory_path` y la muestra.

#### `on_decompression_output_search_clicked` *(static)*

```c
static void on_decompression_output_search_clicked(GtkButton *button, gpointer user_data);
```

[interfaz.c:191](../src/gui/interfaz.c#L191)

Botón "Buscar directorio": abre el diálogo de carpeta de destino.

### Acciones

#### `on_compress_clicked` *(static)*

```c
static void on_compress_clicked(GtkButton *button, gpointer user_data);
```

[interfaz.c:204](../src/gui/interfaz.c#L204)

Botón "Iniciar compresión". Si no hay carpeta elegida muestra un aviso. Si la
hay, actualiza estado y barra, llama a la compresión serial y al terminar pone
la barra en 100 %, marca la columna serial como "Completado" y las otras como
"Pendiente".

- **Notas:** la compresión se ejecuta en el hilo de la interfaz, así que la
  ventana se congela mientras dura. Ver el aviso de estado actual más arriba.

#### `on_decompress_clicked` *(static)*

```c
static void on_decompress_clicked(GtkButton *button, gpointer user_data);
```

[interfaz.c:236](../src/gui/interfaz.c#L236)

Botón "Iniciar descompresión". Exige `.huff` y carpeta de destino; luego
descomprime con la versión serial y actualiza barra, estado, tiempos y salud
(la salud serial se marca "Correcto" si la función devolvió éxito).

### Construcción de la ventana

#### `create_compression_table` *(static)*

```c
static GtkWidget *create_compression_table(CompressionPage *page);
```

[interfaz.c:277](../src/gui/interfaz.c#L277)

Crea una `GtkGrid` con columnas Métrica / Serial / Procesos / Hilos y filas
Tiempo, Aceleración, Tamaño original, Tamaño comprimido y Ratio. Guarda en
`page` los punteros a las etiquetas de valores (inicialmente `"--"`) para poder
actualizarlas después.

- **Devuelve:** la grilla.

#### `create_decompression_table` *(static)*

```c
static GtkWidget *create_decompression_table(DecompressionPage *page);
```

[interfaz.c:344](../src/gui/interfaz.c#L344)

Igual que la anterior con las filas Tiempo, Aceleración y Salud.

#### `create_compression_page` *(static)*

```c
static GtkWidget *create_compression_page(CompressionPage *page);
```

[interfaz.c:390](../src/gui/interfaz.c#L390)

Arma la pestaña Compresión en una caja vertical con márgenes de 25 px: título,
fila con la ruta elegida y el botón "Buscar", botón "Iniciar compresión",
etiqueta de estado, barra de progreso y la tabla de resultados. Conecta las
señales `clicked` de los botones pasando `page` como `user_data`.

- **Devuelve:** la caja con la página.

#### `create_decompression_page` *(static)*

```c
static GtkWidget *create_decompression_page(DecompressionPage *page);
```

[interfaz.c:446](../src/gui/interfaz.c#L446)

Arma la pestaña Descompresión: selector de `.huff`, selector de carpeta de
destino, botón "Iniciar descompresión", estado, barra y tabla de resultados.

#### `create_statistics_page` *(static)*

```c
static GtkWidget *create_statistics_page(void);
```

[interfaz.c:510](../src/gui/interfaz.c#L510)

Arma la pestaña Estadísticas: título, subtítulo y una grilla con ocho métricas
(tiempos de compresión y descompresión, salud, aceleraciones, tamaños y ratio)
por variante, todas con `"--"`. Por ahora es estática: no guarda referencias a
las etiquetas, así que no se puede actualizar.

#### `create_main_window`

```c
void create_main_window(GtkApplication *app, gpointer user_data);
```

Declarada en [interfaz.h](../src/gui/interfaz.h), implementada en
[interfaz.c:557](../src/gui/interfaz.c#L557).

Callback de la señal `activate` de la aplicación. Reserva `AppWidgets`, crea la
ventana principal "Compresor Huffman" (1000 × 700), agrega título y subtítulo y
un `GtkNotebook` con las tres pestañas, y muestra la ventana.

- **Parámetros:** `app` — la aplicación GTK; `user_data` — no se usa.
- **Notas:** `AppWidgets` y las rutas elegidas no se liberan nunca; viven hasta
  que termina el programa.

### `main` (gui)

[gui/main.c:4](../src/gui/main.c#L4)

```c
int main(int argc, char **argv);
```

Crea la `GtkApplication` con id `com.proyecto.compresor`, conecta la señal
`activate` a `create_main_window`, ejecuta el bucle principal con
`g_application_run` y libera la aplicación al salir.

- **Devuelve:** el código de salida de `g_application_run`.
