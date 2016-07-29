//
// Created by Aaron on 7/24/2016.
//

#ifndef UV_UTILS_HPP
#define UV_UTILS_HPP

namespace uv {
    template <typename T, typename K>
    inline bool is_any( T val, K val2 ) {
        return val == val2;
    }

    template <typename T, typename K, typename... Args>
    inline bool is_any( T val, K val2, Args... args ) {
        return is_any( val, val2 ) || is_any( val, args... );
    }

    template <typename T>
    inline bool is_any( T ) {
        return false;
    }

    namespace detail {
        template <typename A, typename B>
        struct TrivialPair {
            typedef A first_type;
            typedef B second_type;

            first_type  first;
            second_type second;
        };
    }
}

#endif //UV_UTILS_HPP
