#ifndef httplib_src_uri_h
#define httplib_src_uri_h

#include "httplib.h"

namespace httplib {

	//---------------------------------------------------------------------------------------------------------
	//--

	struct Uri {
		Uri() {}
		Uri(const string& uri) { parse(uri); }
		Uri(const string& s, const string& a, const string& p, const string& q, const string& f)
			: scheme(s), authority(a), path(p), query(q), fragment(f) {}

		void clear() {
			scheme.clear();
			authority.clear();
			path.clear();
			query.clear();
			fragment.clear();
		}

		void swap(Uri& o) {
			scheme.swap(o.scheme);
			authority.swap(o.authority);
			path.swap(o.path);
			query.swap(o.query);
			fragment.swap(o.fragment);
		}

		string format();
		void parse(const string& uri);

		string scheme;
		string authority;
		string rawpath;
		string path;
		string query;
		string fragment;
	};

}

#endif // httplib_src_uri_h
