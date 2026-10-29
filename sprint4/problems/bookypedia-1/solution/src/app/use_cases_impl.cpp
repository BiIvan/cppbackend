#include <stdexcept>
#include <boost/algorithm/string/trim.hpp>

#include "use_cases_impl.h"

namespace app {

  void UseCasesImpl::AddAuthor(const std::string& name) {
    auto trimmed_name = boost::algorithm::trim_copy(name);
    if (trimmed_name.empty()) {
      throw std::invalid_argument("Author name must not be empty");
    }
    authors_.Save(
      domain::Author{domain::AuthorId::New(), std::move(trimmed_name)});
  }

  std::vector<AuthorInfo> UseCasesImpl::GetAuthors() {
    const auto authors = authors_.GetAll();
    std::vector<AuthorInfo> result;
    result.reserve(authors.size());
    for (const auto& author : authors) {
      result.push_back({
        author.GetId().ToString(),
        author.GetName(),
      });
    }
    return result;
  }

  void UseCasesImpl::AddBook(const std::string& author_id,
                          const std::string& title,
                          int publication_year) {
    auto trimmed_title = boost::algorithm::trim_copy(title);
    if (trimmed_title.empty()) {
      throw std::invalid_argument("Book title must not be empty");
    }
    GetBookRepository().Save(domain::Book{
      domain::BookId::New(),
      domain::AuthorId::FromString(author_id),
      std::move(trimmed_title),
      publication_year,
    });
  }

  std::vector<BookInfo> UseCasesImpl::GetBooks() {
    const auto books = GetBookRepository().GetAll();
    std::vector<BookInfo> result;
    result.reserve(books.size());
    for (const auto& book : books) {
      result.push_back({
        book.GetTitle(),
        book.GetPublicationYear(),
      });
    }
    return result;
  }

  std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(
    const std::string& author_id) {
    const auto books = GetBookRepository().GetByAuthor(
      domain::AuthorId::FromString(author_id));
    std::vector<BookInfo> result;
    result.reserve(books.size());
    for (const auto& book : books) {
      result.push_back({
        book.GetTitle(),
        book.GetPublicationYear(),
      });
    }
    return result;
  }

  domain::BookRepository& UseCasesImpl::GetBookRepository() const {
    if (books_ == nullptr) {
      throw std::logic_error("Book repository is not configured");
    }
    return *books_;
  }

}  // namespace app