#ifndef httplib_src_client_h
#define httplib_src_client_h

#include <sys/uio.h>

#include "httplib.h"
#include "request.h"
#include "parser.h"
#include "header.h"

namespace httplib {

	struct ClientRequest {

		ClientRequest();

		void clear();

		int feed(const char * b, int s);
		virtual void connect(const RequestHeader& header) {};
		virtual void transmit(const iovec* vec, int c) = 0;

		void request(const RequestHeader& header);
		void send(const iovec* vec, int c);
		void send(const void * b, int s);
		void send(const string& str);
		void finish();

		void request(const RequestHeader& header, const iovec* vec, int c);
		void request(const RequestHeader& header, const char * b, int s);
		void request(const RequestHeader& header, const string& str);

		virtual void continue100() {}
		virtual void response(const ResponseHeader& header) {}
		virtual void recv(const void * b, int s) {}
		virtual void end() {}

		bool shouldClose();

	private :

		enum RequestState {
			SendRequestHeader,
			SendRequestBody,
			RecvResponseHeader,
			RecvResponseBody,
			RecvTailHeaders,
			RequestFinished,
		};

		void beginRequest(const RequestHeader& request, Buffers& buffers, uint64_t knownsize = ~int64_t(0));
		void setupResponseBody();

		bool expect100;
		bool headRequest;
		RequestState state;
		BodyTransferMode transferMode;
		uint64_t transferLeft;

		string resource;
		HttpHeaders extraHeaders;
		list<string> chunkLines;

		ResponseHeader responseHdr;
		ResponseParser responseParser;
		ChunkParser chunkParser;
		TailParser tailParser;
	};

}

#endif // httplib_src_client_h
