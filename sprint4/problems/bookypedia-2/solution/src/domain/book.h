#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "author.h"
#include "../util/tagged_uuid.h"

namespace domain {
namespace detail { struct BookTag {}; }
using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
    std::string author_name_;
    std::vector<std::string> tags_;
public:
    Book(BookId id, AuthorId author_id, std::string title, int year,
         std::string author_name = {}, std::vector<std::string> tags = {})
        : id_(std::move(id)), author_id_(std::move(author_id)), title_(std::move(title)),
          publication_year_(year), author_name_(std::move(author_name)), tags_(std::move(tags)) {}
    const BookId& GetId() const noexcept { return id_; }
    const AuthorId& GetAuthorId() const noexcept { return author_id_; }
    const std::string& GetTitle() const noexcept { return title_; }
    int GetPublicationYear() const noexcept { return publication_year_; }
    const std::string& GetAuthorName() const noexcept { return author_name_; }
    const std::vector<std::string>& GetTags() const noexcept { return tags_; }
};
class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual std::vector<Book> GetAll() = 0;
    virtual std::vector<Book> GetByAuthor(const AuthorId& author_id) = 0;
    virtual void SaveWithAuthor(const Book&, const Author&) {
        throw std::logic_error("SaveWithAuthor not implemented");
    }
    virtual std::optional<Book> Find(const BookId&) {
        throw std::logic_error("Find not implemented");
    }
    virtual bool Delete(const BookId&) {
        throw std::logic_error("Delete not implemented");
    }
    virtual bool Edit(const BookId&, const std::optional<std::string>&,
                      const std::optional<int>&, const std::vector<std::string>&) {
        throw std::logic_error("Edit not implemented");
    }
protected:
    ~BookRepository() = default;
};
}
