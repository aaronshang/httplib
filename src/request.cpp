
#include <iostream>
#include <limits>

#include "client.h"
#include "parser.h"
#include "uri.h"

namespace httplib {

	//--------------------------------------------------------------------------------------------------------------
	//--

	size_t bodyBuffers(bool chunked, list<string> &chunkLines, const iovec* vec, int c, Buffers& buffers) {
		size_t rsize = 0;
		for (int i = 0; i < c; ++i)
			rsize += vec[i].iov_len;

		if (rsize != 0) {
			if (chunked) {
				chunkLines.push_back(hexSize(rsize));
				buffers.push_back(toBuffer(chunkLines.back()));
				buffers.push_back(blanklineBuffer());
				buffers.insert(buffers.end(), vec, vec + c);
				buffers.push_back(blanklineBuffer());
			}
			else {
				buffers.insert(buffers.end(), vec, vec + c);
			}
		}

		return rsize;
	}


} // namespace httplib
