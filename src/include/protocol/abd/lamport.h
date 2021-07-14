#pragma once

#include <cstdint>
#include <climits>
#include <limits>

#if (CHAR_BIT != 8)
#error You are weird, go away!
#endif

#if __cpp_impl_three_way_comparison > 201711L
#include <compare>
#else
//We could use a standard memcmp instead, or just a weird combination of operators
#define LAMPORT_OPERATOR(OP) \
    constexpr bool operator OP (Lamport const & rhs) const { \
        return to_int(*this) OP to_int(rhs); \
    }
#endif

namespace avocado {
    
class Lamport {
public:
    using Count_t = uint32_t;
    using M_id_t  = uint32_t;
    using Int_t   = uint64_t;

    static constexpr Int_t to_int(Lamport const & l) {
        static_assert(sizeof(Count_t) + sizeof(M_id_t) <= sizeof(Int_t), "The count + machine id, do not fit into the choosen datatype for comparision, please rewrite operators!");
        Int_t res = l.count;
        return (res << (sizeof(M_id_t) * CHAR_BIT)) | l.m_id;
    }

public:
    constexpr Lamport(Count_t count, M_id_t m_id) : count(count), m_id(m_id) {}
    constexpr explicit Lamport(Int_t l) : count(l >> 32), m_id(0xFFFFFFFF & l) {}

    constexpr Int_t to_int() const {
        return to_int(*this);
    }

    Count_t get_count() const noexcept {
        return count;
    }

    M_id_t get_machine_id() const noexcept {
        return m_id;
    }

    void set_count(Count_t count) noexcept {
        this->count = count;
    }

    static constexpr Lamport min() noexcept {
      return Lamport(std::numeric_limits<Count_t>::min(), std::numeric_limits<M_id_t>::min());
    }

    static constexpr Lamport max() noexcept {
      return Lamport(std::numeric_limits<Count_t>::max(), std::numeric_limits<M_id_t>::max());
    }

#if __cpp_impl_three_way_comparison > 201711L

    auto operator<=>(Lamport const &) const = default; //this should work, but neither gcc nor clang currently support it!

#else //Therefore, we have this: (sorry)

    LAMPORT_OPERATOR(<)
    LAMPORT_OPERATOR(>)
    LAMPORT_OPERATOR(<=)
    LAMPORT_OPERATOR(>=)
    LAMPORT_OPERATOR(==)
    LAMPORT_OPERATOR(!=)

#endif //__cpp_impl_three_way_comparison

private:
    Count_t count;
    M_id_t m_id;
};

} //namespace avocado
