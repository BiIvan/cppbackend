#pragma once
#include <pqxx/pqxx>
#include "../domain/book.h"
namespace postgres {
class AuthorRepositoryImpl : public domain::AuthorRepository {
    pqxx::connection& connection_;
public:
    explicit AuthorRepositoryImpl(pqxx::connection& c) : connection_(c) {}
    void Save(const domain::Author&) override;
    std::vector<domain::Author> GetAll() override;
    bool Delete(const domain::AuthorId&) override;
    bool Edit(const domain::AuthorId&, const std::string&) override;
};
class BookRepositoryImpl : public domain::BookRepository {
    pqxx::connection& connection_;
public:
    explicit BookRepositoryImpl(pqxx::connection& c) : connection_(c) {}
    void Save(const domain::Book&) override;
    void SaveWithAuthor(const domain::Book&, const domain::Author&) override;
    std::vector<domain::Book> GetAll() override;
    std::vector<domain::Book> GetByAuthor(const domain::AuthorId&) override;
    std::optional<domain::Book> Find(const domain::BookId&) override;
    bool Delete(const domain::BookId&) override;
    bool Edit(const domain::BookId&, const std::optional<std::string>&,
              const std::optional<int>&, const std::vector<std::string>&) override;
};
class Database {
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
public:
    explicit Database(pqxx::connection connection);
    AuthorRepositoryImpl& GetAuthors() & { return authors_; }
    BookRepositoryImpl& GetBooks() & { return books_; }
};
}
