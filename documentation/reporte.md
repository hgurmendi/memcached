# (mini) memcached

En este trabajo práctico se presenta una (mini) implementacion de un servidor de `memcached`, una
cache de alto rendimiento. El servidor está implementado en el lenguaje de programación C y utiliza
la API `epoll` de Linux para un manejo más óptimo de las distintas conexiones, así como POSIX
threads para la programación concurrente.

El núcleo del servidor de cache consiste en una hash table con una cantidad de buckets fija en
tiempo de compilación (configurable a través de un archivo de encabezado, valor por defecto: 10.000
buckets) que utiliza separate chaining como mecanismo de resolución de colisiones. Adicionalmente,
la tabla hash cuenta con una implementación de lista doblemente enlazada que se utiliza para
implementar la política de desalojo. Considerando que ambas estructuras están íntimamente
relacionadas y que los distintos hilos del programa interactúan concurrentemente se incluye un mutex
para proteger las operaciones sobre la tabla hash. La elección de estas estructuras de datos está
fundamentada en que permite implementar las operaciones sobre la cache con el menor costo posible:
inserción, borrado y búsqueda tienen un costo constante en promedio gracias a utilizar una hash
table, mientras que las operaciones de la política de desalojo (determinar la entrada menos
utilizada y determinar una entrada como la más recientemente utilizada) también son constantes
gracias a utilizar una lista doblemente enlazada.

Además del servidor de cache, se implementó un módulo de Erlang llamado `memcached` que implementa
una API simple de Erlang para interactuar con una instancia de `memcached` y que permite fácilmente
utilizar términos de Erlang como claves y valores de la cache.

# Protocolos de comunicación

La comunicación con el servidor se puede realizar a través de un protocolo de texto (puerto 888) así
como a través de un protocolo binario (puerto 889). La implementación es tal cual está descripta en
el enunciado del trabajo práctico.

# Estadísticas de uso

A través del comando `STATS` del servidor de cache es posible obtener información sobre el uso del
servidor de cache desde que fue levantado. Estas estadísticas incluyen:

- la cantidad de comandos `PUT` correctamente procesados.
- la cantidad de comandos `DEL` correctamente procesados.
- la cantidad de comandos `GET` correctamente procesados.
- la cantidad de comandos `TAKE` correctamente procesados.
- la cantidad de comandos `STATS` correctamente procesados.
- la cantidad de pares clave-valor (entradas) presentes en la cache.

# Multithreading

El servidor de cache corre un hilo por cada hilo de hardware disponible y cada hilo tiene la
capacidad de manejar la comunicación con múltiples clientes en simultáneo, todo gracias a la
colaboración entre la API `epoll` de Linux y la API de POSIX threads. Dado que `epoll` está sólo
disponible en Linux, el servidor de cache no puede ser ejecutado en otro sistema operativo, pero
en el repositorio de git existe una configuración (muy simple) de Docker para poder correr este
servidor de cache en cualquier sistema que soporte esa plataforma de virtualización. De hecho, el
trabajo práctico fue implementado en una computadora usando MacOS (no quedaba otra...).

# Uso de memoria y política de desalojo

La memoria del programa se limita al iniciar el programa a un valor fijo que está determinado en
tiempo de compilación (configurable a través de un archivo de encabezado, valor por defecto: 1GB).
Al atender las operaciones de la cache es posible que se llegue a ese límite de memoria y los
pedidos de memoria dinámica pueden fallar, en cuyo caso hay que determinar qué entrada(s) eliminar
para poder completar la operación. Esa decisión es implementada a través de una política de desalojo
LRU (Least Recently Used) que utiliza una lista doblemente enlazada que trabaja íntimamente con la
hash table.

Cada entrada de la hash table contiene una referencia a un nodo de la lista doblemente enlazada
(bautizada como _cola de uso_) y cada nodo de la cola de uso contiene una referencia a la
correspondiente entrada de la hash table. Considerando que las operaciones en el principio y el
final de la cola de uso son constantes por utilizar una lista doblemente enlazada, resulta muy
eficiente y fácil determinar qué entrada de la hash table es la menos utilizada, y considerando que
remover nodos de la cola de uso y colocarlos en el extremo también son operaciones eficientes, el
costo de actualizar la cola de uso según una consulta a la tabla hash resulta relativamente poco.
(NOTA: al momento de la redacción de esta parte del informe se me ocurrió que una posible mejora de
la política de desalojo consiste en guardar en el nodo de la cola de uso una referencia al bucket o
el índice del bucket de la entrada de la hash table, ya que evitaría tener que realizar el hash de
la clave para poder determinar esa información, y como la hash table tiene una cantidad fija en
tiempo de compilación de buckets, podemos elegir guardar el índice en vez de la referencia. Como
otras propuestas, esto puede ser una posible mejora de rendimiento).

Considerando que la hash table y la cola de uso están íntimamente relacionadas, se decidió proteger
ambas estructuras de dato con el mismo mutex. Como una posible mejora a esta situación, creo que
sería posible reemplazar el mutex por un spinlock (ambas abstracciones son ofrecidas por la API de
POSIX) para ganar un poco más de rendimiento ya que (a mi entender) un mutex pone al hilo llamante a
dormir cuando está a la espera de tomar un mutex, mientras que un spinlock utiliza busy waiting.
Suponiendo que el servidor de cache en general posee una carga de lectura / escritura mucho más
elevada que de procesamiento al estar interactuando con muchos clientes, el costo de hacer busy wait
resulta mucho menor que el costo de dormir y luego despertar un hilo. A pesar de este rápido
análisis, se eligió no hacer este cambio y dejarlo como una posible mejora de rendimiento.

Cuando un pedido de memoria dinámica falla se realizan desalojos hasta que el pedido resulta exitoso
o se realiza una cantidad máxima de desalojos (configurable a través de un archivo de encabezado,
valor por defecto: 50) y el pedido es reportado como fallado al cliente. Esto se realiza devolviendo
un valor `EUNK` tanto en el protocolo binario como en el protocolo de texto.

# Bajada de privilegios

Se decidió implementar la solución propuesta en el enunciado del trabajo práctico que consiste en
usar un programa auxiliar (en este caso denominado `binder`) que realiza las llamadas a sistema
privilegiadas `bind` para cada protocolo de comunicación, luego baja los permisos mediante `setuid`
y `getuid` y finalmente ejecuta el programa de la cache (`memcached`) pasando como argumentos los
descriptores de archivo de los sockets correctamente bindeados.

Considerando que con la bajada de privilegios se busca tener un servidor de cache más seguro, una
alternativa puede ser correr el servidor en Docker con los scripts provistos en el repositorio, a
costo de performance reducida debido a la virtualización a través de contenedores.

# Bindings para Erlang

En el directorio `resources/` se incluye el módulo `memcached` de Erlang que ofrece una API para
interactuar con la API de `memcached` de forma transparente y que permite utilizar términos de
Erlang como claves y valores de la cache. El funcionamiento de estos bindings para Erlang está
descripto en el archivo `README.md`.

# Análisis de performance

En el directorio `resources/` se incluye un pequeño script de Python que fue implementado muy
rápidamente y que fue utilizado para analizar de forma rápida la performance del servidor de cache.
Dicho script se puede usar junto con GNU Parallel para bombardear el servidor con pedidos. Más
información sobre el script en el archivo `README.md`.

# Corriendo el proyecto

Para correr el proyecto se debería correr `make` en el directorio `src/` y luego correr el programa
"binder" de la siguiente forma:

```bash
$ sudo ./binder memcached 888 889 1000 1000
```

El comando de arriba correrá el programa `binder` que creará los sockets de escucha para ambos
protocolos en los puertos privilegiados 888 y 889, tirará los permisos de root y luego ejecutará el
programa `memcached` como el usuario 1000 con el grupo 1000 (que son generalmente los valores
más comunes).
