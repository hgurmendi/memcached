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
"best-effort" LRU (Least Recently Used) que utiliza una lista doblemente enlazada que trabaja
íntimamente con la hash table.

Cada entrada de la hash table contiene una referencia a un nodo de la lista doblemente enlazada
(bautizada como _cola de uso_) y cada nodo de la cola de uso contiene una referencia a la
correspondiente entrada de la hash table. Considerando que las operaciones en el principio y el
final de la cola de uso son constantes por utilizar una lista doblemente enlazada, resulta muy
eficiente y fácil determinar qué entrada de la hash table es la menos utilizada, y considerando que
remover nodos de la cola de uso y colocarlos en el extremo también son operaciones eficientes, el
costo de actualizar la cola de uso según una consulta a la tabla hash resulta relativamente poco.

Se implementaron 3 distintos tipos de mutexes para sincronizar el acceso de los hilos a la
estructura hash table compartida:

- un mutex por bucket de la hash table, cada uno almacenado en el array del miembro
  `bucket_mutexes` de la hash table.
- un mutex para el contador de claves de la hash table, almacenado en el miembro `key_count_mutex`
  de la hash table.
- un mutex para la cola de uso de la hash table, almacenado en el miembro `usage_mutex` de la hash
  table.

La mayoría de los comandos de la hash table terminan adquiriendo el mutex correspondiente al bucket
de la clave contenida en el comando para realizar su operación y también adquieren el mutex de la
cola de uso para actualizarla (dependiendo del comando de la hash table, esta actualización puede
ser mover un nodo de uso al extremo de mayor uso, o remover un nodo completamente de la cola de
uso) y por lo tanto hay que tener especial cuidado cuando se está haciendo el desalojo de una
entrada de la hash table ya que esa otra operación también toma el mutex de la cola de uso y el
mutex del bucket donde se aloja la víctima del desalojo. Es por esa razón que al realizar un
desalojo se le da prioridad en cuanto a la sincronización del acceso a los buckets a las operaciones
de la hash table y por eso la política de desalojo resulta en "best-effort" Least Recently Used, ya
que va a haber oportunidades en las que no es posible desalojar la entrada menos utilizada de la
tabla pero igual se toma una entrada con cualidades muy similares.

Por otro lado, existe un mutex para proteger el acceso al contador compartido `key_count` de la
hash table, al que acceden todos los hilos de la tabla. Este mutex cubre una región crítica muy
pequeña a la que acceden todos los hilos que manipulan la hash table. Aquí se podría haber optado
por guardar un contador por bucket también, pero esto resulta más simple y rápido.

Cuando un pedido de memoria dinámica falla se realizan desalojos hasta que el pedido resulta exitoso
o se realiza una cantidad máxima de desalojos (configurable a través de un archivo de encabezado,
valor por defecto: 50) y el pedido es reportado como fallado al cliente. Esto se notifica al cliente devolviendo un valor `EUNK` tanto en el protocolo binario como en el protocolo de texto.

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
