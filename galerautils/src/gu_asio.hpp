//
// Copyright (C) 2014 Codership Oy <info@codership.com>
//


//
// Common ASIO methods and configuration options for Galera
//

#ifndef GU_ASIO_HPP
#define GU_ASIO_HPP

#include "gu_config.hpp"

#include "gu_macros.h" // gu_likely()
#include "common.h"    //

// Make GCC to treat this as the system header to suppress compiler
// warnings
#pragma GCC system_header
// Turn off some compiler warnings which are known to break the
// build.
#if defined(__GNUG__)
# if (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || (__GNUC__ > 4)
#  pragma GCC diagnostic push
# endif // (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || (__GNUC__ > 4)
// Ignore possibly unknown warning flags
# pragma GCC diagnostic ignored "-Wpragmas"
# pragma GCC diagnostic ignored "-Weffc++"
# pragma GCC diagnostic ignored "-Wold-style-cast"
# pragma GCC diagnostic ignored "-Wunused-local-typedef"
# pragma GCC diagnostic ignored "-Wunused-variable"
#endif // __GNUG__

#ifdef ASIO_HPP
#error "asio.hpp is already included before gu_asio.hpp, can't customize asio.hpp"
#endif // ASIO_HPP

#include "asio/version.hpp"

// ASIO does not interact well with kqueue before ASIO 1.10.5, see
// https://readlist.com/lists/freebsd.org/freebsd-current/23/119264.html
// http://think-async.com/Asio/asio-1.10.6/doc/asio/history.html#asio.history.asio_1_10_5
#if ASIO_VERSION < 101005
# define ASIO_DISABLE_KQUEUE
#endif // ASIO_VERSION < 101005

#include "asio.hpp"
#include "asio/ssl.hpp"

#include <string>
#include <fstream>


namespace gu
{

    // URI schemes for networking
    namespace scheme
    {
        const std::string tcp("tcp"); /// TCP scheme
        const std::string udp("udp"); /// UDP scheme
        const std::string ssl("ssl"); /// SSL scheme
        const std::string def("tcp"); /// default scheme (TCP)
    }

    //
    // SSL
    //

    // Configuration options for sockets
    namespace conf
    {
        /// Enable SSL explicitly
        const std::string use_ssl("socket.ssl");
        /// SSL cipher list
        const std::string ssl_cipher("socket.ssl_cipher");
        /// SSL compression algorithm
        const std::string ssl_compression("socket.ssl_compression");
        /// SSL private key file
        const std::string ssl_key("socket.ssl_key");
        /// SSL certificate file
        const std::string ssl_cert("socket.ssl_cert");
        /// SSL CA file
        const std::string ssl_ca("socket.ssl_ca");
        /// SSL password file
        const std::string ssl_password_file("socket.ssl_password_file");
    }

    // Return the cipher in use
    template <typename Stream>
    static const char* cipher(asio::ssl::stream<Stream>& socket)
    {
        return SSL_get_cipher_name(socket.impl()->ssl);
    }

    // Return the compression algorithm in use
    template <typename Stream>
    static const char* compression(asio::ssl::stream<Stream>& socket)
    {
        return SSL_COMP_get_name(
            SSL_get_current_compression(socket.impl()->ssl));
    }

    // register ssl parameters to config
    void ssl_register_params(gu::Config&);

    // initialize defaults, verify set options
    void ssl_init_options(gu::Config&);

    // prepare asio::ssl::context using parameters from config
    void ssl_prepare_context(const gu::Config&, asio::ssl::context&,
                             bool verify_peer_cert = true);

    //
    // Address manipulation helpers
    //

    // Return any address string.
    static inline std::string any_addr(const asio::ip::address& addr)
    {
        if (gu_likely(addr.is_v4() == true))
        {
            return addr.to_v4().any().to_string();
        }
        else
        {
            return addr.to_v6().any().to_string();
        }
    }

    // Escape address string. Surrounds IPv6 address with [].
    // IPv4 addresses not affected.
    static inline std::string escape_addr(const asio::ip::address& addr)
    {
        if (gu_likely(addr.is_v4() == true))
        {
            return addr.to_v4().to_string();
        }
        else
        {
            return "[" + addr.to_v6().to_string() + "]";
        }
    }

    // Unescape address string. Remove [] from around the address if found.
    static inline std::string unescape_addr(const std::string& addr)
    {
        std::string ret(addr);
        size_t pos(ret.find('['));
        if (pos != std::string::npos) ret.erase(pos, 1);
        pos = ret.find(']');
        if (pos != std::string::npos) ret.erase(pos, 1);
        return ret;
    }

    // Construct asio::ip::address from address string
    static inline asio::ip::address make_address(const std::string& addr)
    {
        return asio::ip::address::from_string(gu::unescape_addr(addr));
    }

    //
    // Error handling
    //

    // Exclude some errors which are generated by the SSL library.
    static inline bool exclude_ssl_error(const asio::error_code& ec)
    {
        switch (ERR_GET_REASON(ec.value()))
        {
#ifdef SSL_R_SHORT_READ
        case SSL_R_SHORT_READ:
            // Short read error seems to be generated quite frequently
            // by SSL library, probably because broken connections.
            return true;
#endif /* SSL_R_SHORT_READ */
        default:
            return false;
        }
    }

    // Return low level error info for asio::error_code if available.
    static inline const std::string extra_error_info(const asio::error_code& ec)
    {
        std::ostringstream os;
        if (ec.category() == asio::error::get_ssl_category())
        {
            char errstr[120] = {0, };
            ERR_error_string_n(ec.value(), errstr, sizeof(errstr));
            os << ec.value() << ": '" << errstr << "'";
        }
        return os.str();
    }

    //
    // Misc utilities
    //

    // Set common socket options
    template <class S>
    void set_fd_options(S& socket)
    {
        long flags(FD_CLOEXEC);
        if (fcntl(socket.native(), F_SETFD, flags) == -1)
        {
            gu_throw_error(errno) << "failed to set FD_CLOEXEC";
        }
    }
}

#if defined(__GNUG__)
# if (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || (__GNUC__ > 4)
#  pragma GCC diagnostic pop
# endif // (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || (__GNUC__ > 4)
#endif

#endif // GU_ASIO_HPP
