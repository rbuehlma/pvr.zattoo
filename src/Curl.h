#include <string>

class Curl
{
public:
  Curl();
  virtual ~Curl();
  virtual std::string Post(std::string url, std::string postData,
      int &statusCode);
private:
  std::string Base64Encode(unsigned char const* in, unsigned int in_len,
      bool urlEncode);
  std::string cookies;
};
