//
// Created by Aaron on 7/29/2016.
//

#ifndef UV_UTILS_DETAIL_HPP
#define UV_UTILS_DETAIL_HPP

#include <tuple>
#include <future>

namespace uv {
    namespace detail {
        /*
         * The is_any function is to compare a value to any number of other values, generating a long string of
         * (value == other1) || (value == other2) || ... etc
         * */

        template <typename T, typename K>
        inline bool is_any( T val, K val2 ) noexcept {
            return val == val2;
        }

        template <typename T, typename K, typename... Args>
        inline bool is_any( T val, K val2, Args... args ) noexcept {
            return is_any( val, val2 ) || is_any( val, args... );
        }

        template <typename T>
        inline bool is_any( T ) noexcept {
            return false;
        }

        /*
         * This exists because Boost lockfree structures require types that can be trivially copied,
         * with absolutely no overloaded operator= in them. So that rules out std::pair
         *
         * This also doesn't have a constructor because I can use TrivialPair{first_value, second_value}
         * semantics to initialize it. Note the curly braces instead of parenthesis.
         *
         * It's about as basic a pair structure as you can get, which is the idea. It's just going to be holding
         * pointers or other primitive types anyway.
         * */
        template <typename A, typename B>
        struct TrivialPair {
            typedef A first_type;
            typedef B second_type;

            first_type  first;
            second_type second;
        };

        template <typename Functor, typename T, std::size_t... S>
        inline UV_DECLTYPE_AUTO invoke_helper( Functor &&func, T &&t, std::index_sequence<S...> ) {
            return func( std::get<S>( std::forward<T>( t ))... );
        }

        template <typename Functor, typename T>
        inline UV_DECLTYPE_AUTO invoke( Functor &&func, T &&t ) {
            constexpr auto Size = std::tuple_size<typename std::decay<T>::type>::value;

            return invoke_helper( std::forward<Functor>( func ),
                                  std::forward<T>( t ),
                                  std::make_index_sequence<Size>{} );
        }

        template <typename T>
        inline std::future<T> make_ready_future( T &&t ) noexcept {
            std::promise<T> p;
            p.set_value( t );
            return p.get_future();
        }

        inline std::future<void> make_ready_future( void ) noexcept {
            std::promise<void> p;
            p.set_value();
            return p.get_future();
        }

        template <typename T, typename E>
        inline std::future<T> make_exception_future( E e ) noexcept {
            std::promise<T> p;
            p.set_exception( std::make_exception_ptr( e ));
            return p.get_future();
        };

        template <typename T>
        struct LazyStatic {
            typedef T value_type;

            virtual value_type init() = 0;

            value_type               value;
            std::once_flag           once;
            std::atomic_bool         ran;
            std::promise<void>       p;
            std::shared_future<void> s;

            LazyStatic() noexcept
                : p(), s( p.get_future()) {
            }

            void run() {
                this->value = this->init();

                p.set_value();

                this->ran = true;
            }

            inline operator value_type &() {
                return this->get();
            }

            inline value_type &get() {
                if( !this->ran ) {
                    std::call_once( once, &LazyStatic::run, this );

                    if( !this->ran ) {
                        s.wait();
                    }
                }

                return value;
            }
        };

        template <class T, class Compare>
        inline const T &clamp( const T &v, const T &lo, const T &hi, Compare comp ) noexcept {
            assert( !comp( hi, lo ));

            return comp( v, lo ) ? lo : comp( hi, v ) ? hi : v;
        }

        template <class T>
        inline const T &clamp( const T &v, const T &lo, const T &hi ) noexcept {
            return clamp( v, lo, hi, std::less<>());
        }

        namespace _then {
            using namespace std;

            constexpr launch default_policy = launch::deferred | launch::async;

            template <typename T, typename Functor>
            UV_DECLTYPE_AUTO then( future<T> &, Functor, launch = default_policy );

            template <typename T, typename Functor>
            UV_DECLTYPE_AUTO then( shared_future<T>, Functor, launch = default_policy );

            template <typename T, typename Functor>
            UV_DECLTYPE_AUTO then( future<T> &&, Functor, launch = default_policy );

            template <typename T, typename Functor>
            UV_DECLTYPE_AUTO then( promise<T> &, Functor, launch = default_policy );

            template <typename... Args>
            inline UV_DECLTYPE_AUTO waterfall( Args... );

            template <typename K>
            struct recursive_get {
                inline static UV_DECLTYPE_AUTO get( future<K> &&k ) {
                    return k.get();
                }
            };

            template <typename P>
            struct recursive_get<future<P>> {
                inline static UV_DECLTYPE_AUTO get( future<future<P>> &&k ) {
                    return recursive_get<P>::get( k.get());
                }
            };

            template <typename Functor, typename R = result_of<Functor>>
            struct then_invoke_helper {
                template <typename... Args>
                inline static UV_DECLTYPE_AUTO invoke( Functor f, Args... args ) {
                    return f( forward<Args>( args )... );
                }
            };

            template <typename Functor, typename K>
            struct then_invoke_helper<Functor, future<K>> {
                template <typename... Args>
                inline static UV_DECLTYPE_AUTO invoke( Functor f, Args... args ) {
                    return recursive_get<K>::get( f( forward<Args>( args )... ));
                }
            };

            template <typename Functor, typename K>
            struct then_invoke_helper<Functor, shared_future<K>> {
                template <typename... Args>
                inline static UV_DECLTYPE_AUTO invoke( Functor f, Args... args ) {
                    return f( forward<Args>( args )... ).get();
                }
            };

            template <typename T, typename Functor>
            struct then_helper {
                inline static UV_DECLTYPE_AUTO dispatch( launch policy, future<T> &&s, Functor f ) {
                    return then_invoke_helper<Functor>::invoke( f, recursive_get<T>::get( move( s )));
                }

                inline static UV_DECLTYPE_AUTO dispatch( launch policy, shared_future<T> s, Functor f ) {
                    return then_invoke_helper<Functor>::invoke( f, s.get());
                }
            };

            template <typename Functor>
            struct then_helper<void, Functor> {
                inline static UV_DECLTYPE_AUTO dispatch( launch policy, future<void> &&s, Functor f ) {
                    s.get();

                    return then_invoke_helper<Functor>::invoke( f );
                }

                inline static UV_DECLTYPE_AUTO dispatch( launch policy, shared_future<void> s, Functor f ) {
                    s.get();

                    return then_invoke_helper<Functor>::invoke( f );
                }
            };

            template <typename T, typename Functor>
            inline UV_DECLTYPE_AUTO then( future<T> &&s, Functor f, launch policy ) {
                return async( policy, [policy]( future <T> &&s2, Functor f2 ) {
                    return then_helper<T, Functor>::dispatch( policy, move( s2 ), f2 );
                }, move( s ), f );
            };

            template <typename T, typename Functor>
            inline UV_DECLTYPE_AUTO then( future<T> &s, Functor f, launch policy ) {
                return then( move( s ), f, policy );
            };

            template <typename T, typename Functor>
            inline UV_DECLTYPE_AUTO then( shared_future<T> s, Functor f, launch policy ) {
                return async( policy, [policy, s, f]() {
                    return then_helper<T, Functor>::dispatch( policy, s, f );
                } );
            };

            template <typename T, typename Functor>
            inline UV_DECLTYPE_AUTO then( promise<T> &s, Functor f, launch policy ) {
                return then( s.get_future(), f, policy );
            };

            /*
            inline UV_DECLTYPE_AUTO waterfall_helper() {
                return void();
            };

            template <typename Functor, typename... Args>
            inline UV_DECLTYPE_AUTO waterfall_helper( launch policy, Functor f, Args... args ) {
                return async( policy, [policy, f]( Args... inner_args ) {

                }, std::forward<Args>( args )... );
            };

            template <typename Functor, typename... Args>
            inline UV_DECLTYPE_AUTO waterfall_helper( Functor f, Args... args ) {
                return waterfall_helper( default_policy, f, std::forward<Args>( args )... );
            };

            template <typename... Args>
            inline UV_DECLTYPE_AUTO waterfall( Args... args ) {
                return waterfall_helper( std::forward<Args>( args )... );
            }
             */
        }
    }

    namespace util {
        using detail::_then::then;
    }
}

#endif //UV_UTILS_DETAIL_HPP
