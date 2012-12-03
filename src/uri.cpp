
#include "uri.h"
#include "parser.h"

namespace httplib {

	void Uri::parse(const string& uri) {

		// Check for a scheme.
		string::size_type b = 0;
		scheme.clear();
		string::size_type c = uri.find_first_of(":/?#", b);
		if (c != string::npos && uri[c] == ':') {
			b = c + 1;
			scheme = unescapeString(uri.substr(0, c));
		}

		// Check for an authority.
		authority.clear();
		if (b + 1 < uri.size() && uri[b] == '/' && uri[b + 1] == '/') {
			c = b + 2;
			b = uri.find_first_of("/?#", c);
			authority = unescapeString(uri.substr(c, b - c));
		}

		// extract the path component.
		path.clear();
		if (b < uri.size()) {
			c = b;
			b = uri.find_first_of("?#", c);
			path = unescapeString(uri.substr(c, b - c));
		}
		if (path.empty())
			path = "/";

		// Check for a query
		query.clear();
		if (b < uri.size() && uri[b] == '?') {
			c = b + 1;
			b = uri.find('#', c);
			query = uri.substr(c, b - c);
		}

		// Check for a fragment
		fragment.clear();
		if (b < uri.size() && uri[b] == '#') {
			fragment = uri.substr(b + 1);
		}
	}

	string Uri::format() {
		string r;
		if (!scheme.empty()) {
			r.append(escapeStringExtra(scheme, ":/?#"));
			r.push_back(':');
		}

		if (!authority.empty()) {
			r.append("//");
			r.append(escapeString(authority));
		}

		r.append(escapeString(path));

		if (!query.empty()) {
			r.push_back('?');
			r.append(query);
		}

		if (!fragment.empty()) {
			r.push_back('#');
			r.append(fragment);
		}

		return r;
	}

} // namespace httplib
