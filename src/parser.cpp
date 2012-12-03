
#include <string.h>
#include <sys/time.h>

#include "parser.h"

namespace httplib {

	string unescapeString(const string& str) {
		string r;
		r.reserve(str.size());
		for (string::const_iterator i = str.begin(); i != str.end(); ++i) {
			if (*i == '%') {
				if (++i == str.end() || !chartype::isXDigit(*i))
					throw HttpError("invalid escape sequence in uri " + str);
				int v = chartype::hexValue(*i) * 16;
				if (++i == str.end() || !chartype::isXDigit(*i))
					throw HttpError("invalid escape sequence in uri " + str);
				r.push_back(char(v + chartype::hexValue(*i)));
			}
			else {
				r.push_back(*i);
			}
		}
		return r;
	}

	string escapeStringExtra(const string& src, const char* extra) {
		string r;
		r.reserve(src.size() * 9 / 8);
		for (string::const_iterator i = src.begin(); i != src.end(); ++i) {
			if (!chartype::isChar(*i) || chartype::isCtl(*i) || chartype::isWhite(*i) || strchr(extra, *i) != 0) {
				r.push_back('%');
				r.push_back(chartype::hexChar(static_cast<unsigned char>(*i) >> 4));
				r.push_back(chartype::hexChar(static_cast<unsigned char>(*i) & 15));
			}
			else {
				r.push_back(*i);
			}
		}
		return r;
	}

	string escapeString(const string& src) {
		return escapeStringExtra(src, ":?#");
	}

	string hexSize(uint64_t size) {
		string r;
		int i = 15;
		while (i > 0 && (size & (uint64_t(0xff) << 56)) == 0)
			i -= 2, size <<= 8;
		while (i >= 0)
			r.append(1, chartype::hexChar((size >> 60) & 0x0f)), --i, size <<= 4;
		return r;
	}

	string decSize(uint64_t size) {
		string r;
		while (size != 0)
			r.insert(r.begin(), char(size % 10) + '0'), size /= 10;
		if (r.empty())
			r.append(1, '0');
		return r;
	}

	double now() {
		timeval tv;
		gettimeofday(&tv, 0);
		return tv.tv_sec + 1e-6 * tv.tv_usec;
	}

	const char * days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	const char * months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	string digits2(unsigned u) {
		char c[3] = { (u / 10) + '0', (u % 10) + '0', 0 };
		return c;
	}

	string digits4(unsigned u) {
		char c[5] = { (u / 1000) + '0', (u / 100) % 10 + '0', (u / 10) % 10 + '0', (u % 10) + '0', 0 };
		return c;
	}

	string dateStr(double time) {
		struct tm t;
		time_t tt = int(time);
		gmtime_r(&tt, &t);
		string r = days[t.tm_wday];
		r += ", " + digits2(t.tm_mday) + " " + months[t.tm_mon] + " " + digits4(t.tm_year + 1900);
		r += " " + digits2(t.tm_hour) + ":" + digits2(t.tm_min) + ":" + digits2(t.tm_sec) + " GMT";
		return r;
	}

}
