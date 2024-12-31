#include "api.h"

// accepts song titles to be sent via TCP and causes them to be rendered via FreeType
int createServer() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 1) == -1) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d...\n", PORT);

    // Accept a client connection
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    // Receive and echo data
    ssize_t bytes_received;
    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        printf("Received: %s\n", buffer);

        // Echo back the received data
        if (send(client_fd, buffer, bytes_received, 0) == -1) {
            perror("Send failed");
            close(client_fd);
            close(server_fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_received == -1) {
        perror("Receive failed");
    }

    // Clean up
    close(client_fd);
    close(server_fd);
    printf("Connection closed.\n");

    return 0;
}