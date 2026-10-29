#include "use_cases_impl.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <boost/algorithm/string/trim.hpp>
namespace app {
namespace {
std::string Nonempty(const std::string& text) {
    auto value = boost::algorithm::trim_copy(text);
    if (value.empty()) { throw std::invalid_argument("Empty name or title"); }
    return value;
}
BookInfo ToInfo(const domain::Book& b) {
    return {b.GetTitle(), b.GetPublicationYear(), b.GetId().ToString(),
            b.GetAuthorId().ToString(), b.GetAuthorName(), b.GetTags()};
}
std::vector<BookInfo> ToInfos(const std::vector<domain::Book>& books) {
    std::vector<BookInfo> result;
    result.reserve(books.size());
    for (const auto& b : books) { result.push_back(ToInfo(b)); }
    return result;
}
void ValidateTags(const std::vector<std::string>& tags) {
    for (const auto& tag : tags) {
        std::size_t characters = 0;
        for (unsigned char c : tag) { if ((c & 0xc0) != 0x80) { ++characters; } }
        if (characters == 0 || characters > 30) { throw std::invalid_argument("Invalid tag length"); }
    }
}
}
domain::BookRepository& UseCasesImpl::Books() const {
    if (!books_) { throw std::logic_error("Book repository is not configured"); }
    return *books_;
}
void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save(domain::Author{domain::AuthorId::New(), Nonempty(name)});
}
std::vector<AuthorInfo> UseCasesImpl::GetAuthors() {
    std::vector<AuthorInfo> result;
    for (const auto& a : authors_.GetAll()) { result.push_back({a.GetId().ToString(), a.GetName()}); }
    return result;
}
void UseCasesImpl::AddBook(const std::string& author, const std::string& title, int year) {
    AddBookWithTags(author, title, year, {});
}
void UseCasesImpl::AddBookWithTags(const std::string& author, const std::string& title,
                                  int year, const std::vector<std::string>& tags) {
    ValidateTags(tags);
    Books().Save(domain::Book{domain::BookId::New(), domain::AuthorId::FromString(author),
                              Nonempty(title), year, {}, tags});
}
void UseCasesImpl::AddBookWithNewAuthor(const std::string& name, const std::string& title,
                                       int year, const std::vector<std::string>& tags) {
    ValidateTags(tags);
    domain::Author author{domain::AuthorId::New(), Nonempty(name)};
    Books().SaveWithAuthor(domain::Book{domain::BookId::New(), author.GetId(), Nonempty(title),
                                        year, author.GetName(), tags}, author);
}
std::vector<BookInfo> UseCasesImpl::GetBooks() { return ToInfos(Books().GetAll()); }
std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& id) {
    return ToInfos(Books().GetByAuthor(domain::AuthorId::FromString(id)));
}
bool UseCasesImpl::DeleteAuthor(const std::string& id) {
    return authors_.Delete(domain::AuthorId::FromString(id));
}
bool UseCasesImpl::EditAuthor(const std::string& id, const std::string& name) {
    return authors_.Edit(domain::AuthorId::FromString(id), Nonempty(name));
}
std::optional<BookInfo> UseCasesImpl::FindBook(const std::string& id) {
    auto book = Books().Find(domain::BookId::FromString(id));
    if (!book) { return std::nullopt; }
    return ToInfo(*book);
}
bool UseCasesImpl::DeleteBook(const std::string& id) {
    return Books().Delete(domain::BookId::FromString(id));
}
bool UseCasesImpl::EditBook(const std::string& id, const std::optional<std::string>& title,
                           const std::optional<int>& year, const std::vector<std::string>& tags) {
    ValidateTags(tags);
    auto checked_title = title;
    if (checked_title) { checked_title = Nonempty(*checked_title); }
    return Books().Edit(domain::BookId::FromString(id), checked_title, year, tags);
}
}
