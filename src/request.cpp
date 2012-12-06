
#include <iostream>
#include <limits>

#include "request.h"
#include "parser.h"
#include "uri.h"

namespace httplib {

	//--------------------------------------------------------------------------------------------------------------
	//--

	ClientRequest::ClientRequest() {
		clear();
	}

	void ClientRequest::clear() {
		transferLeft = 0;
		expect100 = false;
		headRequest = false;
		state = SendRequestHeader;
		transferMode = BodyTransferIdentity;

		resource.clear();
		extraHeaders.clear();
		chunkLines.clear();

		responseHdr.clear();
		responseParser.clear();
		chunkParser.clear();
		tailParser.clear();
	}

	void ClientRequest::beginRequest(const RequestHeader& request, Buffers& buffers, uint64_t knownsize) {
		if (state != SendRequestHeader)
			throw HttpError("Out of order call");

		bool havehost = false;
		bool havedate = false;
		bool havelength = false;
		bool havechunked = false;
		bool haveidentity = false;
		bool haveencoding = false;
		bool haveuseragent = false;
		bool have100continue = false;
		uint64_t contentlength = 0;
		for (HttpHeaders::const_iterator i = request.headers.begin(); i != request.headers.end(); ++i) {
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
			else if (iCaseEqual(i->name, "User-Agent")) {
				if (haveuseragent)
					throw HttpError("Duplicate User-Agent header");
				haveuseragent = true;
			}
			else if (iCaseEqual(i->name, "Host")) {
				if (havehost)
					throw HttpError("Duplicate Host header");
				havehost = true;
			}
			else if (iCaseEqual(i->name, "Date")) {
				if (havedate)
					throw HttpError("Duplicate Date header");
				havedate = true;
			}
			else if (iCaseEqual(i->name, "Expect")) {
				if (iCaseEqual(i->value, "100-continue"))
					have100continue = true;
			}
		}

		if ((havelength && havechunked) || (havechunked && haveidentity))
			throw HttpError("Inconsistent Transfer-Encoding headers");

		if (haveidentity && knownsize == ~uint64_t(0) && !havelength)
			throw HttpError("Inconsistent Transfer-Encoding headers");

		if (havelength && knownsize != ~uint64_t(0) && contentlength != knownsize)
			throw HttpError("Content-length header doesn't match body size");

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

		Uri uri(request.uri);

		if (!havehost)
			extraHeaders.push_back(HttpHeader("Host", uri.authority));

		if (!haveuseragent)
			extraHeaders.push_back(HttpHeader("User-Agent", "httplib 0.1"));

		if (transferMode == BodyTransferChunked && !havechunked)
			extraHeaders.push_back(HttpHeader("Transfer-Encoding", "chunked"));

		if (!havedate)
			extraHeaders.push_back(HttpHeader("Date", dateStr()));

		if (knownsize != ~uint64_t(0) && transferMode == BodyTransferIdentity && !havelength)
			extraHeaders.push_back(HttpHeader("Content-Length", decSize(transferLeft)));

		uri.scheme.clear();
		uri.authority.clear();
		resource = uri.format();

		buffers.reserve((request.headers.size() + extraHeaders.size()) * 4 + 4);
		requestLineBuffers(request.method, resource, buffers);
		toBuffers(extraHeaders, buffers);
		toBuffers(request.headers, buffers);
		buffers.push_back(blanklineBuffer());

		headRequest = request.method == "HEAD";
		expect100 = have100continue;
	}

	void ClientRequest::setupResponseBody() {
		uint64_t contentlength;
		bool havelength = false;
		bool havechunked = false;
		for (HttpHeaders::const_iterator i = responseHdr.headers.begin(); i != responseHdr.headers.end(); ++i) {
			if (iCaseEqual(i->name, "Transfer-Encoding")) {
				if (!iCaseEqual(i->value, "identity"))
					havechunked = true;
			}
			else if (iCaseEqual(i->name, "Content-Length")) {
				parseInteger(i->value.begin(), i->value.end(), contentlength);
				havelength = true;
			}
		}

		bool mustbeempty = headRequest || responseHdr.code == 204 || responseHdr.code == 304 || responseHdr.code / 100 == 1;
		transferLeft = 0;

		if (mustbeempty) {
			transferMode = BodyTransferIdentity;
		}
		else if (havechunked) {
			transferMode = BodyTransferChunked;
		}
		else {
			transferMode = BodyTransferIdentity;
			transferLeft = havelength ? contentlength : std::numeric_limits<uint64_t>::max();
		}
	}

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

	void ClientRequest::request(const RequestHeader& header) {
		if (state != SendRequestHeader) throw HttpError("can't send request");

		connect(header);
		Buffers buffers;
		state = SendRequestBody;
		beginRequest(header, buffers, ~uint64_t(0));
		transmit(&buffers[0], buffers.size());
		state = SendRequestBody;
	}

	void ClientRequest::request(const RequestHeader& header, const iovec* vec, int c) {
		if (state != SendRequestHeader) throw HttpError("can't send request");

		uint64_t l = 0;
		for (int i = 0; i < c; ++i) l += vec[i].iov_len;

		connect(header);
		Buffers buffers;
		beginRequest(header, buffers, l);
		transferLeft -= bodyBuffers(transferMode == BodyTransferChunked, chunkLines, vec, c, buffers);
		if (transferMode == BodyTransferChunked)
			buffers.push_back(chunkEndBuffer());
		else if (transferLeft != 0)
			throw HttpError("body side doesn't match size header");
		transmit(&buffers[0], buffers.size());
		state = RecvResponseHeader;
	}

	void ClientRequest::request(const RequestHeader& header, const char * b, int s) {
		iovec v = { (void*)b, s };
		request(header, &v, 1);
	}

	void ClientRequest::request(const RequestHeader& header, const string& str) {
		iovec v = { (void*)&str[0], str.size() };
		request(header, &v, 1);
	}

	int ClientRequest::feed(const char *f, int s) {
		const char *b = f;
		const char *e = f + s;
		while (b != e && state == RecvResponseHeader) {
			b = responseParser.parse(b, e, responseHdr);
			if (responseParser.isBad())
				throw HttpError("Invalid response header");

			if (responseParser.isDone()) {
				if (expect100 && responseHdr.code == 100) {
					expect100 = false;
					responseHdr.clear();
					responseParser.clear();
					continue100();
					continue;
				}

				setupResponseBody();
				state = RecvResponseBody;
				response(responseHdr);
			}
		}

		while (state == RecvResponseBody) {
			if (transferMode == BodyTransferIdentity) {
				int l = int(std::min(uint64_t(e - b), transferLeft));
				transferLeft -= l;
				b += l;
				if (l != 0)
					recv(b - l, l);
				if (transferLeft == 0) {
					state = RequestFinished;
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
			b = tailParser.parse(b, e, responseHdr.headers);
			if (tailParser.isDone()) {
				state = RequestFinished;
				end();
			}
		}

		return b - f;
	}


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
