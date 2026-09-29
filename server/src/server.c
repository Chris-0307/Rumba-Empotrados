#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_address;

    char buffer[BUFFER_SIZE];

    const char *response_ok =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"status\":\"ok\"}\n";

    const char *response_not_found =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"error\":\"not found\"}\n";

    /*
     * Crear socket TCP.
     */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("Error creando socket");
        return 1;
    }

    /*
     * Configurar direccion del servidor.
     */
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    /*
     * Asociar el socket con el puerto 8080.
     */
    if (bind(
            server_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) < 0)
    {
        perror("Error en bind");
        close(server_fd);
        return 1;
    }

    /*
     * Esperar conexiones.
     */
    if (listen(server_fd, 5) < 0)
    {
        perror("Error en listen");
        close(server_fd);
        return 1;
    }

    printf("Servidor escuchando en puerto %d\n", PORT);

    /*
     * El servidor se mantiene ejecutando.
     */
    while (1)
    {
        client_fd = accept(server_fd, NULL, NULL);

        if (client_fd < 0)
        {
            perror("Error aceptando cliente");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));

        /*
         * Leer la peticion enviada por el cliente.
         */
        ssize_t bytes_read;

        bytes_read = read(
            client_fd,
            buffer,
            sizeof(buffer) - 1
        );

        if (bytes_read < 0)
        {
            perror("Error leyendo peticion");
            close(client_fd);
            continue;
        }

        /*
         * Si read devuelve 0, el cliente cerro la conexion.
         */
        if (bytes_read == 0)
        {
            close(client_fd);
            continue;
        }

        /*
         * Terminar el texto recibido correctamente.
         */
        buffer[bytes_read] = '\0';

        printf("\nPeticion recibida:\n%s\n", buffer);

        /*
         * Comprobar si el cliente pidio /api/status.
         */
        if (strncmp(buffer, "GET /api/status ", 16) == 0)
        {
            ssize_t bytes_written;

            bytes_written = write(
                client_fd,
                response_ok,
                strlen(response_ok)
            );

            if (bytes_written < 0)
            {
                perror("Error enviando respuesta");
            }
        }
        else
        {
            ssize_t bytes_written;

            bytes_written = write(
                client_fd,
                response_not_found,
                strlen(response_not_found)
            );

            if (bytes_written < 0)
            {
                perror("Error enviando respuesta");
            }
        }

        close(client_fd);
    }

    close(server_fd);

    return 0;
}
