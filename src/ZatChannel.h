#ifndef SRC_ZATCHANNEL_H_
#define SRC_ZATCHANNEL_H_

struct ZatChannel
{
  int iUniqueId;
  int iChannelNumber;
  int selectiveRecallSeconds;
  bool recordingEnabled;
  std::string name;
  std::string strLogoPath;
  std::string cid;
};

#endif /* SRC_ZATCHANNEL_H_ */
