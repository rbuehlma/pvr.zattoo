#ifndef SRC_SQL_PARAMETERDB_H_
#define SRC_SQL_PARAMETERDB_H_

#include "SQLConnection.h"

class ParameterDB : public SQLConnection
{
public:
  ParameterDB(std::string folder);
  ~ParameterDB();
  bool Set(std::string key, std::string value);
  std::string Get(std::string key);
private:
  bool MigrateDbIfRequired();
  bool Migrate0To1();
};

#endif /* SRC_SQL_PARAMETERDB_H_ */
