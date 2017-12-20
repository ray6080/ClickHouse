#pragma once

#include <AggregateFunctions/AggregateFunctionQuantile.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int NOT_IMPLEMENTED;
}

/** */
template <typename Value>
struct QuantileExactWeighted
{
    /// When creating, the hash table must be small.
    using Map = HashMap<
        Value, Weight,
        HashCRC32<Value>,
        HashTableGrower<4>,
        HashTableAllocatorWithStackMemory<sizeof(std::pair<Value, Weight>) * (1 << 3)>
    >;

    Map map;

    void add(const Value & x)
    {
        ++map[x];
    }

    template <typename Weight>
    void add(const Value & x, const Weight & weight)
    {
        map[x] += weight;
    }

    void merge(const QuantileExactWeighted & rhs)
    {
        for (const auto & pair : rhs.map)
            map[pair.first] += pair.second;
    }

    void serialize(WriteBuffer & buf) const
    {
        map.write(buf);
    }

    void deserialize(ReadBuffer & buf)
    {
        typename Map::Reader reader(buf);
        while (reader.next())
        {
            const auto & pair = reader.get();
            map[pair.first] = pair.second;
        }
    }

    /// Get the value of the `level` quantile. The level must be between 0 and 1.
    Value get(Float64 level) const
    {
        size_t size = map.size();

        if (0 == size)
            return Value();

        /// Copy the data to a temporary array to get the element you need in order.
        using Pair = Map::value_type;
        std::unique_ptr<Pair[]> array_holder(new Pair[size]);
        Pair * array = array_holder.get();

        size_t i = 0;
        UInt64 sum_weight = 0;
        for (const auto & pair : map)
        {
            sum_weight += pair.second;
            array[i] = pair;
            ++i;
        }

        std::sort(array, array + size, [](const Pair & a, const Pair & b) { return a.first < b.first; });

        UInt64 threshold = std::ceil(sum_weight * level);
        UInt64 accumulated = 0;

        const Pair * it = array;
        const Pair * end = array + size;
        while (it < end)
        {
            accumulated += it->second;

            if (accumulated >= threshold)
                break;

            ++it;
        }

        if (it == end)
            --it;

        return it->first;
    }

    /// Get the `size` values of `levels` quantiles. Write `size` results starting with `result` address.
    /// indices - an array of index levels such that the corresponding elements will go in ascending order.
    void getMany(const Float64 * levels, const size_t * indices, size_t size, Value * result) const
    {
        size_t size = map.size();

        if (0 == size)
        {
            for (size_t i = 0; i < size; ++i)
                result[i] = Value();
            return;
        }

        /// Copy the data to a temporary array to get the element you need in order.
        using Pair = Map::value_type;
        std::unique_ptr<Pair[]> array_holder(new Pair[size]);
        Pair * array = array_holder.get();

        size_t i = 0;
        UInt64 sum_weight = 0;
        for (const auto & pair : map)
        {
            sum_weight += pair.second;
            array[i] = pair;
            ++i;
        }

        std::sort(array, array + size, [](const Pair & a, const Pair & b) { return a.first < b.first; });

        UInt64 accumulated = 0;

        const Pair * it = array;
        const Pair * end = array + size;

        size_t level_index = 0;
        UInt64 threshold = std::ceil(sum_weight * levels[indices[i]]);

        while (it < end)
        {
            accumulated += it->second;

            while (accumulated >= threshold)
            {
                result[indices[i]] = it->first;
                ++level_index;

                if (level_index == size)
                    return;

                threshold = std::ceil(sum_weight * levels[indices[i]]);
            }

            ++it;
        }

        while (level_index < size)
        {
            result[indices[i]] = array[size - 1].first;
            ++level_index;
        }
    }

    /// The same, but in the case of an empty state, NaN is returned.
    float getFloat(Float64 level) const
    {
        throw Exception("Method getFloat is not implemented for QuantileExact", ErrorCodes::NOT_IMPLEMENTED);
    }

    void getManyFloat(const Float64 * levels, const size_t * indices, size_t size, float * result) const
    {
        throw Exception("Method getManyFloat is not implemented for QuantileExact", ErrorCodes::NOT_IMPLEMENTED);
    }
};

}