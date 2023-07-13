#ifndef SRC_ZATCHANNEL_H_
#define SRC_ZATCHANNEL_H_

#include <string>
#include <vector>

struct ZatChannel
{
  int iUniqueId;
  int iChannelNumber;
  bool recordingEnabled;
  std::vector<std::pair<std::string, bool>> qualityWithDrm;
  std::string name;
  std::string strLogoPath;
  std::string cid;
};

#endif /* SRC_ZATCHANNEL_H_ */
