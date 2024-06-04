#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sstream>
#include <unordered_map> 
#include <thread>
#include <sys/stat.h>

#define CONNECTION_BACKLOG 5
#define MAX_THREADS 5
#define CHUNK_SIZE 8192 // 8 KB

using namespace std;
const std::string CRLF = "\r\n";
string directory;


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
  return "Content-Type: " + content_type + CRLF	 + 
  "Content-Length: " + std::to_string(content_length) + CRLF;
}

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        lock_guard<mutex> lock(mtx);
        Q.push(value);
        cv.notify_one();
    }

    T pop() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]{ return !Q.empty(); });
        T value = Q.front();
        Q.pop();
        return value;
    }

private:
    queue<T> Q;
    mutex mtx;
    condition_variable cv;
};


void listening(int server_fd, ThreadSafeQueue<int> &queue) {
	cout << "Listening " << this_thread::get_id() << endl;
	while(true) {
		int flag = listen(server_fd, CONNECTION_BACKLOG);
		if (flag == -1) {
			std::cerr << "listen failed\n";
			continue;
		}
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		std::cout << "Waiting for a client to connect...\n";
		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
		std::cout << "Client connected\n";
		queue.push(client_fd);
	}
}


void serve(ThreadSafeQueue<int> &queue) {
	cout << "Serve " << this_thread::get_id() << endl;
	while (true) {
		int client_fd = queue.pop();
		char buff[65536] = {};
		ssize_t bytes_received = recv(client_fd, buff, sizeof(buff) - 1, 0);
		if (bytes_received < 0) {
			std::cerr << "recv failed\n";
			close(client_fd);
			continue;
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
		else if (endpoint == "/user-agent")
			response
				.append(build_status("200 OK"))
				.append(build_headers("text/plain", (*headers)["User-Agent"].length()))
				.append(CRLF)
				.append((*headers)["User-Agent"]);
		else if (!directory.empty() && endpoint.length() > 6 && endpoint.substr(0, 6) == "/files") {
			string filename = directory + endpoint.substr(6);
			struct stat sb;
			if (stat(filename.c_str(), &sb) == 0 && !(sb.st_mode & S_IFDIR)) {
				ifstream file(filename, ios::binary);
				
				if (!file.is_open()) {
					response
                        .append(build_status("500 Internal Server Error"))
                        .append(CRLF);
                    send(client_fd, response.c_str(), response.length(), 0);
					close(client_fd);
					continue;
				}

				file.seekg(0, ios::end);
				size_t file_size = file.tellg();
				file.seekg(0, ios::beg);

				// Send headers
                response
                    .append(build_status("200 OK"))
                    .append(build_headers("application/octet-stream", file_size))
                    .append(CRLF);
				send(client_fd, response.c_str(), response.length(), 0);

				// Send file in chunks
				long long total_bytes_sent = 0;
                char buffer[CHUNK_SIZE];
                while (file) {
                    file.read(buffer, CHUNK_SIZE);
                    streamsize bytes_read = file.gcount();
                    if (bytes_read > 0) {
                        ssize_t bytes_sent = 0;
                        while (bytes_sent < bytes_read) {
                            ssize_t result = send(client_fd, buffer + bytes_sent, bytes_read - bytes_sent, 0);
                            if (result < 0) {
                                std::cerr << "send failed\n";
                                close(client_fd);
                                break;
                            }
                            bytes_sent += result;
                        }
                    }
					total_bytes_sent += bytes_read;
                }
				file.close();
				response.clear();
			} else
				response
					.append(build_status("404 Not Found"))
					.append(CRLF);
		}
		else
			response
				.append(build_status("404 Not Found"))
				.append(CRLF);

		send(client_fd, response.c_str(), response.length(), 0);

		close(client_fd);
	}
}

int main(int argc, char **argv)
{
	// Flush after every std::cout / std::cerr
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	
	for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--directory" && i + 1 < argc) {
            directory = argv[i + 1];
            break;
        }
    }

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

	ThreadSafeQueue<int> client_queue;
	thread master(listening, server_fd, ref(client_queue));
	vector<thread> workers;
    for (int i = 0; i < MAX_THREADS; ++i)
        workers.push_back(thread(serve, ref(client_queue)));

	master.join();
    for (auto &t : workers) t.join();

	close(server_fd);
	return 0;
}
