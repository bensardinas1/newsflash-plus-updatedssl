// Copyright (c) 2010-2015 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// This software is copyrighted software. Unauthorized hacking, cracking, distribution
// and general assing around is prohibited.
// Redistribution and use in source and binary forms, with or without modification,
// without permission are prohibited.
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "newsflash/config.h"

#include "newsflash/warnpush.h"
#  include <openssl/opensslv.h>
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#  include <openssl/x509v3.h>
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#  include <openssl/engine.h>
#  include <openssl/conf.h>
#endif
#include "newsflash/warnpop.h"

#include <cassert>
#include <stdexcept>
#include <string>

#ifdef WINDOWS_OS
#include <windows.h>
#endif

#include "sslcontext.h"
#include "platform.h"

namespace {
    class context;

    std::size_t g_refcount;
    std::mutex  g_mutex;
    context*    g_context;

    class context
    {
    public:
        context() : context_(nullptr)
        {
            newsflash::openssl_init();
        }

       ~context()
        {
            if (context_)
                SSL_CTX_free(context_);
        }

        void init()
        {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
            // OpenSSL 1.1.0+ : TLS_method() negotiates the highest available protocol.
            const SSL_METHOD* meth = TLS_method();
#else
            // OpenSSL 1.0.x : SSLv23_method is the flexible method.
            const SSL_METHOD* meth = SSLv23_method();
#endif

            context_ = SSL_CTX_new(meth);
            if (!context_)
               throw std::runtime_error("SSL_CTX_new");

            // Disable known-insecure protocols: SSLv2, SSLv3, TLS 1.0, TLS 1.1
            SSL_CTX_set_options(context_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
#ifdef SSL_OP_NO_TLSv1
            SSL_CTX_set_options(context_, SSL_OP_NO_TLSv1);
#endif
#ifdef SSL_OP_NO_TLSv1_1
            SSL_CTX_set_options(context_, SSL_OP_NO_TLSv1_1);
#endif

            // Use a secure cipher list — exclude NULL, anonymous, weak, and export ciphers.
            SSL_CTX_set_cipher_list(context_, "HIGH:!aNULL:!eNULL:!MD5:!RC4:!3DES:!DES:!EXPORT:!PSK:!SRP");

            // Enable certificate verification.
            // First try a bundled CA file next to the executable,
            // then fall back to OpenSSL's default paths.
            bool ca_loaded = false;
#ifdef WINDOWS_OS
            {
                // Look for ssl/certs/ca-bundle.crt relative to the executable
                wchar_t exepath[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, exepath, MAX_PATH);
                std::wstring dir(exepath);
                auto pos = dir.find_last_of(L"\\//");
                if (pos != std::wstring::npos)
                    dir.resize(pos);
                // Convert to narrow for OpenSSL
                char narrow[MAX_PATH] = {};
                WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, narrow, MAX_PATH, nullptr, nullptr);
                std::string cafile = std::string(narrow) + "\\ssl\\certs\\ca-bundle.crt";
                if (SSL_CTX_load_verify_locations(context_, cafile.c_str(), nullptr) == 1)
                    ca_loaded = true;
            }
#endif
            if (!ca_loaded)
            {
                if (!SSL_CTX_set_default_verify_paths(context_))
                    throw std::runtime_error("SSL_CTX_set_default_verify_paths failed");
            }
            SSL_CTX_set_verify(context_, SSL_VERIFY_PEER, nullptr);
        }

        SSL_CTX* ctx()
        {
            return context_;
        }

    private:
        SSL_CTX* context_;
    };

} // namespace

namespace newsflash
{

sslcontext::sslcontext()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_context)
    {
        assert(g_refcount == 0);
        // need to store the instance pointer, because
        // when we initialize the SSL it already performs
        // callbacks to the locking functions, which require
        // the locks throught the instance pointer.
        // hence we need the two-phase construction here.
        g_context = new context();
        try
        {
            g_context->init();
        }
        catch (const std::exception&)
        {
            delete g_context;
            g_context = nullptr;
            throw;
        }
    }
    ++g_refcount;
}

sslcontext::~sslcontext()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (--g_refcount == 0)
    {
        delete g_context;
        g_context = nullptr;
    }
}

sslcontext::sslcontext(const sslcontext& other)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_refcount;
}

SSL_CTX* sslcontext::ssl()
{
    return g_context->ctx();
}

// https://www.openssl.org/support/faq.html#PROG2
// https://www.openssl.org/support/faq.html#PROG13
struct openssl {
    openssl()
    {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        // OpenSSL 1.1.0+ handles library init and threading internally.
        // OPENSSL_init_ssl() is called automatically, but we call it
        // explicitly for clarity.
        OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
#else
        // OpenSSL 1.0.x requires explicit init and threading callbacks.
        locks = new std::mutex[CRYPTO_num_locks()];

        SSL_library_init();
        SSL_load_error_strings();
        ERR_load_BIO_strings();
        ERR_load_crypto_strings();

        OpenSSL_add_all_algorithms();

        // these callback functions need to be set last because
        // the setup functions above call these (especially the locking function)
        // which calls back to us and whole thing goes astray.
        CRYPTO_set_id_callback(identity_callback);
        CRYPTO_set_locking_callback(lock_callback);
#endif
    }

   ~openssl()
    {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
        CRYPTO_set_locking_callback(nullptr);

        ERR_free_strings();
        EVP_cleanup();
        CRYPTO_cleanup_all_ex_data();
        ENGINE_cleanup();
        CONF_modules_free();

        delete [] locks;
#endif
        // OpenSSL 1.1.0+ handles cleanup automatically via atexit.
    }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    static
    unsigned long identity_callback()
    {
        return newsflash::get_thread_identity();
    }

    static
    void lock_callback(int mode, int index, const char*, int)
    {
        if (mode & CRYPTO_LOCK)
            locks[index].lock();
       else locks[index].unlock();
    }

    static std::mutex* locks;
#endif
};

#if OPENSSL_VERSION_NUMBER < 0x10100000L
std::mutex* openssl::locks;
#endif

void openssl_init()
{
    // thread safe per c++11 rules
    static openssl lib;
}

} // newsflash
