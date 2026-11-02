// SPDX-License-Identifier: MPL-2.0
// TANGENTA: Zahlen und Masseinheiten ohne Boost.Spirit (gleiche Grammatik, Punkt als Dezimalzeichen
// unabhängig von der Spracheinstellung)
const char *rvngSkipSpace(const char *p, const char *end)
{
	while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		++p;
	return p;
}

bool findDouble(const librevenge::RVNGString &str, double &res, librevenge::RVNGUnit &unit)
{
	if (str.empty())
		return false;
	const char *p = str.cstr();
	const char *const end = p + str.size();
	p = rvngSkipSpace(p, end);
	if (p < end && *p == '+')
		++p;
	const auto parsed = std::from_chars(p, end, res);
	if (parsed.ec != std::errc())
		return false;
	p = rvngSkipSpace(parsed.ptr, end);
	using namespace librevenge;
	double ratio = 1;
	const std::string rest(p, end);
	auto take = [&](const char *suffix)
	{
		const size_t n = std::strlen(suffix);
		if (rest.compare(0, n, suffix) != 0)
			return false;
		p += n;
		return true;
	};
	if (take("pt"))
		unit = RVNG_POINT;
	else if (take("in"))
		unit = RVNG_INCH;
	else if (take("*"))
		unit = RVNG_TWIP;
	else if (take("%"))
	{
		unit = RVNG_PERCENT;
		ratio = 100.0;
	}
	else if (take("cm"))
	{
		unit = RVNG_INCH;
		ratio = 2.54;
	}
	else if (take("mm"))
	{
		unit = RVNG_INCH;
		ratio = 25.4;
	}
	else
		unit = RVNG_GENERIC;
	res /= ratio;
	return rvngSkipSpace(p, end) == end;
}

bool findInt(const librevenge::RVNGString &str, int &res)
{
	if (str.empty())
		return false;
	const char *p = str.cstr();
	const char *const end = p + str.size();
	p = rvngSkipSpace(p, end);
	if (p < end && *p == '+')
		++p;
	const auto parsed = std::from_chars(p, end, res);
	return parsed.ec == std::errc() && rvngSkipSpace(parsed.ptr, end) == end;
}

bool findBool(const librevenge::RVNGString &str, bool &res)
{
	if (str.empty())
		return false;
	const char *p = str.cstr();
	const char *const end = p + str.size();
	p = rvngSkipSpace(p, end);
	std::string word;
	while (p < end && std::isalpha((unsigned char)*p))
		word += char(std::tolower((unsigned char)*p++));
	if (rvngSkipSpace(p, end) != end)
		return false;
	if (word == "true")
		res = true;
	else if (word == "false")
		res = false;
	else
		return false;
	return true;
}

