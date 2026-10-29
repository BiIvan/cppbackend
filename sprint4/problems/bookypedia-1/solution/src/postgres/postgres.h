#pragma once

#include <pqxx/pqxx>

#include "../domain/book.h"
#include "../domain/author.h"

namespace postgres {

  class AuthorRepositoryImpl : public domain::AuthorRepository {
    pqxx::connection& connection_;
    
  public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
      : connection_{connection} {
    }

    void Save(const domain::Author& author) override;

    std::vector<domain::Author> GetAll() override;
  };

  class BookRepositoryImpl : public domain::BookRepository {
    pqxx::connection& connection_;
    
  public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Book& book) override;

    std::vector<domain::Book> GetAll() override;

    std::vector<domain::Book> GetByAuthor(
        const domain::AuthorId& author_id) override;
  };

  class Database {
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
    
  public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & {
      return authors_;
    }

    BookRepositoryImpl& GetBooks() & {
      return books_;
    }
  };

}  // namespace postgres