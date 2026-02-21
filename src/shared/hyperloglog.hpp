#pragma once

#include "common.hpp"
#include "bit_utils.hpp"
#include "hash_util.hpp"

#include <xxhash.h>
#include <vector>
#include <cmath>

#define HLL_HASH_SEED 313
static const double pow_2_32 = 4294967296.0;      ///< 2^32
static const double neg_pow_2_32 = -4294967296.0; ///< -(2^32)

class HyperLogLog
{
protected:
    u8 b_;              ///< register bit width
    u32 m_;             ///< register size
    double alphaMM_;    ///< alpha * m^2
    std::vector<u8> M_; ///< registers

public:
    /**
     * Constructor
     *
     * @param[in] b bit width (register size will be 2 to the b power).
     *            This value must be in the range[4,30].Default value is 4.
     *
     * @exception std::invalid_argument the argument is out of range.
     */
    HyperLogLog(u8 b = 4) : b_(b), m_(1 << b), M_(m_, 0)
    {

        if (b < 4 || b > 30)
        {
            throw std::invalid_argument("bit width must be in the range [4,30]");
        }

        double alpha;
        switch (m_)
        {
        case 16:
            alpha = 0.673;
            break;
        case 32:
            alpha = 0.697;
            break;
        case 64:
            alpha = 0.709;
            break;
        default:
            alpha = 0.7213 / (1.0 + 1.079 / m_);
            break;
        }
        alphaMM_ = alpha * m_ * m_;
    }

    /**
     * Adds element to the estimator for general types
     *
     * @param[in] str string to add
     * @param[in] len length of string
     */
    void add(const u8 *str, u32 len)
    {
        u32 hash = XXH32(str, len, HLL_HASH_SEED);
        u32 index = hash >> (32 - b_);
        u8 rank = std::min(32 - b_, (int)std::countl_zero(hash << b_)) + 1;
        if (rank > M_[index])
        {
            M_[index] = rank;
        }
    }

    /**
     * Adds element to the estimator for numbers
     *
     * @param[in] key number to add
     */
    template <typename ValueType>
    void add(const ValueType key)
    {
        // u32 hash = XXH32(key, len, HLL_HASH_SEED);
        u32 hash = XXH32(&key, sizeof(key), HLL_HASH_SEED);
        u32 index = hash >> (32 - b_);
        u8 rank = std::min(32 - b_, (int)std::countl_zero(hash << b_)) + 1;
        if (rank > M_[index])
        {
            M_[index] = rank;
        }
    }

    /**
     * Estimates cardinality value.
     *
     * @return Estimated cardinality value.
     */
    double estimate() const
    {
        double estimate;
        double sum = 0.0;
        for (u32 i = 0; i < m_; i++)
        {
            sum += 1.0 / (1 << M_[i]);
        }
        estimate = alphaMM_ / sum; // E in the original paper
        if (estimate <= 2.5 * m_)
        {
            u32 zeros = 0;
            for (u32 i = 0; i < m_; i++)
            {
                if (M_[i] == 0)
                {
                    zeros++;
                }
            }
            if (zeros != 0)
            {
                estimate = m_ * std::log(static_cast<double>(m_) / zeros);
            }
        }
        else if (estimate > (1.0 / 30.0) * pow_2_32)
        {
            estimate = neg_pow_2_32 * log(1.0 - (estimate / pow_2_32));
        }
        return estimate;
    }
};