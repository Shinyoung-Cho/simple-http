#pragma once

#include <iostream>           // std::stringstream
#include <thread>             // std::thread
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable
#include <vector>
#include <queue>
#include <functional>
#include <chrono>

#define DEBUG 0

//#include <sched.h>
class Pool {
public:
	Pool() {
		int hardConcurrency = std::thread::hardware_concurrency();
		int softConcurrency = hardConcurrency * 8;
		for (int i = 0; i < softConcurrency; ++i) {
			pool.push_back(std::thread(loop));
		}
	}
	~Pool() {
	}

	static void loop() {
		while (true)
		{
			std::function<void()> func = NULL;
			{
				std::unique_lock<std::mutex> lock(mutex);
				cond.wait(lock, [] {return (!queue.empty()); });
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
#include <time.h>

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
		unsigned char *bytes = (unsigned char *)&remote->sin_addr;
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
		char* p = NULL;
		char* r = header_buffer;
		this->method = strtok_r(r, " ", &r);
		this->path = strtok_r(r, " ", &r);
		this->protocol = strtok_r(r, "\r\n", &r);
		p = strtok_r(r, " \r\n", &r);
		while (p) {
			// if-else statement can be extended.
			if(strcmp(p, "Host:") == 0){
				this->host = strtok_r(r, "\r\n", &r);
			} else if (strcmp(p, "Content-Type:") == 0) {
				this->contentType = strtok_r(r, "\r\n", &r);
			} else if (strcmp(p, "Content-Length:") == 0) {
				this->contentLength = atoi(strtok_r(r, "\r\n", &r));
			} p = strtok_r(r, " \r\n", &r);
		}

		// path and query parsing
		p = NULL;
		r = this->path;
		p = strtok_r(r, "?", &r);
		if(p){
			this->path = p;
		}
		this->queryString = r; // can be either the pointer to the query string or NULL.

		// body read only if and if only,
		if ((strcmp(this->method, "POST") == 0) && this->contentLength) {
			if (readn(fd, read_buffer, this->contentLength) != this->contentLength) {
				; // body is not fully read
			}
		}
	}
	bool isReady() {
		return this->fd == -1 ? false : true;
	}
	int getFd() {
		return this->fd;
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
	char* getHost(){
		return this->host;
	}
	char* getProtocol(){
		return this->protocol;
	}
	char* getQueryString(){return this->queryString;}
	char* getContentType(){return this->contentType;}
	char* getParameter(const char* key){ // currently does the linear search all the times
		if(!this->queryString) return NULL;
		char* p = NULL;
		char* r = this->queryString;
		p = strtok_r(r, "=&", &r);
		while(p){
			if(strcmp(p, key) == 0){
				return strtok_r(r, "=&", &r);
			} p = strtok_r(r, "=&", &r);
		} return NULL;
	}
	std::string getBody() {
		return std::string(this->read_buffer);
	}

private:
	char header_buffer[HEADER_BUFFER];  // read buffer from socket
	char read_buffer[BODY_BUFFER];  // read buffer from socket

	char* method = NULL;
	char* path = NULL;
	char* queryString = NULL;
	char* host = NULL;
	char* protocol = NULL;
	char* contentType = NULL;
	int contentLength = 0;
	char remoteAddr[18] = {0};

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
	Response(Request& request) {
		this->fd = request.getFd();
		if (this->fd == -1) this->isClosed = true;
	}
	~Response() {
		finalize();
	}

	void clear() {
		if (isClosed) return;
		b.clear();
	}
	void append(std::string s) {
		if (isClosed) return;
		b << s;
	}
	void append(char* s) {
		if (isClosed) return;
		b << s;
	}
	void setStatusCode(int c) {
		this->statusCode = c;
	}
	void setContentType(char* contentType) {
		this->contentType = contentType;
	}
	int getStatusCode() { return this->statusCode; }
	int getContentLength() { return b.str().size(); }


protected:
	void generateHeader(int contentLength) {
		if (statusCode == 200) {
			h << RESPONSE_200 << ENDL;
		}
		else if (statusCode == 400) {
			h << RESPONSE_400 << ENDL;
		}
		else if (statusCode == 404) {
			h << RESPONSE_404 << ENDL;
		}
		else if (statusCode == 405) {
			h << RESPONSE_405 << ENDL;
		}
		else {
			h << RESPONSE_500 << ENDL;
		}
		h << "Server: " << serverName << ENDL
			<< "Content-Type: " << contentType << ENDL
			<< "Content-Length: " << contentLength << ENDL << ENDL;
	}
	void finalize() {
		if (isClosed) return;

		// finalize body
		std::string temp = b.str();
		char* body = (char*)temp.c_str();
		this->contentLength = strlen(body);

		// finalize header
		generateHeader(contentLength);
		std::string temp2 = h.str();
		char* header = (char*)temp2.c_str();

		//std::cout << "[" << header << body << "]\n";

		// write on socket
		ssize_t n = 0;
		n += writen(fd, header, strlen(header));
		n += writen(fd, body, strlen(body));

		shutdown(fd, SHUT_RDWR);
		close(fd);
		isClosed = true;
	}

private:
	int fd = -1;
	const char* ENDL = "\r\n";
	const char* RESPONSE_200 = "HTTP/1.1 200 OK";
	const char* RESPONSE_400 = "HTTP/1.1 400 Bad Request";
	const char* RESPONSE_404 = "HTTP/1.1 404 Not Found";
	const char* RESPONSE_405 = "HTTP/1.1 405 Method Not Allowed";
	const char* RESPONSE_500 = "HTTP/1.1 500 Internal Server Error";
	std::stringstream h;
	std::stringstream b;
	bool isClosed = false;

	int contentLength = -1;
	int   statusCode = 200;
	const char* contentType = "text/plain"; // default
	const char* serverName = "SIMPLE-HTTP";
	//int   statusCode = 0;
	//char* contentType = NULL;

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
	Base() {
		pool = new Pool;
	}
	Base(int port) {
		pool = new Pool;
		this->port = port;
	}
	~Base() {
		delete pool;
	}

	void start() {
		int l_sock_fd, r_sock_fd;
		struct sockaddr_in serv_addr, cli_addr;
		socklen_t clilen;

		// init
		memset((char*)&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(this->port);
		clilen = sizeof(cli_addr);

		l_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (l_sock_fd < 0) {
			std::cerr << "socket error" << std::endl;
			return;
		}
		if (bind(l_sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
			std::cerr << "bind error" << std::endl;
			return;
		}

		listen(l_sock_fd, 1024);
		std::cout << "server socket initialized." << std::endl;

		do {
			r_sock_fd = accept(l_sock_fd, (struct sockaddr *) &cli_addr, &clilen);
			if (r_sock_fd < 0) {
				errno = 0; // ignore
			}
			else {
				pool->insert(std::bind(&Base::c_handler, this, r_sock_fd, cli_addr));
			}
		} while (1);
	}

protected:
	// request handler
	// define as virtual so that derived class can override
	virtual void r_handler(Request* request, Response* response){
		char* path = request->getPath();
		char* method = request->getMethod();

		if (!strcmp(path, "/")) {
			if (!strcmp(method, "GET")) {
				response->setStatusCode(200);
				response->append("Hello, world!");
			}
			else {
				response->setStatusCode(405);
			}
		}
		else if (!strcmp(path, "/foo")) {
			if (!strcmp(method, "GET")) {
				
			char* value = request->getParameter("hello");
#if DEBUG
			if(value) printf("value=[%s]\n", value);
#endif
				response->setStatusCode(200);
				response->append("You have reached foo resource.");
			}
			else if (!strcmp(method, "POST")) {
				response->setStatusCode(500);
			}
			else {
				response->setStatusCode(405);
			}
		}
		else if (!strcmp(path, "/bar")) {
			response->setStatusCode(200);
			response->append("You have reached bar resource.");
		}
		else {
			response->setStatusCode(404);
			response->append("Nothing is here.");
		}
	}

private:
	int port = 8080;
	Pool* pool;
	
	// host handler
	void h_handler(Request* request, Response* response){
		
		//denied hosts
		if(strcmp(request->getHost(), "somehost.com") == 0){
			std::cerr<< "requested host is not allowed." <<std::endl;
			return;
		}else if(strcmp(request->getHost(), "otherhost.com") == 0){
			std::cerr<< "requested host is not allowed." <<std::endl;
			return;
		}
		
		//allowed hosts
		if(strcmp(request->getHost(), "localhost") == 0){
			//chain request handler here 
			;
		}else if(strcmp(request->getHost(), "www.example.com") == 0){
			//chain request handler here 
			;
		}else{
			// defaule request handler
			r_handler(request, response);
		}
		
		
	}

	// connection handler
	void c_handler(int r_sock, sockaddr_in cli_addr) {
		Request request(r_sock, &cli_addr);
		Response response(request);

		if (!request.isReady()) { return; }

#if DEBUG
	printf("host: [%s]\n", request.getHost());
	printf("path: [%s]\n", request.getPath());
	printf("qstr: [%s]\n", request.getQueryString());
	printf("mthd: [%s]\n", request.getMethod());
	printf("prtc: [%s]\n", request.getProtocol());
	printf("ctyp: [%s]\n", request.getContentType());
#endif
		h_handler(&request, &response);

		// start of info
		auto now = std::chrono::system_clock::now().time_since_epoch().count();
		time_t  n_now = now % 1000; now = now / 1000;
		time_t  u_now = now % 1000; now = now / 1000;
		time_t  m_now = now % 1000; now = now / 1000;
		time_t  t_now = now;
		struct tm  ts;
		//2018-01-02 11:11:33.333 333 333NUL
		char buf[32] = {0};
		ts = *localtime(&t_now);
		strftime(buf, 19+1, "%F %T", &ts);
		sprintf(buf+19, ".%03d", m_now);
		sprintf(buf+19+4, " %03d", u_now);
		sprintf(buf+19+4+4, " %03d", n_now);
		std::cout
			<< "[" << buf << "] "
			//<< request.getRemoteAddr() << " "
			<< request.getMethod() << " "
			<< request.getPath() << " "
			<< response.getStatusCode() << " "
			<< response.getContentLength() << std::endl;
		// end of info
	}
};
