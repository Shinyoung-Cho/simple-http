#pragma once

#include <iostream>           // std::stringstream
#include <thread>             // std::thread
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable
#include <vector>
#include <queue>
#include <functional>

//#include <sched.h>

class Pool {
public:
	Pool() {
		int hardConcurrency = std::thread::hardware_concurrency();
		int softConcurrency = hardConcurrency * 64;
		for (int i = 0; i < softConcurrency; ++i) {
			pool.push_back(std::thread(loop));
		}
	}

	static void loop() {
		while (true)
		{
			std::function<void()> func = NULL;
			{
				std::unique_lock<std::mutex> lock(mutex);
				cond.wait(lock, [] {return !queue.empty(); });
				func = queue.front();
				queue.pop();
			}
			cond.notify_one();
			func();
		}
	};

	void insert(std::function<void()> func) {
		{
			std::unique_lock<std::mutex> lock(mutex);
			queue.push(func);
			cond.notify_one();
		}
	}
private:
	static std::vector<std::thread> pool;
	static std::queue<std::function<void()>> queue;
	static std::mutex mutex;
	static std::condition_variable cond;
};
std::vector<std::thread> Pool::pool;
std::queue<std::function<void()>> Pool::queue;
std::mutex Pool::mutex;
std::condition_variable Pool::cond;
////////////////////////////////////////////////////////////////////////////////



// c headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

// cpp headers
#include <sstream>

#define HEADER_BUFFER 1024
#define BODY_BUFFER   1024*2
class Request {
public:
	Request(int sock, sockaddr_in* remote) {
		memset(header_buffer, 0, HEADER_BUFFER);
		memset(read_buffer, 0, BODY_BUFFER);
		memset(remoteAddr, 0, 18);
		this->fd = sock;

		//inet_ntoa()
		unsigned char *bytes = (unsigned char *)&remote;
		snprintf(remoteAddr, 18, "%d.%d.%d.%d",
			bytes[0], bytes[1], bytes[2], bytes[3]);


		//read from a socket
		if (readh(fd, header_buffer) == -1) {
			shutdown(fd, SHUT_RDWR);
			close(fd);
			this->fd = -1;
			return;
		}

		// http header parsing
		char* p;
		char* r = header_buffer;
		this->method = strtok_r(r, " \n", &r);
		this->path = strtok_r(r, " \n", &r);
		p = strtok_r(r, " \n", &r); // skip the protocol string
		while (p) {
			if (strcmp(p, "Content-Length:") == 0) {
				this->contentLength = atoi(strtok_r(r, " \n", &r));
			} p = strtok_r(r, " \n", &r);
		}

		if ((strcmp(this->method, "POST") == 0) && this->contentLength) {
			if (readn(fd, read_buffer, this->contentLength) != this->contentLength) {
				; // body is not fully read
			}
		}
	}
	char* getMethod() {
		return this->method;
	}
	char* getPath() {
		return this->path;
	}
	char* getRemoteAddr() {
		return this->remoteAddr;
	}
	bool isReady() {
		return this->fd == -1 ? false : true;
	}
	std::string getBody() {
		return std::string(this->read_buffer);
	}

private:
	char header_buffer[HEADER_BUFFER];  // read buffer from socket
	char read_buffer[BODY_BUFFER];  // read buffer from socket

	char* method;
	char* path;
	int contentLength;
	char remoteAddr[18];
	
	int fd = -1;

	ssize_t readh(int fd, char* ptr) {
		char buf[5];
		memset(buf, 0, 5);
		int k = 0;
		ssize_t n = 0;
		for (int i = 0; i < HEADER_BUFFER; ++i) {
			if (1 == read(fd, ptr, 1)) { ++n; }
			else { return -1; } // return on socket read error

			// control char determination
			if ((*ptr) < 32) { buf[k] = *ptr; ++k; }
			else { k = 0; }

			if (3 < k) {
				if ((buf[0] == 13) && (buf[1] == 10) && (buf[0] == buf[2]) && (buf[1] == buf[3])) {
					return n;
				}
			}
			++ptr;
		}
		// undefined. end of http header has not been found.
		return n;
	}
	ssize_t readn(int fd, char* ptr, size_t n) {
		size_t  n_left;
		ssize_t n_read;

		n_left = n;
		while (n_left > 0) {
			if ((n_read = read(fd, ptr, n_left)) < 0) {
				if (errno == EINTR) { n_read = 0; }
				else { return (-1); }
			}
			else if (n_read == 0) {    // EOF
				break;
			}
			n_left -= n_read;
			ptr += n_read;
		} // end of while
		return (n - n_left);
	}
};

class Response {
public:
	Response(int sock) {
		this->fd = sock;
	}
	Response(int sock, int code) {
		this->fd = sock;
		if (code == 200) {
			writen(fd, RESPONSE_200, strlen(RESPONSE_200));
		}
		else if (code == 404) {
			writen(fd, RESPONSE_404, strlen(RESPONSE_404));
		}
		else {
			writen(fd, RESPONSE_200, strlen(RESPONSE_200)); // return 200 defalut
		}
		mClose();
		hasFlushed = true;
	}
	~Response() {
		if (hasFlushed) return;
		mClose();
	}

	void append(std::string s) {
		if (hasFlushed) return;
		ss << s;
	}
	void append(char* s) {
		if (hasFlushed) return;
		ss << s;
	}
	void flush() {
		if (hasFlushed) return;

		const char* HEADER_L1 = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
		const char* HEADER_L2 = "Content-Length: ";
		const char* HEADER_L3 = "\r\n\r\n";
		long unsigned int content_length = ss.str().length();
		std::string body_len = std::to_string(content_length);

		ssize_t n = 0;
		n += writen(fd, HEADER_L1, strlen(HEADER_L1));
		n += writen(fd, HEADER_L2, strlen(HEADER_L2));
		n += writen(fd, body_len.c_str(), strlen(body_len.c_str()));
		n += writen(fd, HEADER_L3, strlen(HEADER_L3));
		n += writen(fd, ss.str().c_str(), content_length);
		mClose();
		hasFlushed = true;
	}
private:
	int fd = -1;
	const char* RESPONSE_200 = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
	const char* RESPONSE_404 = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 19\r\n\r\nResource not found.";
	std::stringstream ss;
	bool hasFlushed = false;

	void mClose() {
		shutdown(fd, SHUT_RDWR);
		close(fd);
	}
	ssize_t writen(int fd, const char* ptr, size_t n) {
		size_t  n_left;
		ssize_t n_written;

		n_left = n;
		while (n_left > 0) {
			if ((n_written = write(fd, ptr, n_left)) <= 0) {
				if (errno == EINTR) { n_written = 0; }
				else { return (-1); }
			}
			n_left -= n_written;
			ptr += n_written;
		} // end of while
		return (n);
	}
};



class Base {
public:
	Base() { ; }
	~Base() { ; }

	void run() {
		int r_sock_fd, l_sockfd;
		struct sockaddr_in serv_addr, cli_addr;
		socklen_t clilen;

		// init
		memset((char*)&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(this->port);
		clilen = sizeof(cli_addr);

		if ((l_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			return;
		if (bind(l_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
			std::cerr << "bind error" << std::endl;
			return;
		}

		listen(l_sockfd, 5);

		do {
			r_sock_fd = accept(l_sockfd, (struct sockaddr *) &cli_addr, &clilen);
			if (r_sock_fd < 0) {
				//errno = 0; // ignore
			}
			else {
				pool.insert(std::bind(&Base::handler, this, r_sock_fd, cli_addr));
			}
		} while (1);
	}

	void handler(int r_sock, sockaddr_in cli_addr) {

	
		Request request(r_sock, &cli_addr);
		if (request.isReady()) {
			Response response(r_sock, 200);
			
			std::cout << request.getBody() << std::endl;
			
			//int64_t t = std::chrono::system_clock::now().time_since_epoch().count();
			//int c = sched_getcpu();
			//std::cout << r_sock << "\t" << std::endl;
		}

	}

private:
	Pool pool;
	int port = 8080;
};


