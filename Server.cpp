#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

class SafeTCPServer {
private:
    int server_fd;
    int port;
    std::set<int> client_fds;
    fd_set read_fds;
    int max_fd;
    bool should_exit;

    sigset_t original_mask;
    sigset_t blocked_mask;

    static volatile sig_atomic_t signal_received;

public:
    SafeTCPServer(int port) : port(port), max_fd(0), should_exit(false) {
        server_fd = -1;
    }

    ~SafeTCPServer() {
        cleanup();
    }

    static void signal_handler(int sig) {
        signal_received = sig;
    }

    bool initialize() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }

        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
        }

        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Bind failed on port " << port << ": " << strerror(errno) << std::endl;

            int new_port = port + 1;
            address.sin_port = htons(new_port);

            if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
                std::cerr << "Bind failed on port " << new_port << ": " << strerror(errno) << std::endl;
                close(server_fd);
                return false;
            } else {
                port = new_port;
                std::cout << "Successfully bound to port " << port << std::endl;
            }
        }

        if (listen(server_fd, 5) < 0) {
            std::cerr << "Listen failed: " << strerror(errno) << std::endl;
            close(server_fd);
            return false;
        }

        std::cout << "Server listening on port " << port << std::endl;
        return true;
    }

    bool setup_signal_handling() {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGHUP, &sa, NULL) < 0) {
            std::cerr << "Failed to set SIGHUP handler: " << strerror(errno) << std::endl;
            return false;
        }
        if (sigaction(SIGINT, &sa, NULL) < 0) {
            std::cerr << "Failed to set SIGINT handler: " << strerror(errno) << std::endl;
            return false;
        }
        if (sigaction(SIGTERM, &sa, NULL) < 0) {
            std::cerr << "Failed to set SIGTERM handler: " << strerror(errno) << std::endl;
            return false;
        }

        sigemptyset(&blocked_mask);
        sigaddset(&blocked_mask, SIGHUP);
        sigaddset(&blocked_mask, SIGINT);
        sigaddset(&blocked_mask, SIGTERM);

        if (sigprocmask(SIG_BLOCK, &blocked_mask, &original_mask) < 0) {
            std::cerr << "Failed to block signals: " << strerror(errno) << std::endl;
            return false;
        }

        std::cout << "Signal handlers registered and signals blocked" << std::endl;
        return true;
    }

    void run() {
        std::cout << "Server starting main loop..." << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  - Send 'SHUTDOWN' to stop the server" << std::endl;
        std::cout << "  - Send 'QUIT' to disconnect client" << std::endl;

        while (!should_exit) {

            FD_ZERO(&read_fds);
            FD_SET(server_fd, &read_fds);
            max_fd = server_fd;

            for (int client_fd : client_fds) {
                FD_SET(client_fd, &read_fds);
                if (client_fd > max_fd) {
                    max_fd = client_fd;
                }
            }

            struct timespec timeout;
            timeout.tv_sec = 1;  
            timeout.tv_nsec = 0;

            int activity = pselect(max_fd + 1, &read_fds, NULL, NULL, &timeout, &original_mask);

            if (activity < 0) {
                if (errno == EINTR) {
                    std::cout << "pselect interrupted by signal" << std::endl;
                    check_signals();
                    continue;
                } else {
                    std::cerr << "pselect error: " << strerror(errno) << std::endl;
                    break;
                }
            }

            if (activity > 0) {
                if (FD_ISSET(server_fd, &read_fds)) {
                    handle_new_connection();
                }
                handle_client_data();
            }

            check_signals();
        }
        std::cout << "Server main loop ended" << std::endl;
    }

    int get_port() const {
        return port;
    }

private:
    void check_signals() {
        if (signal_received) {
            std::cout << "Processing signal " << signal_received << " in main thread" << std::endl;

            if (signal_received == SIGHUP) {
                handle_sighup();
            } else if (signal_received == SIGINT || signal_received == SIGTERM) {
                std::cout << "Shutdown signal received, exiting..." << std::endl;
                should_exit = true;
            }

            signal_received = 0;
        }
    }

    void handle_sighup() {
        std::cout << "SIGHUP processing:" << std::endl;
        std::cout << "SIGHUP processing completed" << std::endl;
    }

    void handle_new_connection() {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int new_client = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

        if (new_client < 0) {
            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
            return;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "New connection from " << client_ip << ":" << ntohs(client_addr.sin_port)
                  << " (fd: " << new_client << ")" << std::endl;

        if (!client_fds.empty()) {
            std::cout << "Closing extra connection (fd: " << new_client << ")" << std::endl;
            close(new_client);
        } else {
            client_fds.insert(new_client);
            std::cout << "Connection accepted (fd: " << new_client << ")" << std::endl;

            const char* welcome_msg = "Connected to server. Commands:\n- SHUTDOWN: stop server\n- QUIT: disconnect\n";
            send(new_client, welcome_msg, strlen(welcome_msg), 0);
        }
    }

    void handle_client_data() {
        std::vector<int> to_remove;

        for (int client_fd : client_fds) {
            if (FD_ISSET(client_fd, &read_fds)) {
                char buffer[1024];
                ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0'; 

                    std::cout << "Received " << bytes_read << " bytes from client (fd: "
                              << client_fd << "): " << buffer << std::endl;

                    if (strncmp(buffer, "SHUTDOWN", 8) == 0) {
                        std::cout << "Shutdown command received from client!" << std::endl;
                        const char* response = "Server shutting down...\n";
                        send(client_fd, response, strlen(response), 0);
                        should_exit = true;
                        to_remove.push_back(client_fd);
                    }
                    else if (strncmp(buffer, "QUIT", 4) == 0) {
                        std::cout << "Quit command received from client" << std::endl;
                        const char* response = "Goodbye!\n";
                        send(client_fd, response, strlen(response), 0);
                        to_remove.push_back(client_fd);
                    }
                    else {
                        std::string response = "Echo: ";
                        response += buffer;
                        send(client_fd, response.c_str(), response.length(), 0);
                    }
                } else if (bytes_read == 0) {
                    std::cout << "Client disconnected (fd: " << client_fd << ")" << std::endl;
                    to_remove.push_back(client_fd);
                } else {
                    std::cerr << "Recv error from client (fd: " << client_fd
                              << "): " << strerror(errno) << std::endl;
                    to_remove.push_back(client_fd);
                }
            }
        }

        for (int fd : to_remove) {
            close(fd);
            client_fds.erase(fd);
        }
    }

    void cleanup() {
        for (int client_fd : client_fds) {
            close(client_fd);
        }
        client_fds.clear();

        if (server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }

        sigprocmask(SIG_SETMASK, &original_mask, NULL);

        std::cout << "Server cleanup completed" << std::endl;
    }
};

volatile sig_atomic_t SafeTCPServer::signal_received = 0;

int main(int argc, char* argv[]) {
    int port = 8080;

    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    std::cout << "Starting server..." << std::endl;

    SafeTCPServer server(port);

    if (!server.initialize()) {
        std::cerr << "Server initialization failed!" << std::endl;
        return 1;
    }

    if (!server.setup_signal_handling()) {
        return 1;
    }

    std::cout << "Server is running on port " << server.get_port() << std::endl;
    std::cout << "Send SIGHUP for reconfiguration: kill -HUP <PID>" << std::endl;

    server.run();

    std::cout << "Server stopped successfully" << std::endl;
    return 0;
}
