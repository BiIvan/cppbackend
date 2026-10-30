#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>
#include "../app/use_cases.h"
namespace menu { class Menu; }
namespace ui {
class View {
    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
    bool AddAuthor(std::istream&) const;
    bool AddBook(std::istream&) const;
    bool ShowAuthors() const;
    bool ShowBooks() const;
    bool ShowAuthorBooks() const;
    bool DeleteAuthor(std::istream&) const;
    bool EditAuthor(std::istream&) const;
    bool ShowBook(std::istream&) const;
    bool DeleteBook(std::istream&) const;
    bool EditBook(std::istream&) const;
    std::optional<app::AuthorInfo> SelectAuthor(const std::string& name, bool header) const;
    std::optional<app::BookInfo> SelectBook(const std::string& title) const;
    void PrintAuthors(const std::vector<app::AuthorInfo>&) const;
    void PrintBooks(const std::vector<app::BookInfo>&, bool with_author = true) const;
    void PrintBook(const app::BookInfo&) const;
    std::string ReadLine() const;
public:
    View(menu::Menu&, app::UseCases&, std::istream&, std::ostream&);
};
}
