#ifndef httplib_src_server_h
#define httplib_src_server_h

#include <sys/uio.h>

#include "httplib.h"
#include "request.h"
#include "parser.h"
#include "header.h"

namespace httplib {

	struct ServerRequest {

		ServerRequest();

		void clear();

		int feed(const char * b, int s);
		virtual void transmit(const iovec* vec, int c) = 0;

		virtual void request(RequestHeader& header) {}
		virtual void recv(const char * b, int s) {}
		virtual void end() {}

		void continue100();
		void response(const ResponseHeader& header);
		void send(const iovec* vec, int c);
		void send(const void * b, int s);
		void send(const string& str);
		void finish();

		void response(const ResponseHeader& header, const iovec* vec, int c);
		void response(const ResponseHeader& header, const char * b, int s);
		void response(const ResponseHeader& header, const string& str);

		bool shouldClose();

	private :

		enum RequestState {
			RecvRequestHeader,
			RecvRequestBody,
			RecvTailHeaders,
			SendResponseHeader,
			SendResponseBody,
			ResponseFinished,
		};

		void beginResponse(const ResponseHeader& request, Buffers& buffers, uint64_t knownsize);
		void setupRequestBody();

		bool need100;
		bool headRequest;
		RequestState state;
		BodyTransferMode transferMode;
		uint64_t transferLeft;

		HttpHeaders extraHeaders;
		list<string> chunkLines;
		string responseLine;

		RequestHeader requestHdr;
		RequestParser requestParser;
		ChunkParser chunkParser;
		TailParser tailParser;
	};

}

#endif // httplib_src_server_h
