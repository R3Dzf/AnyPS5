#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceSslInit_nid_postfix(std::size_t);
int APS5_VABI sceSslTerm_nid_postfix(int);
int APS5_VABI sceSslGetCaCerts(int, void*);
int APS5_VABI sceSslFreeCaCerts(int, void*);
int APS5_VABI sceSslClose(int);
int APS5_VABI sceSslGetSerialNumber(int, void*, std::uint8_t*, std::size_t*);
int APS5_VABI sceSslLoadCert(int, int, void**, void*, void*);
int APS5_VABI sceSslFreeSslCertName(int, void*);
void* APS5_VABI sceSslGetSubjectName(int, void*);
void* APS5_VABI sceSslGetIssuerName(int, void*);
int APS5_VABI sceSslGetNameEntryCount(int, void*);
int APS5_VABI sceSslGetNameEntryInfo(int, void*, int, std::uint8_t*, std::size_t, std::uint8_t*, std::size_t,
                                      std::size_t*);
}

struct SslMemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
    std::int32_t reserved;
};

extern "C" int APS5_VABI sceSslGetMemoryPoolStats(int, SslMemoryPoolStats*);

struct SslCaCerts {
    void* certs;
    std::size_t num;
    void* pool;
};

struct SslData {
    void* ptr;
    std::size_t size;
};

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int notFound = static_cast<int>(0x8095F004);
    constexpr int invalidArg = static_cast<int>(0x8095177A);
    int marker = 0;

    const int context = sceSslInit_nid_postfix(0x10000);
    Require(context > 0);
    SslMemoryPoolStats stats{1, 1, 1, 1};
    Require(sceSslGetMemoryPoolStats(context, &stats) == 0);
    Require(stats.pool_size == 0x10000 && stats.max_inuse_size == 0 && stats.current_inuse_size == 0 && stats.reserved == 0);
    Require(sceSslGetCaCerts(context, nullptr) == invalidArg);
    Require(sceSslFreeCaCerts(context, nullptr) == invalidArg);

    SslCaCerts certs{&marker, 3, &marker};
    Require(sceSslGetCaCerts(context, &certs) == notFound);
    Require(certs.certs == nullptr && certs.num == 0 && certs.pool == nullptr);

    certs = {&marker, 3, &marker};
    Require(sceSslFreeCaCerts(context, &certs) == 0);
    Require(certs.certs == nullptr && certs.num == 0 && certs.pool == nullptr);

    Require(sceSslClose(0) == invalidArg);
    Require(sceSslClose(-1) == invalidArg);
    Require(sceSslClose(context) == 0);

    char caBytes[4] = {1, 2, 3, 4};
    SslData entry{caBytes, sizeof(caBytes)};
    void* list[] = {&entry};
    void* broken[] = {nullptr};
    Require(sceSslLoadCert(context, -1, nullptr, nullptr, nullptr) == invalidArg);
    Require(sceSslLoadCert(context, 2, nullptr, nullptr, nullptr) == invalidArg);
    Require(sceSslLoadCert(context, 1, broken, nullptr, nullptr) == invalidArg);
    Require(sceSslLoadCert(context, 1, list, nullptr, nullptr) == 0);
    Require(sceSslLoadCert(context, 0, nullptr, nullptr, nullptr) == 0);

    std::uint8_t sbo[16]{};
    std::size_t sboLen = sizeof(sbo);
    Require(sceSslGetSerialNumber(context, nullptr, sbo, &sboLen) == invalidArg);
    Require(sceSslGetSerialNumber(context, &marker, sbo, nullptr) == invalidArg);
    Require(sceSslGetSerialNumber(context, &marker, sbo, &sboLen) == notFound);
    Require(sboLen == sizeof(sbo));

    Require(sceSslGetSubjectName(context, nullptr) == nullptr);
    Require(sceSslGetSubjectName(context, &marker) == nullptr);
    Require(sceSslGetIssuerName(context, nullptr) == nullptr);
    Require(sceSslGetIssuerName(context, &marker) == nullptr);

    Require(sceSslGetNameEntryCount(context, nullptr) == invalidArg);
    Require(sceSslGetNameEntryCount(context, &marker) == notFound);

    std::uint8_t oid[32]{};
    std::uint8_t value[32]{};
    std::size_t valueLen = 8;
    Require(sceSslGetNameEntryInfo(context, nullptr, 0, oid, sizeof(oid), value, sizeof(value), &valueLen) == invalidArg);
    Require(sceSslGetNameEntryInfo(context, &marker, -1, oid, sizeof(oid), value, sizeof(value), &valueLen) == invalidArg);
    Require(sceSslGetNameEntryInfo(context, &marker, 0, oid, sizeof(oid), value, sizeof(value), nullptr) == invalidArg);
    Require(sceSslGetNameEntryInfo(context, &marker, 0, oid, sizeof(oid), value, sizeof(value), &valueLen) == notFound);

    Require(sceSslFreeSslCertName(context, nullptr) == invalidArg);
    Require(sceSslFreeSslCertName(context, &marker) == 0);

    Require(sceSslTerm_nid_postfix(context) == 0);
}
