#  OPENSSL_ROOT_DIR - Set this variable to the root installation of OpenSSL
#
# Read-Only variables:
#  OPENSSL_FOUND - system has the OpenSSL library
#  OPENSSL_INCLUDE_DIR - the OpenSSL include directory
#  OPENSSL_LIBRARIES - The libraries needed to use OpenSSL
#  OPENSSL_VERSION - This is set to $major.$minor.$revision$path (eg. 0.9.8s)

find_path(OPENSSL_INCLUDE_DIR
    NAMES openssl/ssl.h
    PATHS /usr/local/opt/openssl/include
    DOC "Openssl include header"
)

find_library(OPENSSL_SSL_LIBRARY NAMES ssl 
             PATHS /usr/local/opt/openssl/lib)

find_library(OPENSSL_CRYPTO_LIBRARY NAMES crypto 
             PATHS /usr/local/opt/openssl/lib /usr/lib/x86_64-linux-gnu)

set(OPENSSL_SSL_LIBRARIES ${OPENSSL_SSL_LIBRARY})
set(OPENSSL_CRYPTO_LIBRARIES ${OPENSSL_CRYPTO_LIBRARY})
set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OPENSSL DEFAULT_MSG OPENSSL_INCLUDE_DIR OPENSSL_LIBRARIES)

mark_as_advanced(OPENSSL_ROOT_DIR OPENSSL_INCLUDE_DIR OPENSSL_LIBRARIES)
