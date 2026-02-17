#pragma once

#include "common.hpp"
#include "helper_utils.hpp"
#include <cassert>

template <typename V>
struct TemplatedValidityData
{
    static constexpr u64 BITS_PER_VALUE = sizeof(V) * 8;
    static constexpr V MAX_ENTRY = V(~V(0));

    std::unique_ptr<V[]> owned_data;

    inline explicit TemplatedValidityData(u64 count)
    {
        auto entry_count = EntryCount(count);
        owned_data = make_unique_array_uninitialized<V>(entry_count);
        for (u64 i = 0; i < entry_count; i++)
        {
            owned_data[i] = MAX_ENTRY;
        }
    }

    inline TemplatedValidityData(const V *validity_mask, u64 count)
    {
        auto entry_count = EntryCount(count);
        owned_data = make_unique_array_uninitialized<V>(entry_count);
        for (u64 i = 0; i < entry_count; i++)
        {
            owned_data[i] = validity_mask[i];
        }
    }

    static inline u64 EntryCount(u64 count)
    {
        return (count + (BITS_PER_VALUE - 1)) / BITS_PER_VALUE;
    }
};

template <typename V>
struct TemplatedValidityMask
{
    using ValidityBuffer = TemplatedValidityData<V>;

protected:
    static constexpr u64 BITS_PER_VALUE = ValidityBuffer::BITS_PER_VALUE;
    static constexpr u64 STANDARD_ENTRY_COUNT = (STANDARD_VECTOR_SIZE + (BITS_PER_VALUE - 1)) / BITS_PER_VALUE;
    static constexpr u64 STANDARD_MASK_SIZE = STANDARD_ENTRY_COUNT * sizeof(V);

    V *validity_mask;
    std::shared_ptr<ValidityBuffer> validity_data;
    u64 capacity;

public:
    inline TemplatedValidityMask() : validity_mask(nullptr), capacity(STANDARD_VECTOR_SIZE) {}
    inline explicit TemplatedValidityMask(u64 target_count) : validity_mask(nullptr), capacity(target_count) {}
    inline explicit TemplatedValidityMask(V *ptr, u64 capacity) : validity_mask(ptr), capacity(capacity) {}
    inline TemplatedValidityMask(const TemplatedValidityMask &original, u64 count)
    {
        Copy(original, count);
    }

    inline void Initialize(u64 count)
    {
        capacity = count;
        validity_data = make_buffer<ValidityBuffer>(count);
        validity_mask = validity_data->owned_data.get();
    }

    inline void Initialize()
    {
        Initialize(capacity);
    }

    static inline u64 ValidityMaskSize(u64 count = STANDARD_VECTOR_SIZE)
    {
        return ValidityBuffer::EntryCount(count) * sizeof(V);
    }

    static inline u64 EntryCount(u64 count)
    {
        return ValidityBuffer::EntryCount(count);
    }

    inline bool AllValid() const
    {
        return !validity_mask;
    }

    static inline bool AllValid(V entry)
    {
        return entry == ValidityBuffer::MAX_ENTRY;
    }

    inline bool CheckAllValid(u64 count) const
    {
        return CountValid(count) == count;
    }

    inline bool CheckAllInvalid(u64 count) const
    {
        return CountValid(count) == 0;
    }

    inline bool CheckAllValid(u64 to, u64 from) const
    {
        if (AllValid())
        {
            return true;
        }
        for (u64 i = from; i < to; i++)
        {
            if (!RowIsValid(i))
            {
                return false;
            }
        }
        return true;
    }

    static inline bool RowIsValid(const V &entry, const u64 &idx_in_entry)
    {
        return entry & (V(1) << V(idx_in_entry));
    }

    inline bool RowIsValid(u64 row_idx) const
    {
        u64 entry_idx = row_idx / BITS_PER_VALUE;
        u64 idx_in_entry = row_idx % BITS_PER_VALUE;
        auto entry = validity_mask[entry_idx];
        return RowIsValid(entry, idx_in_entry);
    }

    u64 CountValid(const u64 count) const
    {
        if (AllValid() || count == 0)
        {
            return count;
        }

        u64 valid = 0;
        const auto entry_count = EntryCount(count);
        for (u64 entry_idx = 0; entry_idx < entry_count;)
        {
            auto entry = GetValidityEntry(entry_idx++);
            // Handle ragged end (if not exactly multiple of BITS_PER_VALUE)
            if (entry_idx == entry_count && count % BITS_PER_VALUE != 0)
            {
                const auto shift = BITS_PER_VALUE - (count % BITS_PER_VALUE);
                const auto mask = ValidityBuffer::MAX_ENTRY >> shift;
                entry &= mask;
            }
            else if (AllValid(entry))
            {
                // Handle all set
                valid += BITS_PER_VALUE;
                continue;
            }

            // Count partial entry (Kernighan's algorithm)
            while (entry)
            {
                entry &= (entry - 1);
                ++valid;
            }
        }

        return valid;
    }

    inline V &GetValidityEntryUnsafe(u64 entry_idx) const
    {
        assert(entry_idx <= capacity);
        return validity_mask[entry_idx];
    }

    inline V GetValidityEntry(u64 entry_idx) const
    {
        if (!validity_mask)
        {
            return ValidityBuffer::MAX_ENTRY;
        }
        return GetValidityEntryUnsafe(entry_idx);
    }

    inline void SetValidUnsafe(u64 row_idx)
    {
        assert(validity_mask != nullptr);
        assert(row_idx <= capacity);
        u64 entry_idx, idx_in_entry;
        entry_idx = row_idx / BITS_PER_VALUE;
        idx_in_entry = row_idx % BITS_PER_VALUE;
        validity_mask[entry_idx] |= (V(1) << V(idx_in_entry));
    }

    inline void SetValid(u64 row_idx)
    {
        if (!validity_mask)
        {
            return;
        }
        SetValidUnsafe(row_idx);
    }

    inline void SetInvalidUnsafe(u64 entry_idx, u64 idx_in_entry)
    {
        assert(validity_mask != nullptr);
        validity_mask[entry_idx] &= ~(V(1) << V(idx_in_entry));
    }

    //! Marks the bit at the specified row index as invalid (i.e. null)
    inline void SetInvalidUnsafe(u64 row_idx)
    {
        assert(validity_mask != nullptr);
        assert(row_idx <= capacity);
        u64 entry_idx = row_idx / BITS_PER_VALUE;
        u64 idx_in_entry = row_idx % BITS_PER_VALUE;
        SetInvalidUnsafe(entry_idx, idx_in_entry);
    }

    //! Marks the entry at the specified row index as invalid (i.e. null)
    inline void SetInvalid(u64 row_idx)
    {
        assert(row_idx <= capacity);
        if (!validity_mask)
        {
            Initialize(capacity);
        }
        SetInvalidUnsafe(row_idx);
    }

    //! Mark the entry at the specified index as either valid or invalid (non-null or null)
    inline void Set(u64 row_idx, bool valid)
    {
        if (valid)
        {
            SetValid(row_idx);
        }
        else
        {
            SetInvalid(row_idx);
        }
    }

    inline void Copy(const TemplatedValidityMask &other, u64 count)
    {
        capacity = count;
        if (other.AllValid())
        {
            validity_data = nullptr;
            validity_mask = nullptr;
        }
        else
        {
            validity_data = make_buffer<ValidityBuffer>(other.validity_mask, count);
            validity_mask = validity_data->owned_data.get();
        }
    }
};

struct ValidityMask : public TemplatedValidityMask<u64>
{
public:
    inline ValidityMask() : TemplatedValidityMask(nullptr, STANDARD_VECTOR_SIZE)
    {
    }
    inline explicit ValidityMask(u64 capacity) : TemplatedValidityMask(capacity)
    {
    }
    inline explicit ValidityMask(u64 *ptr, u64 capacity) : TemplatedValidityMask(ptr, capacity)
    {
    }
    inline ValidityMask(const ValidityMask &original, u64 count) : TemplatedValidityMask(original, count)
    {
    }

    // Some extra apis
};
