
#include "header.h"

namespace httplib {

	namespace strings {

		const char space[] = { ' ' };
		const char seperator[] = { ':', ' ' };
		const char lineend[] = { '\r', '\n' };
		const char chunkend[] = { '0', '\r', '\n', '\r', '\n' };
		const char requestend[] = { ' ', 'H', 'T', 'T', 'P', '/', '1', '.', '1', '\r', '\n' };

	} // namespace strings

	template <size_t N> iovec toBuffer(const char (&b)[N]) {
		iovec r = { (void*)b, N };
		return r;
	}

	void toBuffers(const HttpHeader& o, Buffers& buffers) {
		buffers.push_back(toBuffer(o.name));
		buffers.push_back(toBuffer(strings::seperator));
		buffers.push_back(toBuffer(o.value));
		buffers.push_back(toBuffer(strings::lineend));
	}

	void toBuffers(const HttpHeaders& o, Buffers& buffers) {
		for (HttpHeaders::const_iterator i = o.begin(); i != o.end(); ++i)
			toBuffers(*i, buffers);
	}

	void requestLineBuffers(const string& method, const string& resource, Buffers& buffers) {
		buffers.push_back(toBuffer(method));
		buffers.push_back(toBuffer(strings::space));
		buffers.push_back(toBuffer(resource));
		buffers.push_back(toBuffer(strings::requestend));
	}

	iovec blanklineBuffer() {
		return toBuffer(strings::lineend);
	}

	iovec chunkEndBuffer() {
		return toBuffer(strings::chunkend);
	}

	const char *responseCodePhrase(int code) {
		switch (code) {
			case 100 : return "Continue";
			case 101 : return "Switching Protocols";
			case 200 : return "OK";
			case 201 : return "Created";
			case 202 : return "Accepted";
			case 203 : return "Non-Authoritative Information";
			case 204 : return "No Content";
			case 205 : return "Reset Content";
			case 206 : return "Partial Content";
			case 300 : return "Multiple Choices";
			case 301 : return "Moved Permanently";
			case 302 : return "Found";
			case 303 : return "See Other";
			case 304 : return "Not Modified";
			case 305 : return "Use Proxy";
			case 307 : return "Temporary Redirect";
			case 400 : return "Bad Request";
			case 401 : return "Unauthorized";
			case 402 : return "Payment Required";
			case 403 : return "Forbidden";
			case 404 : return "Not Found";
			case 405 : return "Method Not Allowed";
			case 406 : return "Not Acceptable";
			case 407 : return "Proxy Authentication Required";
			case 408 : return "Request Time-out";
			case 409 : return "Conflict";
			case 410 : return "Gone";
			case 411 : return "Length Required";
			case 412 : return "Precondition Failed";
			case 413 : return "Request Entity Too Large";
			case 414 : return "Request-URI Too Large";
			case 415 : return "Unsupported Media Type";
			case 416 : return "Requested range not satisfiable";
			case 417 : return "Expectation Failed";
			case 500 : return "Internal Server Error";
			case 501 : return "Not Implemented";
			case 502 : return "Bad Gateway";
			case 503 : return "Service Unavailable";
			case 504 : return "Gateway Time-out";
			case 505 : return "HTTP Version not supported";
			default : return "Whatever";
		}
	}

} // namespace httplib
