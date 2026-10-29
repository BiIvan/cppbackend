#include "view.h"

#include <boost/algorithm/string/trim.hpp>

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "../app/use_cases.h"
#include "../menu/menu.h"

namespace ui {
  namespace detail {

    std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
      return out << author.name;
    }

    std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
      return out << book.title << ", " << book.publication_year;
    }
  }  // namespace detail

  namespace {

    template <typename T>
    void PrintVector(std::ostream& out,
                   const std::vector<T>& values,
                   const char* separator = " ") {
      std::size_t index = 1;
      for (const auto& value : values) {
        out << index++ << separator << value << '\n';
      }
    }
  }  // namespace

  View::View(menu::Menu& menu,
           app::UseCases& use_cases,
           std::istream& input,
           std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor", "<name>", "Adds author",
      [this](std::istream& in) { return AddAuthor(in);});
    menu_.AddAction( "AddBook", "<pub_year> <title>", "Adds book",
      [this](std::istream& in) { return AddBook(in);});
    menu_.AddAction( "ShowAuthors", {}, "Show authors",
      [this](std::istream&) { return ShowAuthors();});
    menu_.AddAction( "ShowBooks", {}, "Show books",
      [this](std::istream&) { return ShowBooks();});
    menu_.AddAction( "ShowAuthorBooks", {}, "Show author books",
      [this](std::istream&) { return ShowAuthorBooks();});
  }

  bool View::AddAuthor(std::istream& cmd_input) const {
    try {
      std::string name;
      std::getline(cmd_input, name);
      use_cases_.AddAuthor(name);
    } catch (const std::exception&) {
      output_ << "Failed to add author" << std::endl;
    }
    return true;
  }

  bool View::AddBook(std::istream& cmd_input) const {
    try {
      if (const auto params = GetBookParams(cmd_input)) {
        use_cases_.AddBook(
          params->author_id,
          params->title,
          params->publication_year);
      }
    } catch (const std::exception&) {
      output_ << "Failed to add book" << std::endl;
    }
    return true;
  }

  bool View::ShowAuthors() const {
    PrintVector(output_, GetAuthors(), ". ");
    return true;
  }

  bool View::ShowBooks() const {
    PrintVector(output_, GetBooks());
    return true;
  }

  bool View::ShowAuthorBooks() const {
    try {
      if (const auto author_id = SelectAuthor()) {
        PrintVector(output_, GetAuthorBooks(*author_id));
      }
    } catch (const std::exception&) {
      output_ << "Failed to Show Books" << std::endl;
    }
    return true;
  }

  std::optional<detail::AddBookParams> View::GetBookParams(
    std::istream& cmd_input) const {
    detail::AddBookParams params;
    // Читаем год отдельным токеном, чтобы не принять
    // "1998abc" за год 1998 и начало названия "abc".
    std::string year_token;
    if (!(cmd_input >> year_token)) {
      throw std::invalid_argument("Publication year is required");
    }
    std::istringstream year_input{year_token};
    char extra = '\0';
    if (!(year_input >> params.publication_year) || (year_input >> extra)) {
      throw std::invalid_argument("Invalid publication year");
    }
    std::getline(cmd_input, params.title);
    boost::algorithm::trim(params.title);
    if (params.title.empty()) {
      throw std::invalid_argument("Book title is required");
    }
    const auto author_id = SelectAuthor();
    if (!author_id) {
      return std::nullopt;
    }
    params.author_id = *author_id;
    return params;
  }

  std::optional<std::string> View::SelectAuthor() const {
    const auto authors = GetAuthors();
    output_ << "Select author:\n";
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;
    std::string line;
    if (!std::getline(input_, line)) {
      return std::nullopt;
    }
    boost::algorithm::trim(line);
    if (line.empty()) {
      return std::nullopt;
    }
    std::istringstream number_input{line};
    long long number = 0;
    char extra = '\0';
    if (!(number_input >> number) || (number_input >> extra)) {
      throw std::invalid_argument("Invalid author num");
    }
    if (number <= 0 || static_cast<unsigned long long>(number) > authors.size()) {
      throw std::out_of_range("Invalid author num");
    }
    return authors[static_cast<std::size_t>(number - 1)].id;
  }

  std::vector<detail::AuthorInfo> View::GetAuthors() const {
    const auto authors = use_cases_.GetAuthors();
    std::vector<detail::AuthorInfo> result;
    result.reserve(authors.size());
    for (const auto& author : authors) {
      result.push_back({author.id, author.name});
    }
    return result;
  }

  std::vector<detail::BookInfo> View::GetBooks() const {
    const auto books = use_cases_.GetBooks();
    std::vector<detail::BookInfo> result;
    result.reserve(books.size());
    for (const auto& book : books) {
      result.push_back({book.title, book.publication_year});
    }
    return result;
  }

  std::vector<detail::BookInfo> View::GetAuthorBooks(
    const std::string& author_id) const {
    const auto books = use_cases_.GetAuthorBooks(author_id);
    std::vector<detail::BookInfo> result;
    result.reserve(books.size());
    for (const auto& book : books) {
      result.push_back({book.title, book.publication_year});
    }
    return result;
  }

}  // namespace ui