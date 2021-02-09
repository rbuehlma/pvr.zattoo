#ifndef SRC_ZATCHANNEL_H_
#define SRC_ZATCHANNEL_H_

#include <string>

struct ZatChannel
{
  int iUniqueId;
  int iChannelNumber;
  bool recordingEnabled;
  std::string name;
  std::string strLogoPath;
  std::string cid;
};

#endif /* SRC_ZATCHANNEL_H_ */
