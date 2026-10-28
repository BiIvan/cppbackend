#pragma once

#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

#include "../util/tagged_uuid.h"

namespace domain {

  namespace detail {
    struct AuthorTag {};
  }  // namespace detail

  using AuthorId = util::TaggedUUID<detail::AuthorTag>;

  class Author {
    AuthorId id_;
    std::string name_;
    
  public:
    Author(AuthorId id, std::string name)
      : id_(std::move(id))
      , name_(std::move(name)) {
    }

    const AuthorId& GetId() const noexcept {
      return id_;
    }

    const std::string& GetName() const noexcept {
      return name_;
    }
  };

  class AuthorRepository {
  public:
    virtual void Save(const Author& author) = 0;

    // Реализация по умолчанию сохраняет совместимость
    // с существующим MockAuthorRepository.
    virtual std::vector<Author> GetAll() {
      throw std::logic_error("AuthorRepository::GetAll is not implemented");
    }

  protected:
    ~AuthorRepository() = default;
  };

}  // namespace domain