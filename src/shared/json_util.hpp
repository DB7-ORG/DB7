#include "json.hpp"
#include "managed_pointer.hpp"

#include <memory>

namespace noisepage::common
{
    using json = nlohmann::json;

#define DEFINE_JSON_BODY_DECLARATIONS(ClassName)                                          \
    void to_json(nlohmann::json &j, const ClassName &c) { j = c.ToJson(); } /* NOLINT */  \
    void to_json(nlohmann::json &j, const std::unique_ptr<ClassName> c)                   \
    { /* NOLINT */                                                                        \
        if (c != nullptr)                                                                 \
        {                                                                                 \
            j = *c;                                                                       \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            j = nullptr;                                                                  \
        }                                                                                 \
    }                                                                                     \
    void to_json(nlohmann::json &j, common::ManagedPointer<ClassName> c)                  \
    { /* NOLINT */                                                                        \
        if (c != nullptr)                                                                 \
        {                                                                                 \
            j = c->ToJson();                                                              \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            j = nullptr;                                                                  \
        }                                                                                 \
    }                                                                                     \
    void from_json(const nlohmann::json &j, ClassName &c) { c.FromJson(j); } /* NOLINT */ \
    void from_json(const nlohmann::json &j, std::unique_ptr<ClassName> c)                 \
    { /* NOLINT */                                                                        \
        if (c != nullptr)                                                                 \
        {                                                                                 \
            c->FromJson(j);                                                               \
        }                                                                                 \
    }

#define DEFINE_JSON_HEADER_DECLARATIONS(ClassName)                                     \
    void to_json(nlohmann::json &j, const ClassName &c);                  /* NOLINT */ \
    void to_json(nlohmann::json &j, const std::unique_ptr<ClassName> c);  /* NOLINT */ \
    void to_json(nlohmann::json &j, common::ManagedPointer<ClassName> c); /* NOLINT */ \
    void from_json(const nlohmann::json &j, ClassName &c);                /* NOLINT */ \
    void from_json(const nlohmann::json &j, std::unique_ptr<ClassName> c);
}