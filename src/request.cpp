
#include "request.h"
#include "parser.h"
#include "uri.h"

namespace httplib {

	void ClientRequest::beginRequest(const RequestHeader& request, Buffers& buffers, uint64_t knownsize) {
		if (state != SendRequestHeader)
			throw HttpError("Out of order call");

		bool havehost = false;
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

		transferDone = 0;
		if (!havechunked && havelength) {
			transferMode = BodyTransferIdentity;
			transferLength = contentlength;
		}
		else if (!havechunked && knownsize != ~uint64_t(0)) {
			transferMode = BodyTransferIdentity;
			transferLength = knownsize;
		}
		else {
			transferMode = BodyTransferChunked;
			transferLength = 0;
		}

		headRequest = request.method == "HEAD";
		bool knownbodysize = havelength || knownsize != ~uint64_t(0);
		bool knownempty = knownbodysize && transferLength == 0;
		bool putorpost = request.method == "PUT" || request.method == "POST";
		bool needlength = putorpost || (knownbodysize && !knownempty);

		Uri uri(request.uri);

		if (!havehost)
			extraheaders.push_back(HttpHeader("Host", uri.authority));

		if (!haveuseragent)
			extraheaders.push_back(HttpHeader("User-Agent", "httplib 0.1"));

		if (needlength && transferMode == BodyTransferIdentity && !havelength)
			extraheaders.push_back(HttpHeader("Content-Length", decSize(transferLength)));

		if (transferMode == BodyTransferChunked && !havechunked)
			extraheaders.push_back(HttpHeader("Transfer-Encoding", "chunked"));

		uri.scheme.clear();
		uri.authority.clear();
		resource = uri.format();

		buffers.reserve((request.headers.size() + extraheaders.size()) * 4 + 4);
		requestLineBuffers(request.method, resource, buffers);
		toBuffers(extraheaders, buffers);
		toBuffers(request.headers, buffers);
		buffers.push_back(blanklineBuffer());

		state = have100continue ? Expect100Continue : SendRequestBody;
	}

	void ClientRequest::request(const RequestHeader& header) {

	}

} // namespace httplib