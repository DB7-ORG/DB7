#include "json.hpp"
#include "strong_typedef.hpp"

namespace noisepage::common
{

    template <class Tag, typename IntType>
    nlohmann::json StrongTypeAlias<Tag, IntType>::ToJson() const
    {
        nlohmann::json j = val_;
        return j;
    }

    template <class Tag, typename IntType>
    void StrongTypeAlias<Tag, IntType>::FromJson(const nlohmann::json &j)
    {
        val_ = j.get<IntType>();
    }

}