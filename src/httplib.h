#ifndef httplib_src_httplib_h
#define httplib_src_httplib_h

#include <stdint.h>

#include <list>
#include <vector>
#include <string>
#include <stdexcept>

namespace httplib {

	using std::string;
	using std::vector;
	using std::list;

	struct HttpError : public std::runtime_error {
		HttpError(const string& msg) : runtime_error(msg) {}
	};

}

#endif // httplib_src_httplib_h
