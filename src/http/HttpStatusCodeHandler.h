#ifndef SRC_HTTP_HTTPSTATUSCODEHANDLER_H_
#define SRC_HTTP_HTTPSTATUSCODEHANDLER_H_


class HttpStatusCodeHandler
{
    public:
    virtual void ErrorStatusCode (int statusCode) {};
    virtual ~HttpStatusCodeHandler() {};
};


#endif /* SRC_HTTP_HTTPSTATUSCODEHANDLER_H_ */
