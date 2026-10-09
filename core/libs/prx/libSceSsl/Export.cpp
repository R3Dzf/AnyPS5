#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>

// No network is emulated: contexts, templates and requests can be created, but any request
// that would touch the network fails with the library's network error.
static constexpr int ERROR_NETWORK = static_cast<int>(0x80435001);
static std::atomic<int> g_nextHandle{1};
static std::mutex g_poolsMutex;
static std::map<int, uint64_t> g_pools;

namespace {

constexpr int ERROR_NOT_FOUND = static_cast<int>(0x8095F004);
constexpr int ERROR_INVALID_ARG = static_cast<int>(0x8095177A);

struct SslData {
    char* ptr;
    size_t size;
};

struct SslMemoryPoolStats {
    size_t pool_size;
    size_t max_inuse_size;
    size_t current_inuse_size;
    int32_t reserved;
};

struct SslCaCerts {
    SslData* certs;
    size_t num;
    void* pool;
};

}

extern "C" {

int APS5_VABI sceSslFreeCaCerts(int ssl_ctx_id, void* ca_certs) {
    (void)ssl_ctx_id;
    if (!ca_certs) return ERROR_INVALID_ARG;
    *static_cast<SslCaCerts*>(ca_certs) = {};
    return 0;
}

int APS5_VABI sceSslGetCaCerts(int ssl_ctx_id, void* ca_certs) {
    (void)ssl_ctx_id;
    if (!ca_certs) return ERROR_INVALID_ARG;
    *static_cast<SslCaCerts*>(ca_certs) = {};
    return ERROR_NOT_FOUND;
}

int APS5_VABI sceSslInit_nid_postfix(uint64_t pool_size) {
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(g_poolsMutex);
    g_pools[id] = pool_size;
    return id;
}

int APS5_VABI sceSslTerm_nid_postfix(int ssl_ctx_id) {
    std::lock_guard lock(g_poolsMutex);
    g_pools.erase(ssl_ctx_id);
    return 0;
}

int APS5_VABI sceSslClose(int ssl_connection_id) {
    if (ssl_connection_id <= 0) return ERROR_INVALID_ARG;
    return 0;
}

int APS5_VABI sceSslGetSerialNumber(int ssl_ctx_id, void* ssl_cert, std::uint8_t* sbo_data, std::size_t* sbo_len) {
    if (!ssl_cert || !sbo_len) return ERROR_INVALID_ARG;
    (void)ssl_ctx_id;
    (void)sbo_data;
    return ERROR_NOT_FOUND;
}

int APS5_VABI sceSslLoadCert(int ssl_ctx_id, int ca_cert_num, SslData** ca_list, SslData* cert, SslData* priv_key) {
    if (ca_cert_num < 0 || (ca_cert_num > 0 && !ca_list)) return ERROR_INVALID_ARG;
    for (int index = 0; index < ca_cert_num; ++index) {
        if (!ca_list[index] || !ca_list[index]->ptr || !ca_list[index]->size) return ERROR_INVALID_ARG;
    }
    (void)ssl_ctx_id;
    (void)cert;
    (void)priv_key;
    return 0;
}

int APS5_VABI sceSslGetMemoryPoolStats(int ssl_ctx_id, SslMemoryPoolStats* stats) {
    if (stats == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(g_poolsMutex);
    const auto pool = g_pools.find(ssl_ctx_id);
    if (pool == g_pools.end()) throw std::invalid_argument("sceSslGetMemoryPoolStats: unknown context");
    *stats = {static_cast<size_t>(pool->second), 0, 0, 0};
    return 0;
}

int APS5_VABI sceSslFreeSslCertName(int ssl_ctx_id, void* cert_name) {
    if (!cert_name) return ERROR_INVALID_ARG;
    (void)ssl_ctx_id;
    return 0;
}

void* APS5_VABI sceSslGetIssuerName(int ssl_ctx_id, void* ssl_cert) {
    if (!ssl_cert) return nullptr;
    (void)ssl_ctx_id;
    return nullptr;
}

int APS5_VABI sceSslGetNameEntryCount(int ssl_ctx_id, void* cert_name) {
    if (!cert_name) return ERROR_INVALID_ARG;
    (void)ssl_ctx_id;
    return ERROR_NOT_FOUND;
}

int APS5_VABI sceSslGetNameEntryInfo(int ssl_ctx_id, void* cert_name, int entry_num, std::uint8_t* oid_name,
                                     std::size_t max_oid_name_len, std::uint8_t* value, std::size_t max_value_len,
                                     std::size_t* value_len) {
    if (!cert_name || !value_len || entry_num < 0) return ERROR_INVALID_ARG;
    (void)ssl_ctx_id;
    (void)oid_name;
    (void)max_oid_name_len;
    (void)value;
    (void)max_value_len;
    return ERROR_NOT_FOUND;
}

void* APS5_VABI sceSslGetSubjectName(int ssl_ctx_id, void* ssl_cert) {
    if (!ssl_cert) return nullptr;
    (void)ssl_ctx_id;
    return nullptr;
}

int APS5_VABI sceSslGetPem(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
