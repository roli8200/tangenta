// SPDX-License-Identifier: MPL-2.0
// TANGENTA: Base64 ohne Boost.Serialization; Leerraum wird übersprungen, '=' beendet
void convertFromBase64(std::vector<unsigned char> &result, const std::string &source)
{
	unsigned acc = 0;
	int bits = 0;
	for (char ch : source)
	{
		int v;
		if (ch >= 'A' && ch <= 'Z')
			v = ch - 'A';
		else if (ch >= 'a' && ch <= 'z')
			v = ch - 'a' + 26;
		else if (ch >= '0' && ch <= '9')
			v = ch - '0' + 52;
		else if (ch == '+')
			v = 62;
		else if (ch == '/')
			v = 63;
		else if (ch == '=')
			break;
		else
			continue;
		acc = (acc << 6) | unsigned(v);
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			result.push_back((unsigned char)((acc >> bits) & 0xff));
		}
	}
}

void convertToBase64(std::string &result, const std::vector<unsigned char> &source)
{
	static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t i = 0;
	for (; i + 2 < source.size(); i += 3)
	{
		const unsigned n = (unsigned(source[i]) << 16) | (unsigned(source[i + 1]) << 8) | unsigned(source[i + 2]);
		result += table[(n >> 18) & 63];
		result += table[(n >> 12) & 63];
		result += table[(n >> 6) & 63];
		result += table[n & 63];
	}
	if (i < source.size())
	{
		const bool two = i + 1 < source.size();
		const unsigned n = (unsigned(source[i]) << 16) | (two ? unsigned(source[i + 1]) << 8 : 0u);
		result += table[(n >> 18) & 63];
		result += table[(n >> 12) & 63];
		result += two ? table[(n >> 6) & 63] : '=';
		result += '=';
	}
}

