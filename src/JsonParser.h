#include "yajl/yajl_tree.h"
#include <iostream>
#include <stdarg.h>
#include <time.h>

using namespace std;

class JsonParser
{
public:
	static yajl_val parse(string jsonString);
	static bool getBoolean(yajl_val json, int path_len, ...);
	static string getString(yajl_val json, int path_len, ...);
	static time_t getTime(yajl_val json, int path_len, ...);
	static int getInt(yajl_val json, int path_len, ...);
	static yajl_val getArray(yajl_val json, int path_len, ...);
private:
	static const char** getPath(int path_len, va_list args);
};
