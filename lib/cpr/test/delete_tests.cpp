#include <gtest/gtest.h>

#include <string>

#include <cpr.h>

#include "server.h"

using namespace cpr;

static Server* server = new Server();
auto base = server->GetBaseUrl();

TEST(DeleteTests, DeleteTest) {
    auto url = Url{base + "/delete.html"};
    auto response = cpr::Delete(url);
    auto expected_text = std::string{"Delete success"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(200, response.status_code);
}

TEST(DeleteTests, DeleteUnallowedTest) {
    auto url = Url{base + "/delete_unallowed.html"};
    auto response = cpr::Delete(url);
    auto expected_text = std::string{"Method unallowed"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(405, response.status_code);
}

TEST(DeleteTests, SessionDeleteTest) {
    auto url = Url{base + "/delete.html"};
    Session session;
    session.SetUrl(url);
    auto response = session.Delete();
    auto expected_text = std::string{"Delete success"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(200, response.status_code);
}

TEST(DeleteTests, SessionDeleteUnallowedTest) {
    auto url = Url{base + "/delete_unallowed.html"};
    Session session;
    session.SetUrl(url);
    auto response = session.Delete();
    auto expected_text = std::string{"Method unallowed"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(405, response.status_code);
}

TEST(DeleteTests, SessionDeleteAfterGetTest) {
    Session session;
    {
        auto url = Url{base + "/get.html"};
        session.SetUrl(url);
        auto response = session.Get();
    }
    auto url = Url{base + "/delete.html"};
    session.SetUrl(url);
    auto response = session.Delete();
    auto expected_text = std::string{"Delete success"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(200, response.status_code);
}

TEST(DeleteTests, SessionDeleteUnallowedAfterGetTest) {
    Session session;
    {
        auto url = Url{base + "/get.html"};
        session.SetUrl(url);
        auto response = session.Get();
    }
    auto url = Url{base + "/delete_unallowed.html"};
    session.SetUrl(url);
    auto response = session.Delete();
    auto expected_text = std::string{"Method unallowed"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(405, response.status_code);
}

TEST(DeleteTests, SessionDeleteAfterHeadTest) {
    Session session;
    {
        auto url = Url{base + "/get.html"};
        session.SetUrl(url);
        auto response = session.Head();
    }
    auto url = Url{base + "/delete.html"};
    session.SetUrl(url);
    auto response = session.Delete();
    auto expected_text = std::string{"Delete success"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(200, response.status_code);
}

TEST(DeleteTests, SessionDeleteUnallowedAfterHeadTest) {
    Session session;
    {
        auto url = Url{base + "/get.html"};
        session.SetUrl(url);
        auto response = session.Head();
    }
    auto url = Url{base + "/delete_unallowed.html"};
    session.SetUrl(url);
    auto response = session.Delete();
    auto expected_text = std::string{"Method unallowed"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(405, response.status_code);
}

TEST(DeleteTests, SessionDeleteAfterPostTest) {
    Session session;
    {
        auto url = Url{base + "/url_post.html"};
        auto payload = Payload{{"x", "5"}};
        session.SetUrl(url);
        auto response = session.Post();
    }
    auto url = Url{base + "/delete.html"};
    session.SetUrl(url);
    auto response = session.Delete();
    auto expected_text = std::string{"Delete success"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(200, response.status_code);
}

TEST(DeleteTests, SessionDeleteUnallowedAfterPostTest) {
    Session session;
    {
        auto url = Url{base + "/url_post.html"};
        auto payload = Payload{{"x", "5"}};
        session.SetUrl(url);
        auto response = session.Post();
    }
    auto url = Url{base + "/delete_unallowed.html"};
    session.SetUrl(url);
    auto response = session.Delete();
    auto expected_text = std::string{"Method unallowed"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(405, response.status_code);
}

TEST(DeleteTests, AsyncDeleteTest) {
    auto url = Url{base + "/delete.html"};
    auto future_response = cpr::DeleteAsync(url);
    auto response = future_response.get();
    auto expected_text = std::string{"Delete success"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(200, response.status_code);
}

TEST(DeleteTests, AsyncDeleteUnallowedTest) {
    auto url = Url{base + "/delete_unallowed.html"};
    auto future_response = cpr::DeleteAsync(url);
    auto response = future_response.get();
    auto expected_text = std::string{"Method unallowed"};
    EXPECT_EQ(expected_text, response.text);
    EXPECT_EQ(url, response.url);
    EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
    EXPECT_EQ(405, response.status_code);
}

TEST(DeleteTests, AsyncMultipleDeleteTest) {
    auto url = Url{base + "/delete.html"};
    std::vector<AsyncResponse> responses;
    for (int i = 0; i < 10; ++i) {
        responses.emplace_back(cpr::DeleteAsync(url));
    }
    for (auto& future_response : responses) {
        auto response = future_response.get();
        auto expected_text = std::string{"Delete success"};
        EXPECT_EQ(expected_text, response.text);
        EXPECT_EQ(url, response.url);
        EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
        EXPECT_EQ(200, response.status_code);
    }
}

TEST(DeleteTests, AsyncMultipleDeleteUnallowedTest) {
    auto url = Url{base + "/delete_unallowed.html"};
    std::vector<AsyncResponse> responses;
    for (int i = 0; i < 10; ++i) {
        responses.emplace_back(cpr::DeleteAsync(url));
    }
    for (auto& future_response : responses) {
        auto response = future_response.get();
        auto expected_text = std::string{"Method unallowed"};
        EXPECT_EQ(expected_text, response.text);
        EXPECT_EQ(url, response.url);
        EXPECT_EQ(std::string{"text/html"}, response.header["content-type"]);
        EXPECT_EQ(405, response.status_code);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(server);
    return RUN_ALL_TESTS();
}
