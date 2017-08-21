#include "Utils.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <sstream>

#include "p8-platform/os.h"

#include "client.h"

//Fix an error in libyajl 2.0.1
//These lines can be removed after the upgrade to 2.1.0
#define YAJL_IS_DOUBLE_FIX(v) (YAJL_IS_NUMBER(v) && ((v)->u.number.flags & YAJL_NUMBER_DOUBLE_VALID))
#define YAJL_IS_INTEGER_FIX(v) (YAJL_IS_NUMBER(v) && ((v)->u.number.flags & YAJL_NUMBER_INT_VALID))

std::string Utils::GetFilePath(std::string strPath, bool bUserPath)
{
    return (bUserPath ? g_strUserPath : g_strClientPath) + PATH_SEPARATOR_CHAR + strPath;
}

// http://stackoverflow.com/a/17708801
std::string Utils::UrlEncode(const std::string &value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << '%' << std::setw(2) << int((unsigned char) c);
    }

    return escaped.str();
}

double Utils::StringToDouble(const std::string &value)
{
    std::istringstream iss(value);
    double result;

    iss >> result;

    return result;
}

int Utils::StringToInt(const std::string &value)
{
    return (int)StringToDouble(value);
}

int Utils::GetIntFromJsonValue(yajl_val &value, int defaultValue)
{
    int iTemp = defaultValue;

    // some json responses have have ints formated as strings
    if (YAJL_IS_STRING(value))
        iTemp = StringToInt(YAJL_GET_STRING(value));
    else if (YAJL_IS_INTEGER_FIX(value))
        iTemp = YAJL_GET_INTEGER(value);

    return iTemp;
}

double Utils::GetDoubleFromJsonValue(yajl_val &value, double defaultValue)
{
    double dTemp = defaultValue;

    /* some json responses have have doubles formated as strings,
    or an expected double is formated as an int */
    if (YAJL_IS_STRING(value))
        dTemp = StringToDouble(YAJL_GET_STRING(value));
    else if (YAJL_IS_INTEGER_FIX(value) || YAJL_IS_DOUBLE_FIX(value))
        dTemp = YAJL_GET_DOUBLE(value);

    return dTemp;
}
