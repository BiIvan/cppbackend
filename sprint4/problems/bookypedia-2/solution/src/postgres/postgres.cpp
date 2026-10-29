#include "postgres.h"
#include <utility>
namespace postgres {
namespace {
void InsertTags(pqxx::work& tx, const std::string& id, const std::vector<std::string>& tags) {
    for (const auto& tag : tags) {
        tx.exec_params("INSERT INTO book_tags(book_id, tag) VALUES ($1, $2)", id, tag);
    }
}
void InsertBook(pqxx::work& tx, const domain::Book& b, const std::string& author_id) {
    tx.exec_params("INSERT INTO books(id, author_id, title, publication_year) VALUES ($1,$2,$3,$4)",
                   b.GetId().ToString(), author_id, b.GetTitle(), b.GetPublicationYear());
    InsertTags(tx, b.GetId().ToString(), b.GetTags());
}
std::vector<domain::Book> ReadBooks(pqxx::transaction_base& tx, const pqxx::result& rows) {
    std::vector<domain::Book> books;
    for (const auto& row : rows) {
        const auto id = row["id"].as<std::string>();
        const auto tag_rows = tx.exec_params(
            "SELECT DISTINCT tag FROM book_tags WHERE book_id=$1 ORDER BY tag", id);
        std::vector<std::string> tags;
        for (const auto& t : tag_rows) { tags.push_back(t[0].as<std::string>()); }
        books.emplace_back(domain::BookId::FromString(id),
            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
            row["title"].as<std::string>(), row["publication_year"].as<int>(),
            row["author_name"].as<std::string>(), std::move(tags));
    }
    return books;
}
constexpr auto kColumns = "SELECT b.id, b.author_id, b.title, b.publication_year, a.name AS author_name "
                          "FROM books b JOIN authors a ON a.id=b.author_id ";
constexpr auto kOrder = " ORDER BY b.title, a.name, b.publication_year, b.id";
}
void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work tx{connection_};
    tx.exec_params("INSERT INTO authors(id,name) VALUES ($1,$2) "
                   "ON CONFLICT(id) DO UPDATE SET name=EXCLUDED.name",
                   author.GetId().ToString(), author.GetName());
    tx.commit();
}
std::vector<domain::Author> AuthorRepositoryImpl::GetAll() {
    pqxx::read_transaction tx{connection_};
    auto rows = tx.exec("SELECT id,name FROM authors ORDER BY name,id");
    std::vector<domain::Author> result;
    for (const auto& row : rows) {
        result.emplace_back(domain::AuthorId::FromString(row[0].as<std::string>()), row[1].as<std::string>());
    }
    return result;
}
bool AuthorRepositoryImpl::Delete(const domain::AuthorId& author_id) {
    pqxx::work tx{connection_};
    const auto id = author_id.ToString();
    if (tx.exec_params("SELECT id FROM authors WHERE id=$1 FOR UPDATE", id).empty()) { return false; }
    tx.exec_params("DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id=$1)", id);
    tx.exec_params("DELETE FROM books WHERE author_id=$1", id);
    tx.exec_params("DELETE FROM authors WHERE id=$1", id);
    tx.commit();
    return true;
}
bool AuthorRepositoryImpl::Edit(const domain::AuthorId& id, const std::string& name) {
    pqxx::work tx{connection_};
    auto result = tx.exec_params("UPDATE authors SET name=$2 WHERE id=$1", id.ToString(), name);
    tx.commit();
    return result.affected_rows() == 1;
}
void BookRepositoryImpl::Save(const domain::Book& b) {
    pqxx::work tx{connection_};
    InsertBook(tx, b, b.GetAuthorId().ToString());
    tx.commit();
}
void BookRepositoryImpl::SaveWithAuthor(const domain::Book& b, const domain::Author& a) {
    pqxx::work tx{connection_};
    auto rows = tx.exec_params("INSERT INTO authors(id,name) VALUES ($1,$2) "
        "ON CONFLICT(name) DO UPDATE SET name=EXCLUDED.name RETURNING id", a.GetId().ToString(), a.GetName());
    InsertBook(tx, b, rows[0][0].as<std::string>());
    tx.commit();
}
std::vector<domain::Book> BookRepositoryImpl::GetAll() {
    pqxx::read_transaction tx{connection_};
    tx.exec("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    return ReadBooks(tx, tx.exec(std::string{kColumns} + kOrder));
}
std::vector<domain::Book> BookRepositoryImpl::GetByAuthor(const domain::AuthorId& id) {
    pqxx::read_transaction tx{connection_};
    tx.exec("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    auto rows = tx.exec_params(std::string{kColumns} +
        " WHERE b.author_id=$1 ORDER BY b.publication_year,b.title,b.id", id.ToString());
    return ReadBooks(tx, rows);
}
std::optional<domain::Book> BookRepositoryImpl::Find(const domain::BookId& id) {
    pqxx::read_transaction tx{connection_};
    tx.exec("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    auto rows = tx.exec_params(std::string{kColumns} + " WHERE b.id=$1", id.ToString());
    if (rows.empty()) { return std::nullopt; }
    return ReadBooks(tx, rows).front();
}
bool BookRepositoryImpl::Delete(const domain::BookId& book_id) {
    pqxx::work tx{connection_};
    auto id = book_id.ToString();
    if (tx.exec_params("SELECT id FROM books WHERE id=$1 FOR UPDATE", id).empty()) { return false; }
    tx.exec_params("DELETE FROM book_tags WHERE book_id=$1", id);
    tx.exec_params("DELETE FROM books WHERE id=$1", id);
    tx.commit();
    return true;
}
bool BookRepositoryImpl::Edit(const domain::BookId& book_id, const std::optional<std::string>& title,
                               const std::optional<int>& year, const std::vector<std::string>& tags) {
    pqxx::work tx{connection_};
    const auto id = book_id.ToString();
    auto current = tx.exec_params("SELECT title,publication_year FROM books WHERE id=$1 FOR UPDATE", id);
    if (current.empty()) { return false; }
    const auto new_title = title ? *title : current[0][0].as<std::string>();
    const auto new_year = year ? *year : current[0][1].as<int>();
    tx.exec_params("UPDATE books SET title=$2,publication_year=$3 WHERE id=$1", id, new_title, new_year);
    tx.exec_params("DELETE FROM book_tags WHERE book_id=$1", id);
    InsertTags(tx, id, tags);
    tx.commit();
    return true;
}
Database::Database(pqxx::connection connection) : connection_(std::move(connection)) {
    pqxx::work tx{connection_};
    tx.exec(R"(
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
        CREATE TABLE IF NOT EXISTS book_tags (
            book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
            tag VARCHAR(30) NOT NULL
        );
        CREATE INDEX IF NOT EXISTS bookypedia2_books_author_idx ON books(author_id);
        CREATE INDEX IF NOT EXISTS bookypedia2_book_tags_book_idx ON book_tags(book_id);
    )");
    tx.commit();
}
}
