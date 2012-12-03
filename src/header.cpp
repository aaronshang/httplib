
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

} // namespace httplib
