#include <string>
#include <map>

class Curl
{
public:
  Curl();
  virtual ~Curl();
  virtual std::string Delete(std::string url, int &statusCode);
  virtual std::string Get(std::string url, int &statusCode);
  virtual std::string Post(std::string url, std::string postData,
      int &statusCode);
  virtual void AddHeader(std::string name, std::string value);
  virtual void AddOption(std::string name, std::string value);
  virtual void ResetHeaders();
  virtual std::string GetCookie(std::string name);
  virtual std::string GetLocation() {
    return location;
  }

private:
  virtual std::string Request(std::string action, std::string url,
      std::string postData, int &statusCode);
  std::string Base64Encode(unsigned char const* in, unsigned int in_len,
      bool urlEncode);
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> options;
  std::map<std::string, std::string> cookies;
  std::string location;
};
