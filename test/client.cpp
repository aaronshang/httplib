
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

#include "uri.h"
#include "request.h"

namespace test {
	using namespace httplib;

	struct SyncClientRequest : public ClientRequest {

		SyncClientRequest() { sock = -1; done = false; }
		~SyncClientRequest() { if (sock != -1) close(sock); }

		virtual void connect(const RequestHeader& header) {
			Uri uri(header.uri);
			addrinfo * addresses;
			int err = getaddrinfo(uri.authority.c_str(), uri.scheme.empty() ? "http" : uri.scheme.c_str(), 0, &addresses);
			if (err != 0)
				throw std::runtime_error("Failed to get host '" + uri.authority + "': " + gai_strerror(err));

			for (addrinfo * a = addresses;; a = a->ai_next) {
				if (a == 0) throw std::runtime_error("Failed to connect to host '" + uri.authority + "'");
				sock = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
				if (sock == -1) continue;
				if (::connect(sock, a->ai_addr, a->ai_addrlen) != -1) break;
				close(sock);
			}

			freeaddrinfo(addresses);
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

		void read() {
			while (!done) {
				char buf[4096];
				int w = ::read(sock, buf, 4096);
				if (w == -1) throw std::runtime_error("Failed to read from string " + string(strerror(errno)));
				std::cout << "received data ---\n" << string(buf, buf + w) << "---" << std::endl;
				for (int o = 0; !done && o < w;)
					o -= feed(buf + o, w - o);
			}
		}

		virtual void reponse(const ResponseHeader& header) {
			std::cout << "received response: " << header.code << std::endl;
			std::cout << "---" << std::endl;
			for (HttpHeaders::const_iterator i = header.headers.begin(); i != header.headers.end(); ++i)
				std::cout << i->name << ": " << i->value << std::endl;
			std::cout << "---" << std::endl;
		}

		virtual void recv(const void * b, int s) {
			std::cout << "received body: " << std::endl;
			std::cout << "---" << std::endl;
			std::cout << string((const char*)b, (const char*)b + s) << std::endl;
			std::cout << "---" << std::endl;
		}

		virtual void end() {
			std::cout << "request end" << std::endl;
			done = true;
		}

		int sock;
		bool done;
	};

	void clientTest() {
		RequestHeader hdr;
		hdr.method = "GET";
		hdr.uri = "http://jonspencer.ca/";

		SyncClientRequest req;
		req.request(hdr, string());
		req.read();
	}
}

int main(int ac, char **av) {
	try {
		test::clientTest();
		return 0;
	}
	catch (std::runtime_error& err) {
		std::cerr << err.what() << std::endl;
		return 10;
	}
}
