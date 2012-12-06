#ifndef httplib_src_request_h
#define httplib_src_request_h

#include <sys/uio.h>

#include "httplib.h"
#include "parser.h"
#include "header.h"

namespace httplib {

	enum BodyTransferMode {
		BodyTransferIdentity,
		BodyTransferChunked
	};

	size_t bodyBuffers(bool chunked, list<string> &chunkLines, const iovec* vec, int c, Buffers& buffers);

} // namespace httplib

#endif // httplib_src_request_h
