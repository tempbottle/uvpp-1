//
// Created by Aaron on 7/26/2016.
//

#ifndef UV_OS_HPP
#define UV_OS_HPP

#include "fwd.hpp"

#include "exception.hpp"

#include <vector>
#include <array>
#include <algorithm>
#include <mutex>

namespace uv {
    namespace os {
        //uv_rusage_t is just all primitives, so it can be copied directly.
        using rusage_t = uv_rusage_t;

        /*
         * uv_cpu_info_t and uv_interface_address_t both allocate memory which needs to be freed by libuv,
         * so we just copy all the primitives and put the allocated c-strings into std::strings
         * */
        struct cpu_info_t {
            std::string                   model;
            int                           speed;
            uv_cpu_info_t::uv_cpu_times_s cpu_times;

            cpu_info_t( const uv_cpu_info_t &c ) noexcept
                : model( c.model ),
                  speed( c.speed ),
                  cpu_times( c.cpu_times ) {
            }

            cpu_info_t( const cpu_info_t & )noexcept = default;

            cpu_info_t( cpu_info_t && ) noexcept = default;
        };

        struct passwd_t {
            std::string username;
            long        uid;
            long        gid;
            std::string shell;
            std::string homedir;

            passwd_t( const uv_passwd_t &psw ) noexcept
                : username( psw.username ),
                  uid( psw.uid ),
                  gid( psw.gid ),
                  homedir( psw.homedir ) {
                if( psw.shell != nullptr ) {
                    this->shell = psw.shell;
                }
            }

            passwd_t( const passwd_t & ) noexcept = default;

            passwd_t( passwd_t && ) noexcept = default;
        };

        inline char **setup_args( int argc, char **argv ) noexcept {
            assert( argv != nullptr );

            return uv_setup_args( argc, argv );
        }

        /*
         * uv_get_process_title returns UV_ENOBUFS on failure when the supplied buffer is too small, so just allocate
         * more memory next round if that happens. Maximum buffer sizes are persistent so it probably won't need to
         * reallocate much after the first time.
         * */
        std::string process_title() {
            static size_t buffer_size = 16; //Good starting size, will statically increase if needed

            int res = 0;

            while( true ) {
                char *buffer = new char[buffer_size];

                res = uv_get_process_title( buffer, buffer_size );

                if( res == 0 ) {
                    std::string ret( buffer );

                    delete[] buffer;

                    return ret;

                } else if( res == UV_ENOBUFS ) {
                    delete[] buffer;

                    buffer_size *= 2;

                } else {
                    /*
                     * If we received UV_EINVAL, that means either the buffer was null or the buffer_size was
                     * incorrect, so double check it here and delete if necessary
                     * */
                    if( buffer != nullptr ) {
                        delete[] buffer;
                    }

                    throw ::uv::Exception( res );
                }
            }
        }

        void process_title( const std::string &title ) {
            int res = uv_set_process_title( title.c_str());

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }
        }

        /*
         * So because this requires a pre-allocated buffer, it may need to reallocate a couple times to
         * get enough space to store the whole path. Most paths should be fine since the default is MAX_PATH,
         * but some utf8 long path might need larger buffers.
         *
         * Luckily, uv_cwd sets the buffer_size pointer to the correct value on failure, so we know exactly how much
         * needs to be allocated the next round.
         * */
        std::string cwd() {
            size_t buffer_size = 260; //MAX_PATH on Windows

            int res = 0;

            while( true ) {
                char *buffer = new char[buffer_size];

                res = uv_cwd( buffer, &buffer_size );

                if( res == 0 ) {
                    std::string ret( buffer, buffer_size );

                    delete[] buffer;

                    return ret;

                } else if( res == UV_ENOBUFS ) {
                    /*
                     * uv_cwd automatically assigns the new space requirements to buffer_size,
                     * so we just need to clear this one so it can allocate the correct amount next loop.
                     * */
                    delete[] buffer;

                } else {
                    /*
                     * If we received UV_EINVAL, that means either the buffer was null or the buffer_size was
                     * incorrect, so double check it here and delete if necessary
                     * */
                    if( buffer != nullptr ) {
                        delete[] buffer;
                    }

                    throw ::uv::Exception( res );
                }
            }
        }

        void chdir( const std::string &dir ) {
            int res = uv_chdir( dir.c_str());

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }
        }

        std::string homedir() {
            static std::mutex m;

            std::lock_guard<std::mutex> lock( m );
            size_t                      buffer_size = 260; //MAX_PATH on Windows
            int                         res         = 0;

            while( true ) {
                char *buffer = new char[buffer_size];

                res = uv_os_homedir( buffer, &buffer_size );

                if( res == 0 ) {
                    std::string ret( buffer, buffer_size );

                    delete[] buffer;

                    return ret;

                } else if( res == UV_ENOBUFS ) {
                    /*
                     * uv_cwd automatically assigns the new space requirements to buffer_size,
                     * so we just need to clear this one so it can allocate the correct amount next loop.
                     * */
                    delete[] buffer;

                } else {
                    /*
                     * If we received UV_EINVAL, that means either the buffer was null or the buffer_size was
                     * incorrect, so double check it here and delete if necessary
                     * */
                    if( buffer != nullptr ) {
                        delete[] buffer;
                    }

                    throw ::uv::Exception( res );
                }
            }
        }

        std::string tmpdir() {
            static std::mutex m;

            std::lock_guard<std::mutex> lock( m );
            size_t                      buffer_size = 260; //MAX_PATH on Windows
            int                         res         = 0;

            while( true ) {
                char *buffer = new char[buffer_size];

                res = uv_os_tmpdir( buffer, &buffer_size );

                if( res == 0 ) {
                    std::string ret( buffer, buffer_size );

                    delete[] buffer;

                    return ret;

                } else if( res == UV_ENOBUFS ) {
                    /*
                     * uv_cwd automatically assigns the new space requirements to buffer_size,
                     * so we just need to clear this one so it can allocate the correct amount next loop.
                     * */
                    delete[] buffer;

                } else {
                    /*
                     * If we received UV_EINVAL, that means either the buffer was null or the buffer_size was
                     * incorrect, so double check it here and delete if necessary
                     * */
                    if( buffer != nullptr ) {
                        delete[] buffer;
                    }

                    throw ::uv::Exception( res );
                }
            }
        }

        /*
         * Unlike uv_cwd or uv_get_process_title, uv_exepath gives NO INDICATION that the supplied buffer was too small.
         *
         * So we literally just have to keep increasing the buffer size until the two strings are equal to each other,
         * because that would mean the path length has been reached.
         * */
        std::string exepath() {
            static size_t buffer_hint = 260; //MAX_PATH on Windows

            size_t buffer_size = buffer_hint;

            int res = 0;

            bool same_flag = false;

            std::string result;

            while( !same_flag ) {
                char *buffer = new char[buffer_size];

                res = uv_exepath( buffer, &buffer_size );

                if( res != 0 ) {
                    delete[] buffer;

                    throw ::uv::Exception( res );

                } else if( result.compare( 0, buffer_size, buffer ) == 0 ) {
                    same_flag = true;

                } else {
                    result = std::string( buffer, buffer_size );

                    buffer_size *= 2;

                    buffer_hint = buffer_size;
                }

                delete[] buffer;
            }

            return result;
        }

        size_t rss_memory() {
            size_t rss;

            int res = uv_resident_set_memory( &rss );

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }

            return rss;
        }

        inline uint64_t total_memory() {
            return uv_get_total_memory();
        }

        double uptime() {
            double u;

            int res = uv_uptime( &u );

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }

            return u;
        }

        std::array<double, 3> loadavg() {
            std::array<double, 3> result;

            uv_loadavg( result.data());

            return result;
        };

        rusage_t rusage() {
            rusage_t u;

            int res = uv_getrusage( &u );

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }

            return u;
        }

        std::vector<cpu_info_t> cpu_info() {
            uv_cpu_info_t *cpus;
            int           count;

            int res = uv_cpu_info( &cpus, &count );

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }

            std::vector<cpu_info_t> c( &cpus[0], &cpus[count] );

            uv_free_cpu_info( cpus, count );

            return c;
        }

        passwd_t passwd() {
            uv_passwd_t uv_psw;

            int res = uv_os_get_passwd( &uv_psw );

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }

            passwd_t psw( uv_psw );

            uv_os_free_passwd( &uv_psw );

            return psw;
        }

        /*
         * Doesn't copy shell or homedir
         * */
        std::string username() {
            uv_passwd_t uv_psw;

            int res = uv_os_get_passwd( &uv_psw );

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }

            std::string u = uv_psw.username;

            uv_os_free_passwd( &uv_psw );

            return u;
        }

        void kill( int pid, int signum ) {
            int res = uv_kill( pid, signum );

            if( res != 0 ) {
                throw ::uv::Exception( res );
            }
        }
    }
}

#endif //UV_OS_HPP
