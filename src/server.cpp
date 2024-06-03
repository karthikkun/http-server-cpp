#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "util.h"
#include <sstream>
#include <unordered_map> 

using namespace std;
const std::string CRLF = "\r\n";

void print(const char* msg) {
	std::cout << msg << std::endl;
}

std::string get_endpoint(char buf[]) {
  std::string request (buf);
  int start = request.find(" ") + 1;
  int end = request.find(" ", start);
  return request.substr(start, end - start);
}

std::unordered_map<std::string, std::string>* get_headers(char buf[])
{
	unordered_map<string, string>* headers = new unordered_map<string, string>();
	std::string request(buf);
	int start = request.find(CRLF) + 2;
	int end = request.find(CRLF + CRLF, start);
	std::string headers_str = request.substr(start, end - start);
    std::istringstream stream(headers_str);
    std::string line;

    while (std::getline(stream, line) && !line.empty()) {
        if (line.back() == '\r') line.pop_back();
        std::size_t pos = line.find(": ");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2);
            (*headers)[key] = value;
        }
    }
    return headers;
}

std::string build_status (std::string status) {
  return "HTTP/1.1 " + status + CRLF;
}

std::string build_headers(const std::string& content_type, int content_length) {
  return "Content-Type: " + content_type + CRLF + 
  "Content-Length: " + std::to_string(content_length) + CRLF;
}

int main(int argc, char **argv)
{
	// Flush after every std::cout / std::cerr
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;

	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		std::cerr << "Failed to create server socket\n";
		return 1;
	}

	// setting SO_REUSEADDR ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		std::cerr << "setsockopt failed\n";
		return 1;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(4221);

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
	{
		std::cerr << "Failed to bind to port 4221\n";
		close(server_fd);
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		std::cerr << "listen failed\n";
		close(server_fd);
		return 1;
	}

	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	std::cout << "Waiting for a client to connect...\n";

	int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
	std::cout << "Client connected\n";

	char buff[65536] = {};
	ssize_t bytes_received = recv(client_fd, buff, sizeof(buff) - 1, 0);
	if (bytes_received < 0) {
		std::cerr << "recv failed\n";
		close(client_fd);
		close(server_fd);
		return 1;
	}
	buff[bytes_received] = '\0';

	// parsing url endpoint
	std::string endpoint {get_endpoint(buff)};
	// parsing headers
	std::unordered_map<std::string, std::string>* headers{get_headers(buff)};

	std::string response;

	if (endpoint == "/")
		response
			.append(build_status("200 OK"))
			.append(CRLF);
	else if (endpoint.length() > 5 && endpoint.substr(0, 5) == "/echo")
		response
			.append(build_status("200 OK"))
			.append(build_headers("text/plain", endpoint.substr(6).length()))
			.append(CRLF)
			.append(endpoint.substr(6));
	else if (endpoint == "/user-agent") {
		std::string response_body = "User-Agent: " + (*headers)["User-Agent"] + CRLF;
		response
			.append(build_status("200 OK"))
			.append(build_headers("text/plain", response_body.length()))
			.append(CRLF)
			.append(response_body);
	}
	else
		response
			.append(build_status("404 Not Found"))
			.append(CRLF);

	send(client_fd, response.c_str(), response.length(), 0);

	close(server_fd);
	close(client_fd);

	return 0;
}
