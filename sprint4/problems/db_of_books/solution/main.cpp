#include <string>
#include <cstdlib>
#include <iostream>
#include <exception>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

using Json = nlohmann::json;

void CreateTable(pqxx::connection& connection) {
    pqxx::work transaction{connection};

    transaction.exec(R"(
        CREATE TABLE IF NOT EXISTS books (
            id SERIAL PRIMARY KEY,
            title VARCHAR(100) NOT NULL,
            author VARCHAR(100) NOT NULL,
            year INTEGER NOT NULL,
            ISBN CHAR(13) UNIQUE
        );
    )");

    transaction.commit();
}

bool AddBook(pqxx::connection& connection, const Json& payload) {
    const std::string title = payload.at("title").get<std::string>();
    const std::string author = payload.at("author").get<std::string>();
    const int year = payload.at("year").get<int>();

    std::string isbn;
    const char* isbn_parameter = nullptr;

    if (!payload.at("ISBN").is_null()) {
        isbn = payload.at("ISBN").get<std::string>();
        isbn_parameter = isbn.c_str();
    }

    try {
        pqxx::work transaction{connection};

        transaction.exec_params(
            R"(
                INSERT INTO books (title, author, year, ISBN)
                VALUES ($1, $2, $3, $4);
            )",
            title,
            author,
            year,
            isbn_parameter
        );

        transaction.commit();
        return true;
    } catch (const pqxx::sql_error&) {
        return false;
    }
}

Json GetAllBooks(pqxx::connection& connection) {
    pqxx::read_transaction transaction{connection};

    const pqxx::result rows = transaction.exec(R"(
        SELECT id, title, author, year, ISBN
        FROM books
        ORDER BY
            year DESC,
            title ASC,
            author ASC,
            ISBN ASC NULLS LAST;
    )");

    Json books = Json::array();

    for (const auto& row : rows) {
        Json book = {
            {"id", row["id"].as<int>()},
            {"title", row["title"].as<std::string>()},
            {"author", row["author"].as<std::string>()},
            {"year", row["year"].as<int>()},
            {"ISBN", nullptr}
        };

        if (!row["isbn"].is_null()) {
            book["ISBN"] = row["isbn"].as<std::string>();
        }

        books.push_back(std::move(book));
    }

    transaction.commit();
    return books;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: book_manager <connection-url>\n";
        return EXIT_FAILURE;
    }

    try {
        pqxx::connection connection{argv[1]};

        CreateTable(connection);

        std::string line;

        while (std::getline(std::cin, line)) {
            const Json request = Json::parse(line);
            const std::string action =
                request.at("action").get<std::string>();

            if (action == "exit") {
                break;
            }

            if (action == "add_book") {
                const bool success =
                    AddBook(connection, request.at("payload"));

                std::cout << Json{{"result", success}}.dump()
                          << std::endl;
            } else if (action == "all_books") {
                std::cout << GetAllBooks(connection).dump()
                          << std::endl;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}