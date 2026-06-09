#include "transaction/transaction_context.hpp"

namespace db7::transaction
{
    void TransactionContext::Abort()
    {
        rollback_ = true;
    }
}