#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include <cpr.h>

#include "multipart.h"
#include "server.h"

using namespace cpr;

static Server* server = new Server();
auto base = server->GetBaseUrl();


TEST(UrlEncodedPostTests, UrlPostSingleTest) {
    auto url = Url{base + "/url_post.html"};
    auto response = cpr::Post(url, Payload{{"x", "5"}});
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 5\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, UrlPostAddPayloadPair) {
    auto url = Url{base + "/url_post.html"};
    auto payload = Payload{{"x", "1"}};
    payload.AddPair({"y", "2"});
    auto response = cpr::Post(url, Payload(payload));
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 1,\n"
                                     "  \"y\": 2,\n"
                                     "  \"sum\": 3\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, UrlPostPayloadIteratorTest) {
    auto url = Url{base + "/url_post.html"};
    std::vector<Pair> payloadData;
    payloadData.push_back({"x", "1"});
    payloadData.push_back({"y", "2"});
    auto response = cpr::Post(url, Payload(payloadData.begin(), payloadData.end()));
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 1,\n"
                                     "  \"y\": 2,\n"
                                     "  \"sum\": 3\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, UrlPostEncodeTest) {
    auto url = Url{base + "/url_post.html"};
    auto response = cpr::Post(url, Payload{{"x", "hello world!!~"}});
    auto expected_text = std::string{"{\n"
                                     "  \"x\": hello world!!~\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, UrlPostEncodeNoCopyTest) {
    auto url = Url{base + "/url_post.html"};
    auto payload = Payload{{"x", "hello world!!~"}};
    // payload lives through the lifetime of Post, so it doesn't need to be copied
    auto response = cpr::Post(url, payload);
    auto expected_text = std::string{"{\n"
                                     "  \"x\": hello world!!~\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, UrlPostManyTest) {
    auto url = Url{base + "/url_post.html"};
    auto response = cpr::Post(url, Payload{{"x", 5}, {"y", 13}});
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 5,\n"
                                     "  \"y\": 13,\n"
                                     "  \"sum\": 18\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, UrlPostBadHostTest) {
    auto url = Url{"http://bad_host/"};
    auto response = cpr::Post(url, Payload{{"hello", "world"}});
    EXPECT_EQ(std::string{}, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{}, response.header["content-type"]);
    EXPECT_EQ(0, response.status_code);
}

TEST(UrlEncodedPostTests, FormPostSingleTest) {
    auto url = Url{base + "/form_post.html"};
    auto response = cpr::Post(url, Multipart{{"x", 5}});
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 5\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, FormPostFileTest) {
    auto filename = std::string{"test_file"};
    auto content = std::string{"hello world"};
    std::ofstream test_file;
    test_file.open(filename);
    test_file << content;
    test_file.close();
    auto url = Url{base + "/form_post.html"};
    auto response = cpr::Post(url, Multipart{{"x", File{filename}}});
    auto expected_text = std::string{"{\n"
                                     "  \"x\": " + content + "\n"
                                     "}"};
    std::remove(filename.data());
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, FormPostFileNoCopyTest) {
    auto filename = std::string{"test_file"};
    auto content = std::string{"hello world"};
    std::ofstream test_file;
    test_file.open(filename);
    test_file << content;
    test_file.close();
    auto url = Url{base + "/form_post.html"};
    auto multipart = Multipart{{"x", File{filename}}};
    auto response = cpr::Post(url, multipart);
    auto expected_text = std::string{"{\n"
                                     "  \"x\": " + content + "\n"
                                     "}"};
    std::remove(filename.data());
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, FormPostManyTest) {
    auto url = Url{base + "/form_post.html"};
    auto response = cpr::Post(url, Multipart{{"x", 5}, {"y", 13}});
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 5,\n"
                                     "  \"y\": 13,\n"
                                     "  \"sum\": 18\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, FormPostManyNoCopyTest) {
    auto url = Url{base + "/form_post.html"};
    auto multipart = Multipart{{"x", 5}, {"y", 13}};
    auto response = cpr::Post(url, multipart);
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 5,\n"
                                     "  \"y\": 13,\n"
                                     "  \"sum\": 18\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, FormPostContentTypeTest) {
    auto url = Url{base + "/form_post.html"};
    auto response = cpr::Post(url, Multipart{{"x", 5, "application/number"}});
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 5\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, FormPostContentTypeLValueTest) {
    auto url = Url{base + "/form_post.html"};
    auto multipart = Multipart{{"x", 5, "application/number"}};
    auto response = cpr::Post(url, multipart);
    auto expected_text = std::string{"{\n"
                                     "  \"x\": 5\n"
                                     "}"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
    EXPECT_EQ(201, response.status_code);
}

TEST(UrlEncodedPostTests, UrlPostAsyncSingleTest) {
    auto url = Url{base + "/url_post.html"};
    auto payload = Payload{{"x", "5"}};
    std::vector<AsyncResponse> responses;
    for (int i = 0; i < 10; ++i) {
        responses.emplace_back(cpr::PostAsync(url, payload));
    }
    for (auto& future_response : responses) {
        auto response = future_response.get();
        auto expected_text = std::string{"{\n"
                                         "  \"x\": 5\n"
                                         "}"};
        EXPECT_EQ(expected_text, response.text);
        EXPECT_EQ(url, response.url);
        EXPECT_EQ(std::string{"application/json"}, response.header["content-type"]);
        EXPECT_EQ(201, response.status_code);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(server);
    return RUN_ALL_TESTS();
}
