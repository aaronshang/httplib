#ifndef httplib_src_parser_h
#define httplib_src_parser_h

#include "httplib.h"
#include "header.h"

namespace httplib {

	//------------------------------------------------------------------------------------------
	//--

	namespace chartype {

		inline bool isChar(int c) {
			return c >= 0 && c <= 127;
		}

		inline bool isCtl(int c) {
			return (c >= 0 && c <= 31) || c == 127;
		}

		inline bool isTSpecial(int c) {
			return c == '(' || c ==')' || c == '<' || c == '>' || c == '@' || c == ',' || c == ';' ||
				c == ':' || c == '\\' || c == '"' || c == '/' || c == '[' || c == ']' || c == '?' ||
				c == '=' || c == '{' || c == '}' || c == ' ' || c == '\t';
		}

		inline bool isUpper(int c) {
			return c >= 'A' && c <= 'Z';
		}

		inline bool isLower(int c) {
			return c >= 'a' && c <= 'z';
		}

		inline bool isAlpha(int c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
		}

		inline char toLower(int c) {
			return isUpper(c) ? c | 0x20 : c;
		}

		inline bool isDigit(int c) {
			return c >= '0' && c <= '9';
		}

		inline bool isXDigit(int c) {
			return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		}

		inline int hexValue(int c) {
			return c < 'A' ? c - '0' : c < 'a' ? c - 'A' + 10 : c - 'a' + 10;
		}

		inline int hexChar(int c) {
			return c < 10 ? c + '0' : c - 10 + 'A';
		}

		inline bool isSpace(int c) {
			return c == ' ';
		}

		inline bool isWhite(int c) {
			return c == ' ' || c == '\t';
		}

		static inline bool iCaseEqual(int l, int r)
		{
			return isAlpha(l) ? (l | 0x20) == (r | 0x20) : l == r;
		}

	}

	//---------------------------------------------------------------------------------------------------------
	//--

	template <typename Impl> struct ParserBase {
		ParserBase() { clear(); }

		template <typename Arg> const char* parse(const char* b, const char* e, Arg& arg) {
			while (b != e && pstate != Impl::badState && pstate != Impl::endState)
				b = static_cast<Impl&>(*this).parse_some(b, e, arg);
			return b;
		}

		bool isBad() {
			return pstate == Impl::badState;
		}

		bool isDone() {
			return pstate == Impl::endState;
		}

		void clear() {
			pstate = Impl::startState;
		}

	protected :

		const char* parseNewLine(const char* b, const char* e, int next) {
			if (b == e)
				return b;
			if (*b != '\n')
				return pstate = Impl::badState, b;
			return pstate = next, ++b;
		}

		template <typename Int> const char* parse_integer_start(const char* b, const char* e, Int& val, int next) {
			if (b == e)
				return b;
			if (!chartype::isDigit(*b))
				return pstate = Impl::badState, b;
			pstate = next;
			val = 0;
			return b;
		}

		template <typename Int> const char* parse_integer(const char* b, const char* e, Int& val, int next, char end) {
			for (;;) {
				if (b == e)
					return b;
				if (*b == end)
					return pstate = next, ++b;
				if (!chartype::isDigit(*b))
					return pstate = Impl::badState, b;
				val = val * 10 + (*b++ - '0');
			}
		}

		template <typename Int> const char* parse_integer(const char* b, const char* e, Int& val, int next) {
			for (;;) {
				if (b == e)
					return b;
				if (!chartype::isDigit(*b))
					return pstate = next, b;
				val = val * 10 + (*b++ - '0');
			}
		}

		template <typename Int> const char* parse_xinteger_start(const char* b, const char* e, Int& val, int next) {
			if (b == e)
				return b;
			if (!chartype::isXDigit(*b))
				return pstate = Impl::badState, b;
			pstate = next;
			val = 0;
			return b;
		}

		template <typename Int> const char* parse_xinteger(const char* b, const char* e, Int& val, int next) {
			for (;;) {
				if (b == e)
					return b;
				if (!chartype::isXDigit(*b))
					return pstate = next, b;
				val = val * 16 + chartype::hexValue(*b++);
			}
		}

		const char* parse_skip_past(const char* b, const char* e, int next, char match) {
			for (;;) {
				if (b == e)
					return b;
				if (*b++ == match)
					return pstate = next, b;
			}
		}

		int pstate;
	};

	//------------------------------------------------------------------------------------------
	//--

	template <typename Impl> struct HeaderParser : public ParserBase<Impl> {
		const char* parse_header_start(const char* b, const char* e, HttpHeaders& headers,
			int next, int cont, int final) {
			if (b == e)
				return b;
			if (*b == '\r')
				return this->pstate = final, ++b;
			if (chartype::isWhite(*b) && !headers.empty())
				return this->pstate = cont, ++b;
			if (!chartype::isChar(*b) || chartype::isCtl(*b) || chartype::isTSpecial(*b))
				return this->pstate = Impl::badState, b;
			headers.push_back(HttpHeader());
			return this->pstate = next, b;
		}

		const char* parse_header_name(const char* b, const char* e, HttpHeaders& headers, int next) {
			for (;;) {
				if (b == e)
					return b;
				if (*b == ':')
					return this->pstate = next, ++b;
				if (!chartype::isChar(*b) || chartype::isCtl(*b) || chartype::isTSpecial(*b))
					return this->pstate = Impl::badState, b;
				headers.back().name.push_back(*b);
				++b;
			}
		}

		const char* parse_value_start(const char* b, const char* e, int next) {
			for (;;) {
				if (b == e)
					return b;
				if (!chartype::isWhite(*b))
					return this->pstate = next, b;
				++b;
			}
		}

		const char* parse_header_value(const char* b, const char* e, HttpHeaders& headers, int next) {
			string &value = headers.back().value;
			for (;;) {
				if (b == e)
					return b;
				if (*b == '\r') {
					while (!value.empty() && chartype::isWhite(value[value.size() - 1]))
						value.resize(value.size() - 1);
					return this->pstate = next, ++b;
				}
				if (chartype::isCtl(*b))
					return this->pstate = Impl::badState, b;
				value.push_back(*b);
				++b;
			}
		}

		const char* parse_continuation(const char* b, const char* e, HttpHeaders& headers, int next, int blank) {
			string &value = headers.back().value;
			for (;;) {
				if (b == e)
					return b;
				if (*b == '\r')
					return this->pstate = blank, ++b;
				if (!chartype::isWhite(*b)) {
					value.push_back(' ');
					return this->pstate = next, b;
				}
				++b;
			}
		}
	};


	//------------------------------------------------------------------------------------------
	//--

	struct ChunkParser : public ParserBase<ChunkParser> {
		enum ParseState {
			pstate_bad,

			pstate_chunk_start,
			pstate_chunk_size,
			pstate_extension,
			pstate_eol,

			pstate_done,
		};

		static const int badState = pstate_bad;
		static const int endState = pstate_done;
		static const int startState = pstate_chunk_start;

		const char* parse_some(const char* b, const char* e, uint64_t& arg) {
			switch (pstate) {
			case pstate_chunk_start: return this->parse_xinteger_start(b, e, arg, pstate_chunk_size);
			case pstate_chunk_size: return this->parse_xinteger(b, e, arg, pstate_extension);
			case pstate_extension: return this->parse_skip_past(b, e, pstate_eol, '\r');
			case pstate_eol: return parseNewLine(b, e, pstate_done);
			case pstate_done: return b;
			case pstate_bad: return b;
			}
			return this->pstate = pstate_bad, b;
		}
	};

	struct TailParser : public HeaderParser<TailParser> {
		enum ParseState {
			pstate_bad,

			pstate_header_start,
			pstate_header_name,
			pstate_value_start,
			pstate_header_value,
			pstate_continuation,
			pstate_header_eol,
			pstate_final_eol,

			pstate_done,
		};

		static const int badState = pstate_bad;
		static const int endState = pstate_done;
		static const int startState = pstate_header_start;

		const char* parse_some(const char* b, const char* e, HttpHeaders& headers) {
			switch (pstate) {
			case pstate_header_start: return parse_header_start(b, e, headers, pstate_header_name, pstate_continuation, pstate_final_eol);
			case pstate_header_name: return parse_header_name(b, e, headers, pstate_value_start);
			case pstate_value_start: return parse_value_start(b, e, pstate_header_value);
			case pstate_header_value: return parse_header_value(b, e, headers, pstate_header_eol);
			case pstate_continuation: return parse_continuation(b, e, headers, pstate_header_value, pstate_header_eol);
			case pstate_header_eol: return parseNewLine(b, e, pstate_header_start);
			case pstate_final_eol: return parseNewLine(b, e, pstate_done);
			case pstate_done: return b;
			case pstate_bad: return b;
			}
			return this->pstate = pstate_bad, b;
		}
	};

	//------------------------------------------------------------------------------------------
	//--

	struct request_parser : public HeaderParser<request_parser> {
		enum ParseState {
			pstate_bad,

			pstate_method,
			pstate_uri,
			pstate_http_h,
			pstate_http_t1,
			pstate_http_t2,
			pstate_http_p,
			pstate_http_slash,
			pstate_major_start,
			pstate_version_major,
			pstate_minor_start,
			pstate_version_minor,
			pstate_request_eol,
			pstate_header_start,
			pstate_header_name,
			pstate_value_start,
			pstate_header_value,
			pstate_continuation,
			pstate_header_eol,
			pstate_final_eol,

			pstate_done,
		};

		static const int badState = pstate_bad;
		static const int endState = pstate_done;
		static const int startState = pstate_method;

		const char* parse_some(const char* b, const char* e, RequestHeader& request) {
			switch (pstate) {
			case pstate_method: return parse_method(b, e, request);
			case pstate_uri: return parse_uri(b, e, request);
			case pstate_http_h: return parse_http(b, e);
			case pstate_http_t1: return parse_http(b, e);
			case pstate_http_t2: return parse_http(b, e);
			case pstate_http_p: return parse_http(b, e);
			case pstate_http_slash: return parse_http(b, e);
			case pstate_major_start: return parse_integer_start(b, e, request.versionmajor, pstate_version_major);
			case pstate_version_major: return parse_integer(b, e, request.versionmajor, pstate_minor_start, '.');
			case pstate_minor_start: return parse_integer_start(b, e, request.versionminor, pstate_version_minor);
			case pstate_version_minor: return parse_integer(b, e, request.versionminor, pstate_request_eol, '\r');
			case pstate_request_eol: return parseNewLine(b, e, pstate_header_start);
			case pstate_header_start: return parse_header_start(b, e, request.headers, pstate_header_name, pstate_continuation, pstate_final_eol);
			case pstate_header_name: return parse_header_name(b, e, request.headers, pstate_value_start);
			case pstate_value_start: return parse_value_start(b, e, pstate_header_value);
			case pstate_header_value: return parse_header_value(b, e, request.headers, pstate_header_eol);
			case pstate_continuation: return parse_continuation(b, e, request.headers, pstate_header_value, pstate_header_eol);
			case pstate_header_eol: return parseNewLine(b, e, pstate_header_start);
			case pstate_final_eol: return parseNewLine(b, e, pstate_done);
			case pstate_done: return b;
			case pstate_bad: return b;
			}
			return this->pstate = pstate_bad, b;
		}

		const char* parse_method(const char* b, const char* e, RequestHeader& request) {
			for (;;) {
				if (b == e)
					return b;
				if (chartype::isSpace(*b))
					return this->pstate = pstate_uri, ++b;
				if (!chartype::isChar(*b) || chartype::isCtl(*b) || chartype::isTSpecial(*b))
					return this->pstate = pstate_bad, b;
				request.method.push_back(*b++);
			}
		}

		const char* parse_uri(const char* b, const char* e, RequestHeader& request) {
			for (;;) {
				if (b == e)
					return b;
				if (chartype::isCtl(*b))
					return this->pstate = pstate_bad, b;
				if (chartype::isSpace(*b))
					return this->pstate = pstate_http_h, ++b;
				request.uri.push_back(*b++);
			}
		}

		const char* parse_http(const char* b, const char* e) {
			for (;;) {
				if (b == e)
					return b;
				static const char tag[] = { 'H', 'T', 'T', 'P', '/' };
				if (*b != tag[pstate - pstate_http_h])
					return this->pstate = pstate_bad, b;
				if (pstate == pstate_http_slash)
					return this->pstate = pstate_major_start, ++b;
				pstate = ParseState(pstate + 1);
				++b;
			}
		}
	};


	//------------------------------------------------------------------------------------------
	//--

	struct ResponseParser : public HeaderParser<ResponseParser> {
		enum ParseState {
			pstate_bad,

			pstate_http_h,
			pstate_http_t1,
			pstate_http_t2,
			pstate_http_p,
			pstate_http_slash,
			pstate_major_start,
			pstate_version_major,
			pstate_minor_start,
			pstate_version_minor,

			pstate_begin_status,
			pstate_status,

			pstate_reason,
			pstate_status_eol,

			pstate_header_start,
			pstate_header_name,
			pstate_value_start,
			pstate_header_value,
			pstate_continuation,
			pstate_header_eol,
			pstate_final_eol,

			pstate_done,
		};

		static const int badState = pstate_bad;
		static const int endState = pstate_done;
		static const int startState = pstate_http_h;

		const char* parse_some(const char* b, const char* e, ResponseHeader& response) {
			switch (pstate) {
			case pstate_http_h: return parse_http(b, e);
			case pstate_http_t1: return parse_http(b, e);
			case pstate_http_t2: return parse_http(b, e);
			case pstate_http_p: return parse_http(b, e);
			case pstate_http_slash: return parse_http(b, e);
			case pstate_major_start: return parse_integer_start(b, e, response.versionmajor, pstate_version_major);
			case pstate_version_major: return parse_integer(b, e, response.versionmajor, pstate_minor_start, '.');
			case pstate_minor_start: return parse_integer_start(b, e, response.versionminor, pstate_version_minor);
			case pstate_version_minor: return parse_integer(b, e, response.versionminor, pstate_begin_status, ' ');

			case pstate_begin_status: return parse_integer_start(b, e, response.code, pstate_status);
			case pstate_status: return parse_integer(b, e, response.code, pstate_reason, ' ');

			case pstate_reason: return parse_skip_past(b, e, pstate_status_eol, '\r');
			case pstate_status_eol: return parseNewLine(b, e, pstate_header_start);

			case pstate_header_start: return parse_header_start(b, e, response.headers, pstate_header_name, pstate_continuation, pstate_final_eol);
			case pstate_header_name: return parse_header_name(b, e, response.headers, pstate_value_start);
			case pstate_value_start: return parse_value_start(b, e, pstate_header_value);
			case pstate_header_value: return parse_header_value(b, e, response.headers, pstate_header_eol);
			case pstate_continuation: return parse_continuation(b, e, response.headers, pstate_header_value, pstate_header_eol);
			case pstate_header_eol: return parseNewLine(b, e, pstate_header_start);
			case pstate_final_eol: return parseNewLine(b, e, pstate_done);
			case pstate_done: return b;
			case pstate_bad: return b;
			}
			return this->pstate = pstate_bad, b;
		}

		const char* parse_http(const char* b, const char* e) {
			for (;;) {
				if (b == e)
					return b;
				static const char tag[] = { 'H', 'T', 'T', 'P', '/' };
				if (*b != tag[pstate - pstate_http_h])
					return this->pstate = pstate_bad, b;
				if (pstate == pstate_http_slash)
					return this->pstate = pstate_major_start, ++b;
				pstate = ParseState(pstate + 1);
				++b;
			}
		}
	};

	inline bool iCaseEqual(const string& l, const string& r) {
		if (l.length() != r.length()) return false;
		for (size_t i = 0; i < l.length(); ++i)
			if (!chartype::iCaseEqual(l[i], r[i])) return false;
		return true;
	}

	inline bool iCaseEqual(const string &l, const char *r) {
		for (size_t i = 0; i < l.length(); ++i)
			if (r[i] == 0 || !chartype::iCaseEqual(l[i], r[i])) return false;
		return r[l.length()] == 0;
	}

	inline bool iCaseEqual(const char *l, const string &r) {
		return iCaseEqual(r, l);
	}

	template <typename I> I parseInteger(I b, I e, uint64_t & v) {
		I ib(b);
		while (b != e && chartype::isWhite(*b))
			++b;
		if (b == e || !chartype::isDigit(*b)) {
			throw HttpError("invalid integer value: " + string(ib, e));
		}
		else {
			v = 0;
			do v = v * 10 + *b++ - '0'; while (b != e && chartype::isDigit(*b));
		}
		return b;
	}

	template <typename I> inline I skipWhite(I b, I e) {
		while (b != e && (*b == ' ' || *b == '\t'))
			++b;
		return b;
	}

	template <typename I> I skipPastComma(I b, I e) {
		while (b != e && *b++ != ',')
			++b;
		return b;
	}

	template <typename I> bool hasCsvValue(I b, I e, const string& match) {
		for (;;) {
			b = skipWhite(b, e);

			for (typename string::const_iterator i = match.begin();;) {
				if (b == e)
					return i == match.end();

				if (chartype::isCtl(*b) || chartype::isTSpecial(*b)) {
					if (i == match.end())
						return true;
					break;
				}

				if (i == match.end() || !chartype::iCaseEqual(*b++, *i++))
					break;
			}

			b = skipPastComma(b, e);
		}
	}

	double now();
	string decSize(uint64_t size);
	string hexSize(uint64_t size);
	string unescapeString(const string& str);
	string escapeString(const string& str);
	string escapeStringExtra(const string& str, const char*);
	string dateStr(double time = now());

}

#endif // httplib_src_parser_h
