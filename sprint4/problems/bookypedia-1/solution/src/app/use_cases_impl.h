#pragma once

#include "use_cases.h"
#include "../domain/book.h"
#include "../domain/author.h"

namespace app {

  class UseCasesImpl : public UseCases {
  public:
    // Нужен для существующего теста добавления автора.
    explicit UseCasesImpl(domain::AuthorRepository& authors)
      : authors_{authors} {
    }

    UseCasesImpl(domain::AuthorRepository& authors,
                 domain::BookRepository& books)
      : authors_{authors}
      , books_{&books} {
    }

    void AddAuthor(const std::string& name) override;

    std::vector<AuthorInfo> GetAuthors() override;

    void AddBook(const std::string& author_id,
                 const std::string& title,
                 int publication_year) override;

    std::vector<BookInfo> GetBooks() override;

    std::vector<BookInfo> GetAuthorBooks(
      const std::string& author_id) override;

  private:
    domain::BookRepository& GetBookRepository() const;

    domain::AuthorRepository& authors_;
    domain::BookRepository* books_ = nullptr;
  };

}  // namespace app