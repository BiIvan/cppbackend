#pragma once

#include <string>
#include <pqxx/pqxx>

#include "postgres/postgres.h"
#include "app/use_cases_impl.h"

namespace bookypedia {

  struct AppConfig {
    std::string db_url;
  };

  class Application {
    postgres::Database db_;
    app::UseCasesImpl use_cases_{db_.GetAuthors(), db_.GetBooks()};
    
  public:
    explicit Application(const AppConfig& config);

    void Run();
  };

}  // namespace bookypedia