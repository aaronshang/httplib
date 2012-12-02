#ifndef httplib_src_header_h
#define httplib_src_header_h

#include "httplib.h"

namespace httplib {

	//---------------------------------------------------------------------------------------------------------
	//--

	struct HttpHeader {
		HttpHeader() {}
		HttpHeader(const string& n, const string& v) : name(n), value(v) {}

		string name;
		string value;
	};

	typedef list<HttpHeader> HttpHeaders;

	string getHeaderValue(const HttpHeaders& headers, const string& tag);


	//---------------------------------------------------------------------------------------------------------
	//--

	struct RequestHeader {
		RequestHeader() : versionmajor(0), versionminor(0) {}

		void clear() {
			uri.clear();
			method.clear();
			versionmajor = 0;
			versionminor = 0;
			headers.clear();
		}

		void swap(RequestHeader& o) {
			uri.swap(o.uri);
			method.swap(o.method);
			std::swap(versionmajor, o.versionmajor);
			std::swap(versionminor, o.versionminor);
			headers.swap(o.headers);
		}

		string uri;
		string method;
		int versionmajor;
		int versionminor;
		HttpHeaders headers;
	};


	//---------------------------------------------------------------------------------------------------------
	//--

	struct ResponseHeader {
		int code;
		int versionmajor;
		int versionminor;
		HttpHeaders headers;

		ResponseHeader() : code(500), versionmajor(0), versionminor(0) {}

		void clear() {
			code = 500;
			versionmajor = 0;
			versionminor = 0;
			headers.clear();
		}

		void swap(ResponseHeader& o) {
			std::swap(code, o.code);
			std::swap(versionmajor, o.versionmajor);
			std::swap(versionminor, o.versionminor);
			headers.swap(o.headers);
		}
	};


	//---------------------------------------------------------------------------------------------------------
	//--

	string decSize(uint64_t size);
	string hexSize(uint64_t size);

	typedef vector<iovec> Buffers;

	void toBuffers(const HttpHeader& o, Buffers& buffers);
	void toBuffers(const list<HttpHeader>& o, Buffers& buffers);
	void toBuffers(const ResponseHeader& o, Buffers& buffers);
	//void reponseBuffers(int code, Buffers& buffers, error_code& err);
	void requestLineBuffers(const string &method, const string &uri, Buffers& buffers);
	iovec blanklineBuffer();

}

#endif // httplib_src_header_h
