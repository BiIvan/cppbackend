#pragma once
#include "use_cases.h"
#include "../domain/book.h"
namespace app {
class UseCasesImpl : public UseCases {
    domain::AuthorRepository& authors_;
    domain::BookRepository* books_ = nullptr;
    domain::BookRepository& Books() const;
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors) : authors_(authors) {}
    UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_(authors), books_(&books) {}
    void AddAuthor(const std::string& name) override;
    std::vector<AuthorInfo> GetAuthors() override;
    void AddBook(const std::string& author_id, const std::string& title, int year) override;
    std::vector<BookInfo> GetBooks() override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) override;
    void AddBookWithTags(const std::string&, const std::string&, int,
                         const std::vector<std::string>&) override;
    void AddBookWithNewAuthor(const std::string&, const std::string&, int,
                              const std::vector<std::string>&) override;
    bool DeleteAuthor(const std::string&) override;
    bool EditAuthor(const std::string&, const std::string&) override;
    std::optional<BookInfo> FindBook(const std::string&) override;
    bool DeleteBook(const std::string&) override;
    bool EditBook(const std::string&, const std::optional<std::string>&,
                  const std::optional<int>&, const std::vector<std::string>&) override;
};
}
