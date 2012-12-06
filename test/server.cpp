
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

#include "request.h"

namespace test {
	using namespace httplib;

	struct SyncServerRequest : public ServerRequest {

		SyncServerRequest(int s) : sock(s) { done = false; }
		~SyncServerRequest() { close(sock); }

		virtual void request(RequestHeader& header) {
			std::cout << "received request: " << header.method << " " << header.uri << std::endl;
			std::cout << "---" << std::endl;
			for (HttpHeaders::const_iterator i = header.headers.begin(); i != header.headers.end(); ++i)
				std::cout << i->name << ": " << i->value << std::endl;
			std::cout << "---" << std::endl;
		}

		virtual void recv(const char * b, int s) {
			std::cout << "received body: " << std::endl;
			std::cout << "---" << std::endl;
			std::cout << string((const char*)b, (const char*)b + s) << std::endl;
			std::cout << "---" << std::endl;
		}

		virtual void end() {
			std::cout << "request end" << std::endl;
			ResponseHeader r;
			r.code = 200;
			r.headers.push_back(HttpHeader("Content-Type", "text/plain"));
			response(r, "Hello");
			done = true;
		}

		virtual void transmit(const iovec* vec, int c) {
			std::cout << "transmit data ---" << std::endl;
			for (int i = 0; i < c; ++i)
				std::cout << string((char*)vec[i].iov_base, (char*)vec[i].iov_base + vec[i].iov_len);
			std::cout << "---" << std::endl;
			msghdr mhdr = { 0, 0, (iovec*)vec, c, 0, 0, 0};
			int send = sendmsg(sock, &mhdr, 0);
			if (send == -1) throw std::runtime_error("Failed to send " + string(strerror(errno)));
		}

		void run() {
			while (!done) {
				char buf[4096];
				int w = ::read(sock, buf, 4096);
				if (w == -1) throw std::runtime_error("Failed to read from string " + string(strerror(errno)));
				std::cout << "received data ---\n" << string(buf, buf + w) << "---" << std::endl;
				for (int o = 0; !done && o < w;)
					o -= feed(buf + o, w - o);
			}
		}

		int sock;
		bool done;
	};

	int getListenSocket() {
		addrinfo * addresses, proto = {};
		proto.ai_family = PF_UNSPEC;
		proto.ai_socktype = SOCK_STREAM;
		proto.ai_flags = AI_PASSIVE;
		int err = getaddrinfo(0, "9080", &proto, &addresses);
		if (err != 0)
			throw std::runtime_error("Failed to get local address: " + string(gai_strerror(err)));

		int sock, yes = 1;
		for (addrinfo * a = addresses;; a = a->ai_next) {
			if (a == 0) throw std::runtime_error("Failed to bind socket");
			sock = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
			if (sock == -1) continue;
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
			if (::bind(sock, a->ai_addr, a->ai_addrlen) == 0 && listen(sock, 1) == 0) break;
			close(sock);
		}

		freeaddrinfo(addresses);
		return sock;
	}

	void serverTest() {
		int sock = getListenSocket();

		std::cout << "listen socket " << sock << std::endl;

		sockaddr_storage peerAddr = {};
		socklen_t peerAddrLen = sizeof(peerAddr);
		std::cout << "peerAddrLen " << peerAddrLen << std::endl;
		int peerSock = accept(sock, (sockaddr*)&peerAddr, &peerAddrLen);
		if (peerSock == -1) throw std::runtime_error("Failed to accept connection " + string(strerror(errno)));

		SyncServerRequest req(peerSock);
		req.run();
	}

}

int main(int ac, char **av) {
	try {
		test::serverTest();
		return 0;
	}
	catch (std::runtime_error& err) {
		std::cerr << err.what() << std::endl;
		return 10;
	}
}
