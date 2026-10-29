#pragma once
#include <optional>
#include <string>
#include <vector>
namespace app {
struct AuthorInfo { std::string id; std::string name; };
struct BookInfo {
    std::string title;
    int publication_year = 0;
    std::string id;
    std::string author_id;
    std::string author_name;
    std::vector<std::string> tags;
};
std::vector<std::string> NormalizeTags(const std::string& text);
class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<AuthorInfo> GetAuthors() = 0;
    virtual void AddBook(const std::string& author_id, const std::string& title, int year) = 0;
    virtual std::vector<BookInfo> GetBooks() = 0;
    virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) = 0;
    virtual void AddBookWithTags(const std::string& author_id, const std::string& title,
                                 int year, const std::vector<std::string>& tags) = 0;
    virtual void AddBookWithNewAuthor(const std::string& author_name, const std::string& title,
                                      int year, const std::vector<std::string>& tags) = 0;
    virtual bool DeleteAuthor(const std::string& id) = 0;
    virtual bool EditAuthor(const std::string& id, const std::string& name) = 0;
    virtual std::optional<BookInfo> FindBook(const std::string& id) = 0;
    virtual bool DeleteBook(const std::string& id) = 0;
    virtual bool EditBook(const std::string& id, const std::optional<std::string>& title,
                          const std::optional<int>& year, const std::vector<std::string>& tags) = 0;
protected:
    ~UseCases() = default;
};
}
