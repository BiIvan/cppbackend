#include "postgres.h"

#include <utility>

namespace postgres {
  namespace {

    std::vector<domain::Book> ReadBooks(const pqxx::result& rows) {
      std::vector<domain::Book> books;
      books.reserve(rows.size());
      for (const auto& row : rows) {
        books.emplace_back(
          domain::BookId::FromString(row["id"].as<std::string>()),
          domain::AuthorId::FromString(row["author_id"].as<std::string>()),
          row["title"].as<std::string>(),
          row["publication_year"].as<int>());
      }
      return books;
    }
  }  // namespace

  void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work transaction{connection_};
    transaction.exec_params(
      R"(
        INSERT INTO authors (id, name)
        VALUES ($1, $2)
        ON CONFLICT (id) DO UPDATE
        SET name = EXCLUDED.name;
      )",
      author.GetId().ToString(),
      author.GetName());
    transaction.commit();
  }

  std::vector<domain::Author> AuthorRepositoryImpl::GetAll() {
    pqxx::read_transaction transaction{connection_};
    const auto rows = transaction.exec(
      R"(
        SELECT id, name
        FROM authors
        ORDER BY name, id;
      )");
    std::vector<domain::Author> authors;
    authors.reserve(rows.size());
    for (const auto& row : rows) {
      authors.emplace_back(
        domain::AuthorId::FromString(row["id"].as<std::string>()),
        row["name"].as<std::string>());
    }
    return authors;
  }

  void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work transaction{connection_};
    transaction.exec_params(
      R"(
        INSERT INTO books (id, author_id, title, publication_year)
        VALUES ($1, $2, $3, $4);
      )",
      book.GetId().ToString(),
      book.GetAuthorId().ToString(),
      book.GetTitle(),
      book.GetPublicationYear());
    transaction.commit();
  }

  std::vector<domain::Book> BookRepositoryImpl::GetAll() {
    pqxx::read_transaction transaction{connection_};
    const auto rows = transaction.exec(
      R"(
        SELECT id, author_id, title, publication_year
        FROM books
        ORDER BY title, publication_year, id;
      )");
    return ReadBooks(rows);
  }

  std::vector<domain::Book> BookRepositoryImpl::GetByAuthor(
    const domain::AuthorId& author_id) {
    pqxx::read_transaction transaction{connection_};
    const auto rows = transaction.exec_params(
      R"(
        SELECT id, author_id, title, publication_year
        FROM books
        WHERE author_id = $1
        ORDER BY publication_year, title, id;
      )",
      author_id.ToString());
    return ReadBooks(rows);
  }

  Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work transaction{connection_};
    transaction.exec(R"(
      CREATE TABLE IF NOT EXISTS authors (
        id UUID PRIMARY KEY,
        name VARCHAR(100) UNIQUE NOT NULL
      );
      CREATE TABLE IF NOT EXISTS books (
        id UUID PRIMARY KEY,
        author_id UUID NOT NULL REFERENCES authors(id),
        title VARCHAR(100) NOT NULL,
        publication_year INTEGER
      );
    )");
    transaction.commit();
  }

}  // namespace postgres