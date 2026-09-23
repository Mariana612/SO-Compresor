# SO-Compresor

## Interfaz gráfica
La interfaz gráfica del proyecto fue desarrollada en C utilizando GTK 4. Esto le permite al usuario poder interactuar con las diferentes implementaciones del compresor y descompresor Huffman sin utilizar directamente la línea de comandos.

La interfaz está dividida en tres secciones principales:
- Compresión: permite seleccionar un directorio de entrada y ejecutar las versiones Serial, Procesos y Hilos del compresor.
- Descompresión: permite seleccionar los archivos o directorios correspondientes y ejecutar las tres versiones del descompresor.
- Estadísticas: muestra los resultados obtenidos durante las ejecuciones y permite comparar el rendimiento de las diferentes implementaciones.

Durante la ejecución se utiliza una barra de progreso para indicar el estado del proceso y la versión que se está ejecutando. Los resultados se presentan mediante tablas que incluyen métricas como el tiempo de ejecución, la aceleración, la salud de la compresión, el tamaño de los archivos y la razón de compresión.

### Dependencias de la interfaz gráfica
Para desarrollar y compilar la interfaz gráfica se utilizaron las siguientes herramientas:
- GCC y herramientas de compilación incluidas en build-essential.
- pkg-config, utilizado para obtener automáticamente las opciones necesarias para compilar y enlazar GTK.
- GTK 4 y sus archivos de desarrollo, instalados en el paquete libgtk-4-dev.

En Debian 13, estas dependencias pueden instalarse utilizando:

su

apt update

apt install build-essential pkg-config libgtk-4-dev

Para comprobar que GTK 4 se encuentra correctamente instalado se puede ejecutar:

pkg-config --modversion gtk4

Este comando debe mostrar la versión instalada de GTK 4.

### Compilación de la interfaz
La interfaz se encuentra separada en los archivos:

main.c

interfaz.c

interfaz.h

main.c se encarga de iniciar la aplicación GTK, mientras que interfaz.c contiene la creación y el comportamiento de las ventanas, botones, pestañas, selectores de directorios, barras de progreso y tablas de resultados. El archivo interfaz.h contiene las declaraciones necesarias para utilizar la interfaz desde otros módulos del programa.

Para compilar manualmente la interfaz se puede utilizar:

gcc main.c interfaz.c -o compresor $(pkg-config --cflags --libs gtk4)

Ell programa se ejecuta con:

./compresor

### Organización de la interfaz
La ventana principal contiene el título:
Compresor y Descompresor utilizando Huffman
y el subtítulo:
Comparación de las versiones Serial, de Procesos y de Hilos

La navegación se realiza mediante tres pestañas:
Compresión | Descompresión | Estadísticas

En las pestañas de compresión y descompresión, el usuario puede seleccionar un directorio. Una vez seleccionado, la ruta se muestra en pantalla.

Al iniciar una operación, la interfaz muestra el progreso de las tres implementaciones:
Serial -> Procesos -> Hilos

Los resultados obtenidos por cada implementación se almacenan y se muestran en las tablas para poder compararlos.

La interfaz gráfica está separada de la implementación del algoritmo de Huffman. De esta forma, la GUI se encarga únicamente de recibir las acciones del usuario, ejecutar las funciones correspondientes y mostrar los resultados, mientras que los módulos de compresión, descompresión, procesos e hilos contienen la lógica del programa.
#