#include "JsonParser.h"

#define DEBUG

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

using namespace std;

yajl_val JsonParser::parse(string jsonString) {
	char errbuf[1024];
	yajl_val json = yajl_tree_parse(jsonString.c_str(), errbuf, sizeof(errbuf));
	if (json == NULL) {
		D(cout  << "Error parsing json:\n" << jsonString << "\nMessage:\n" << errbuf << "\n");
	}
	return json;
}

const char** JsonParser::getPath(int path_len, va_list args) {
	char** path = new char*[path_len + 1];
	for (int i = 0; i < path_len; i++) {
		path[i] = va_arg(args, char*);
	}
	path[path_len] = (char*)0;
	return (const char**)path;
}

bool JsonParser::getBoolean(yajl_val json, int path_len, ...) {
	va_list args;
    va_start(args, path_len);
    const char **path = getPath(path_len, args);
	bool ret = YAJL_IS_TRUE(yajl_tree_get(json, path, yajl_t_true));
	va_end(args);
	delete [] path;
	return ret;
}

string JsonParser::getString(yajl_val json, int path_len, ...) {
	va_list args;
    va_start(args, path_len);
    const char **path = getPath(path_len, args);
	char *str = YAJL_GET_STRING(yajl_tree_get(json, path, yajl_t_string));
	va_end(args);
	delete [] path;
	if (str == NULL) {
		return "";
	}
	return str;
}

time_t JsonParser::getTime(yajl_val json, int path_len, ...) {
  va_list args;
    va_start(args, path_len);
    const char **path = getPath(path_len, args);
  char *str = YAJL_GET_STRING(yajl_tree_get(json, path, yajl_t_string));
  va_end(args);
  delete [] path;
  if (str == NULL) {
    return 0;
  }
  struct tm tm;
  strptime(str, "%Y-%m-%dT%H:%M:%SZ", &tm);
  time_t t = timegm(&tm);
  return t;
}

int JsonParser::getInt(yajl_val json, int path_len, ...) {
	va_list args;
    va_start(args, path_len);
    const char **path = getPath(path_len, args);
	int ret = YAJL_GET_INTEGER(yajl_tree_get(json, path, yajl_t_number));
	va_end(args);
	delete [] path;
	return ret;
}

yajl_val JsonParser::getArray(yajl_val json, int path_len, ...) {
	va_list args;
    va_start(args, path_len);
    const char **path = getPath(path_len, args);
	yajl_val ret = yajl_tree_get(json, path, yajl_t_array);
	va_end(args);
	delete [] path;
	return ret;
}


