#include "../src/ui/view.h"
#include "../src/menu/menu.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace {
void Require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
struct Fake : app::UseCases {
    std::vector<app::AuthorInfo> authors;
    std::vector<app::BookInfo> books;
    int next = 1;
    std::string Id() { return std::to_string(next++); }
    void AddAuthor(const std::string& name) override { authors.push_back({Id(), name}); }
    std::vector<app::AuthorInfo> GetAuthors() override {
        auto result = authors;
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
        return result;
    }
    void AddBook(const std::string& a, const std::string& t, int y) override { AddBookWithTags(a,t,y,{}); }
    void AddBookWithTags(const std::string& a, const std::string& t, int y,
                         const std::vector<std::string>& tags) override {
        auto it = std::find_if(authors.begin(), authors.end(), [&](const auto& v) { return v.id == a; });
        Require(it != authors.end(), "Missing fake author");
        books.push_back({t,y,Id(),a,it->name,tags});
    }
    void AddBookWithNewAuthor(const std::string& a, const std::string& t, int y,
                              const std::vector<std::string>& tags) override {
        AddAuthor(a); AddBookWithTags(authors.back().id,t,y,tags);
    }
    std::vector<app::BookInfo> GetBooks() override { return books; }
    std::vector<app::BookInfo> GetAuthorBooks(const std::string& id) override {
        auto result = books;
        std::erase_if(result, [&](const auto& b) { return b.author_id != id; });
        return result;
    }
    bool DeleteAuthor(const std::string& id) override {
        std::erase_if(books, [&](const auto& b) { return b.author_id == id; });
        return std::erase_if(authors, [&](const auto& a) { return a.id == id; }) != 0;
    }
    bool EditAuthor(const std::string& id, const std::string& name) override {
        for (auto& a : authors) { if (a.id == id) {
            a.name = name;
            for (auto& b : books) { if (b.author_id == id) { b.author_name = name; } }
            return true;
        } }
        return false;
    }
    std::optional<app::BookInfo> FindBook(const std::string& id) override {
        for (const auto& b : books) { if (b.id == id) { return b; } }
        return std::nullopt;
    }
    bool DeleteBook(const std::string& id) override {
        return std::erase_if(books, [&](const auto& b) { return b.id == id; }) != 0;
    }
    bool EditBook(const std::string& id, const std::optional<std::string>& t,
                  const std::optional<int>& y, const std::vector<std::string>& tags) override {
        for (auto& b : books) { if (b.id == id) {
            if (t) { b.title = *t; } if (y) { b.publication_year = *y; } b.tags = tags; return true;
        } }
        return false;
    }
};
std::string Run(Fake& fake, const std::string& commands) {
    std::istringstream input{commands};
    std::ostringstream output;
    menu::Menu menu{input, output};
    menu.AddAction("Exit", {}, {}, [](auto&) { return false; });
    ui::View view{menu, fake, input, output};
    menu.Run();
    return output.str();
}
void TestNormalization() {
    Require(app::NormalizeTags("adventure, dog,   gold   rush  , dog,,dogs") ==
        std::vector<std::string>{"adventure", "dog", "dogs", "gold rush"}, "Tag normalization");
    Require(app::NormalizeTags(" , , ").empty(), "Empty tags");
    Require(app::NormalizeTags("Tag,tag").size() == 2, "Case preservation");
    bool thrown = false;
    try { app::NormalizeTags(std::string(31, 'a')); } catch (const std::invalid_argument&) { thrown = true; }
    Require(thrown, "Tag length limit");
}
void TestAddAndShow() {
    Fake fake;
    auto output = Run(fake, "AddBook 1906 White Fang\nJack London\ny\nadventure, dog, gold   rush, dog\nShowAuthors\nShowBooks\nShowBook White Fang\nExit\n");
    Require(output ==
        "Enter author name or empty line to select from list:\n"
        "No author found. Do you want to add Jack London (y/n)?\n"
        "Enter tags (comma separated):\n"
        "1 Jack London\n"
        "1 White Fang by Jack London, 1906\n"
        "Title: White Fang\nAuthor: Jack London\nPublication year: 1906\n"
        "Tags: adventure, dog, gold rush\n", "Add/show exact output");
    Require(fake.books.size() == 1, "Book inserted");
}
void TestCancelAndSelection() {
    Fake fake;
    auto output = Run(fake, "AddBook 1906 White Fang\nJack London\nn\nExit\n");
    Require(fake.authors.empty() && fake.books.empty(), "Cancelled add has no effects");
    Require(output.ends_with("Failed to add book\n"), "Rejected author error");
    fake.AddAuthor("Jack London");
    output = Run(fake, "AddBook 1906 White Fang\n\n1\n\nShowBook White Fang\nExit\n");
    Require(fake.books.size() == 1, "Select author by index");
    Require(output.find("Tags:") == std::string::npos, "Omit empty tag line");
    Require(Run(fake, "ShowBook Missing\nExit\n").empty(), "Missing book silent");
}
void TestEditDelete() {
    Fake fake;
    fake.AddAuthor("Jack London");
    fake.AddBookWithTags(fake.authors[0].id, "White Fan", 1906, {"cat"});
    Run(fake, "EditBook White Fan\nWhite Fang\n\ndog, adventure\nEditAuthor Jack London\nJohn Chaney\nExit\n");
    Require(fake.books[0].title == "White Fang" && fake.books[0].publication_year == 1906,
            "Edit keeps blank year");
    Require(fake.books[0].tags == std::vector<std::string>{"adventure","dog"}, "Edit replaces tags");
    Require(fake.authors[0].name == "John Chaney", "Rename author");
    Run(fake, "EditBook White Fang\n\n\n\nExit\n");
    Require(fake.books[0].tags.empty(), "Blank edited tags clear tags");
    Run(fake, "DeleteAuthor John Chaney\nExit\n");
    Require(fake.authors.empty() && fake.books.empty(), "Delete selected author");
    Require(Run(fake, "EditBook Missing\nExit\n") == "Book not found\n", "Edit missing book");
}
void TestDuplicateTitle() {
    Fake fake;
    fake.AddAuthor("A"); fake.AddAuthor("B");
    fake.AddBookWithTags(fake.authors[0].id, "Same", 2000, {});
    fake.AddBookWithTags(fake.authors[1].id, "Same", 2001, {});
    auto output = Run(fake, "ShowBook Same\n2\nExit\n");
    Require(output.find("Enter the book # or empty line to cancel:\n") != std::string::npos,
            "Duplicate selection prompt");
    Require(output.ends_with("Title: Same\nAuthor: B\nPublication year: 2001\n"), "Duplicate selection result");
    Run(fake, "DeleteBook Same\n\nExit\n");
    Require(fake.books.size() == 2, "Cancel duplicate delete");
    Run(fake, "DeleteBook\n1\nExit\n");
    Require(fake.books.size() == 1 && fake.books[0].author_name == "B", "Global selection deletes one book");
}
}
int main() {
    try {
        TestNormalization(); TestAddAndShow(); TestCancelAndSelection(); TestEditDelete(); TestDuplicateTitle();
        std::cout << "5 UI/normalization test groups passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
