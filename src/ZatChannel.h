#ifndef SRC_ZATCHANNEL_H_
#define SRC_ZATCHANNEL_H_

#include <string>
#include <vector>

struct ZatChannel
{
  int iUniqueId = 0;
  int iChannelNumber = 0;
  bool recordingEnabled = false;
  bool available = true;
  std::vector<std::string> languages; /* language codes reported by the API, if any */
  std::vector<std::pair<std::string, bool>> qualityWithDrm;
  std::string name;
  std::string strLogoPath;
  std::string cid;
};

#endif /* SRC_ZATCHANNEL_H_ */
