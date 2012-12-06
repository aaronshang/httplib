
#include <iostream>
#include <limits>

#include "server.h"
#include "parser.h"
#include "uri.h"

namespace httplib {

	//--------------------------------------------------------------------------------------------------------------
	//--

	ServerRequest::ServerRequest() {
		clear();
	}

	void ServerRequest::clear() {
		state = RecvRequestHeader;
		need100 = false;

		requestHdr.clear();
		requestParser.clear();
		chunkParser.clear();
		tailParser.clear();
	}

	void ServerRequest::setupRequestBody() {
		uint64_t contentlength;
		bool havelength = false;
		bool havechunked = false;
		bool have100continue = false;
		for (HttpHeaders::const_iterator i = requestHdr.headers.begin(); i != requestHdr.headers.end(); ++i) {
			if (iCaseEqual(i->name, "Transfer-Encoding")) {
				if (!iCaseEqual(i->value, "identity"))
					havechunked = true;
			}
			else if (iCaseEqual(i->name, "Content-Length")) {
				parseInteger(i->value.begin(), i->value.end(), contentlength);
				havelength = true;
			}
			else if (iCaseEqual(i->name, "Expect")) {
				if (iCaseEqual(i->value, "100-continue"))
					have100continue = true;
			}
		}

		transferLeft = 0;
		transferMode = BodyTransferIdentity;
		if (havechunked)
			transferMode = BodyTransferChunked;
		else if (havelength)
			transferLeft = contentlength;

		headRequest = requestHdr.method == "HEAD";

		need100 = have100continue;
	}

	int ServerRequest::feed(const char * f, int s) {
		const char *b = f;
		const char *e = f + s;
		while (b != e && state == RecvRequestHeader) {
			b = requestParser.parse(b, e, requestHdr);
			if (requestParser.isBad())
				throw HttpError("Invalid request header");

			if (requestParser.isDone()) {
				setupRequestBody();
				state = RecvRequestBody;
				request(requestHdr);
			}
		}

		while (state == RecvRequestBody) {
			if (transferMode == BodyTransferIdentity) {
				int l = int(std::min(uint64_t(e - b), transferLeft));
				transferLeft -= l;
				b += l;
				if (l != 0)
					recv(b - l, l);
				if (transferLeft == 0) {
					state = SendResponseHeader;
					end();
				}
			}
			else if (b != e) {
				if (!chunkParser.isDone()) {
					b = chunkParser.parse(b, e, transferLeft);
					if (!chunkParser.isDone()) break;
					if (transferLeft == 0) {
						state = RecvTailHeaders;
						break;
					}
				}

				int l = int(std::min(uint64_t(e - b), transferLeft));
				transferLeft -= l;
				b += l;
				if (transferLeft == 0)
					chunkParser.clear();
				recv(b - l, l);
			}
			if (b == e)
				break;
		}

		while (b != e && state == RecvTailHeaders) {
			b = tailParser.parse(b, e, requestHdr.headers);
			if (tailParser.isDone()) {
				state = SendResponseHeader;
				end();
			}
		}

		return b - f;
	}

	void ServerRequest::beginResponse(const ResponseHeader& response, Buffers& buffers, uint64_t knownsize) {

		uint64_t contentlength;
		bool havedate = false;
		bool haveserver = false;
		bool havelength = false;
		bool havechunked = false;
		bool haveidentity = false;
		bool haveencoding = false;
		bool haveconnectionclose = false;
		for (HttpHeaders::const_iterator i = response.headers.begin(); i != response.headers.end(); ++i) {
			if (iCaseEqual(i->name, "Transfer-Encoding")) {
				haveencoding = true;
				if (hasCsvValue(i->value.begin(), i->value.end(), "chunked"))
					havechunked = true;
				if (hasCsvValue(i->value.begin(), i->value.end(), "identity"))
					haveidentity = true;
			}
			else if (iCaseEqual(i->name, "Content-Length")) {
				if (havelength)
					throw HttpError("Duplicate Content-Length header");
				parseInteger(i->value.begin(), i->value.end(), contentlength);
				havelength = true;
			}
			else if (iCaseEqual(i->name, "Server")) {
				haveserver = true;
			}
			else if (iCaseEqual(i->name, "Connection")) {
				if (iCaseEqual(i->value, "close"))
					haveconnectionclose = true;
			}
			else if (iCaseEqual(i->name, "Date")) {
				havedate = true;
			}
		}

		bool emptyresponse = response.code == 204 || response.code == 304 || response.code / 100 == 1;

		if ((havelength && havechunked) || (havechunked && haveidentity))
			throw HttpError("Inconsistent Transfer-Encoding headers");

		if (haveidentity && knownsize == ~uint64_t(0) && !havelength)
			throw HttpError("Identity content without known body size");

		if (havelength && knownsize != ~uint64_t(0) && contentlength != knownsize)
			throw HttpError("Content-length header doesn't match body size");

		if (emptyresponse && knownsize != ~uint64_t(0) && knownsize != 0)
			throw HttpError("message body not allowed");

		if (emptyresponse && havechunked)
			throw HttpError("chunked mode not allowed for empty response");

		if (response.code < 100 || response.code >= 1000)
			throw HttpError("invalid resposne code");

		if (!havechunked && havelength) {
			transferMode = BodyTransferIdentity;
			transferLeft = contentlength;
		}
		else if (!havechunked && knownsize != ~uint64_t(0)) {
			transferMode = BodyTransferIdentity;
			transferLeft = knownsize;
		}
		else {
			transferMode = BodyTransferChunked;
			transferLeft = 0;
		}

		if (headRequest) {
			transferLeft = 0;
		}

		if (!haveserver)
			extraHeaders.push_back(HttpHeader("Server", "httplib 0.0"));

		if (transferMode == BodyTransferChunked && !havechunked)
			extraHeaders.push_back(HttpHeader("Transfer-Encoding", "chunked"));

		if (transferMode == BodyTransferIdentity && !haveidentity)
			extraHeaders.push_back(HttpHeader("Transfer-Encoding", "identity"));

		if (transferMode == BodyTransferIdentity && !havelength && !emptyresponse && knownsize != ~uint64_t(0))
			extraHeaders.push_back(HttpHeader("Content-Length", decSize(knownsize)));

		if (!havedate)
			extraHeaders.push_back(HttpHeader("Date", dateStr()));

		responseLine = "HTTP/1.1 " + decSize(response.code) + " " + responseCodePhrase(response.code) + "\r\n";
		buffers.reserve((response.headers.size() + extraHeaders.size()) * 4 + 1);
		buffers.push_back(toBuffer(responseLine));
		toBuffers(extraHeaders, buffers);
		toBuffers(response.headers, buffers);
		buffers.push_back(blanklineBuffer());
	}

	void ServerRequest::response(const ResponseHeader& header) {
		if (state != SendResponseHeader) throw HttpError("can't send response");

		Buffers buffers;
		beginResponse(header, buffers, ~uint64_t(0));
		transmit(&buffers[0], buffers.size());
		state = SendResponseBody;
	}

	void ServerRequest::send(const iovec* vec, int c) {
		Buffers buffers;
		uint64_t l = bodyBuffers(transferMode == BodyTransferChunked, chunkLines, vec, c, buffers);
		if (transferMode == BodyTransferIdentity && l > transferLeft)
			throw HttpError("body longer than specified size");
		transmit(&buffers[0], buffers.size());
	}

	void ServerRequest::send(const void * b, int s) {
		iovec v = { (void*)b, s };
		send(&v, 1);
	}

	void ServerRequest::send(const string& str) {
		iovec v = { (void*)&str[0], str.size() };
		send(&v, 1);
	}

	void ServerRequest::finish() {
		if (transferMode == BodyTransferIdentity) {
			if (transferLeft != 0)
				throw HttpError("body size mismatch");
		}
		else {
			Buffers buffers;
			buffers.push_back(chunkEndBuffer());
			transmit(&buffers[0], buffers.size());
		}
		state = ResponseFinished;
	}

	void ServerRequest::response(const ResponseHeader& header, const iovec* vec, int c) {
		if (state != SendResponseHeader) throw HttpError("can't send response");

		uint64_t l = 0;
		for (int i = 0; i < c; ++i) l += vec[i].iov_len;

		Buffers buffers;
		beginResponse(header, buffers, l);
		if (!headRequest) {
			transferLeft -= bodyBuffers(transferMode == BodyTransferChunked, chunkLines, vec, c, buffers);
			if (transferMode == BodyTransferChunked)
				buffers.push_back(chunkEndBuffer());
			else if (transferLeft != 0)
				throw HttpError("body side doesn't match size header");
		}
		transmit(&buffers[0], buffers.size());
		state = ResponseFinished;
	}

	void ServerRequest::response(const ResponseHeader& header, const char * b, int s) {
		iovec v = { (void*)b, s };
		response(header, &v, 1);
	}

	void ServerRequest::response(const ResponseHeader& header, const string& str) {
		iovec v = { (void*)&str[0], str.size() };
		response(header, &v, 1);
	}

} // namespace httplib
