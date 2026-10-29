#include "view.h"
#include "../menu/menu.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
namespace ui {
namespace {
std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string::npos) { return {}; }
    const auto last = value.find_last_not_of(" \t\r\n\f\v");
    return value.substr(first, last - first + 1);
}
std::string Tail(std::istream& in) {
    std::string value;
    std::getline(in, value);
    return Trim(std::move(value));
}
int ParseYear(const std::string& value) {
    std::istringstream input{value};
    int year;
    char extra;
    if (!(input >> year) || (input >> extra)) { throw std::invalid_argument("Invalid year"); }
    return year;
}
std::size_t ParseIndex(const std::string& value, std::size_t count) {
    std::istringstream input{value};
    long long index;
    char extra;
    if (!(input >> index) || (input >> extra) || index < 1 ||
        static_cast<unsigned long long>(index) > count) {
        throw std::invalid_argument("Invalid selection");
    }
    return static_cast<std::size_t>(index - 1);
}
std::string JoinTags(const std::vector<std::string>& tags) {
    std::string result;
    for (const auto& tag : tags) {
        if (!result.empty()) { result += ", "; }
        result += tag;
    }
    return result;
}
}
View::View(menu::Menu& menu, app::UseCases& uc, std::istream& input, std::ostream& output)
    : menu_(menu), use_cases_(uc), input_(input), output_(output) {
    menu_.AddAction("AddAuthor", "<name>", "Adds author", [this](auto& in) { return AddAuthor(in); });
    menu_.AddAction("AddBook", "<pub_year> <title>", "Adds book", [this](auto& in) { return AddBook(in); });
    menu_.AddAction("ShowAuthors", {}, "Shows authors", [this](auto&) { return ShowAuthors(); });
    menu_.AddAction("ShowBooks", {}, "Shows books", [this](auto&) { return ShowBooks(); });
    menu_.AddAction("ShowAuthorBooks", {}, "Shows author books", [this](auto&) { return ShowAuthorBooks(); });
    menu_.AddAction("DeleteAuthor", "[name]", "Deletes author", [this](auto& in) { return DeleteAuthor(in); });
    menu_.AddAction("EditAuthor", "[name]", "Edits author", [this](auto& in) { return EditAuthor(in); });
    menu_.AddAction("ShowBook", "[title]", "Shows book", [this](auto& in) { return ShowBook(in); });
    menu_.AddAction("DeleteBook", "[title]", "Deletes book", [this](auto& in) { return DeleteBook(in); });
    menu_.AddAction("EditBook", "[title]", "Edits book", [this](auto& in) { return EditBook(in); });
}
std::string View::ReadLine() const {
    std::string value;
    if (!std::getline(input_, value)) { throw std::runtime_error("End of input"); }
    return Trim(std::move(value));
}
void View::PrintAuthors(const std::vector<app::AuthorInfo>& authors) const {
    std::size_t index = 1;
    for (const auto& author : authors) { output_ << index++ << ' ' << author.name << '\n'; }
    output_.flush();
}
void View::PrintBooks(const std::vector<app::BookInfo>& books, bool with_author) const {
    std::size_t index = 1;
    for (const auto& book : books) {
        output_ << index++ << ' ' << book.title;
        if (with_author) { output_ << " by " << book.author_name; }
        output_ << ", " << book.publication_year << '\n';
    }
    output_.flush();
}
void View::PrintBook(const app::BookInfo& book) const {
    output_ << "Title: " << book.title << '\n'
            << "Author: " << book.author_name << '\n'
            << "Publication year: " << book.publication_year << '\n';
    if (!book.tags.empty()) {
        auto tags = book.tags;
        std::sort(tags.begin(), tags.end());
        output_ << "Tags: " << JoinTags(tags) << '\n';
    }
    output_.flush();
}
std::optional<app::AuthorInfo> View::SelectAuthor(const std::string& name, bool header) const {
    auto authors = use_cases_.GetAuthors();
    if (!name.empty()) {
        for (const auto& author : authors) { if (author.name == name) { return author; } }
        throw std::runtime_error("Author not found");
    }
    if (header) { output_ << "Select author:\n"; }
    PrintAuthors(authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;
    const auto line = ReadLine();
    if (line.empty()) { return std::nullopt; }
    return authors.at(ParseIndex(line, authors.size()));
}
std::optional<app::BookInfo> View::SelectBook(const std::string& title) const {
    auto books = use_cases_.GetBooks();
    if (!title.empty()) {
        std::erase_if(books, [&title](const auto& book) { return book.title != title; });
        if (books.empty()) { throw std::runtime_error("Book not found"); }
        if (books.size() == 1) { return books.front(); }
    }
    PrintBooks(books);
    output_ << "Enter the book # or empty line to cancel:" << std::endl;
    const auto line = ReadLine();
    if (line.empty()) { return std::nullopt; }
    return books.at(ParseIndex(line, books.size()));
}
bool View::AddAuthor(std::istream& in) const {
    try { use_cases_.AddAuthor(Tail(in)); }
    catch (const std::exception&) { output_ << "Failed to add author" << std::endl; }
    return true;
}
bool View::AddBook(std::istream& in) const {
    try {
        std::string token;
        if (!(in >> token)) { throw std::invalid_argument("Year missing"); }
        const int year = ParseYear(token);
        const auto title = Tail(in);
        if (title.empty()) { throw std::invalid_argument("Title missing"); }
        output_ << "Enter author name or empty line to select from list:" << std::endl;
        const auto name = ReadLine();
        std::optional<app::AuthorInfo> author;
        bool create_author = false;
        if (name.empty()) {
            author = SelectAuthor({}, true);
            if (!author) { throw std::runtime_error("Cancelled"); }
        } else {
            const auto authors = use_cases_.GetAuthors();
            for (const auto& item : authors) { if (item.name == name) { author = item; break; } }
            if (!author) {
                output_ << "No author found. Do you want to add " << name << " (y/n)?" << std::endl;
                const auto answer = ReadLine();
                if (answer != "y" && answer != "Y") { throw std::runtime_error("Cancelled"); }
                create_author = true;
            }
        }
        output_ << "Enter tags (comma separated):" << std::endl;
        const auto tags = app::NormalizeTags(ReadLine());
        if (create_author) { use_cases_.AddBookWithNewAuthor(name, title, year, tags); }
        else { use_cases_.AddBookWithTags(author->id, title, year, tags); }
    } catch (const std::exception&) { output_ << "Failed to add book" << std::endl; }
    return true;
}
bool View::ShowAuthors() const { PrintAuthors(use_cases_.GetAuthors()); return true; }
bool View::ShowBooks() const { PrintBooks(use_cases_.GetBooks()); return true; }
bool View::ShowAuthorBooks() const {
    try {
        if (auto author = SelectAuthor({}, true)) {
            PrintBooks(use_cases_.GetAuthorBooks(author->id), false);
        }
    } catch (const std::exception&) { output_ << "Failed to Show Books" << std::endl; }
    return true;
}
bool View::DeleteAuthor(std::istream& in) const {
    try {
        auto author = SelectAuthor(Tail(in), false);
        if (author && !use_cases_.DeleteAuthor(author->id)) { throw std::runtime_error("Deleted"); }
    } catch (const std::exception&) { output_ << "Failed to delete author" << std::endl; }
    return true;
}
bool View::EditAuthor(std::istream& in) const {
    try {
        auto author = SelectAuthor(Tail(in), true);
        if (!author) { return true; }
        output_ << "Enter new name:" << std::endl;
        if (!use_cases_.EditAuthor(author->id, ReadLine())) { throw std::runtime_error("Deleted"); }
    } catch (const std::exception&) { output_ << "Failed to edit author" << std::endl; }
    return true;
}
bool View::ShowBook(std::istream& in) const {
    try {
        if (auto selected = SelectBook(Tail(in))) {
            if (auto book = use_cases_.FindBook(selected->id)) { PrintBook(*book); }
        }
    } catch (const std::exception&) { /* Missing title produces no output. */ }
    return true;
}
bool View::DeleteBook(std::istream& in) const {
    try {
        auto book = SelectBook(Tail(in));
        if (book && !use_cases_.DeleteBook(book->id)) { throw std::runtime_error("Deleted"); }
    } catch (const std::exception&) { output_ << "Failed to delete book" << std::endl; }
    return true;
}
bool View::EditBook(std::istream& in) const {
    try {
        auto selected = SelectBook(Tail(in));
        if (!selected) { return true; }
        auto book = use_cases_.FindBook(selected->id);
        if (!book) { throw std::runtime_error("Deleted"); }
        output_ << "Enter new title or empty line to use the current one (" << book->title << "):" << std::endl;
        auto title_line = ReadLine();
        std::optional<std::string> title;
        if (!title_line.empty()) { title = std::move(title_line); }
        output_ << "Enter publication year or empty line to use the current one ("
                << book->publication_year << "):" << std::endl;
        const auto year_line = ReadLine();
        std::optional<int> year;
        if (!year_line.empty()) { year = ParseYear(year_line); }
        output_ << "Enter tags (current tags: " << JoinTags(book->tags) << "):" << std::endl;
        const auto tags = app::NormalizeTags(ReadLine());
        if (!use_cases_.EditBook(book->id, title, year, tags)) { throw std::runtime_error("Deleted"); }
    } catch (const std::exception&) { output_ << "Book not found" << std::endl; }
    return true;
}
}
