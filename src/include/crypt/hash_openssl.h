#pragma once

#include <type_traits>
#include <iostream>

#include "final_act.h"
#include "constexpr_math.h"

#include "openssl/aes.h"
#include "openssl/evp.h"
#include "openssl/err.h"

#include "encrypt_openssl.h"

class HashSsl {
public:

    static constexpr size_t KeySize = 16;
    static constexpr size_t IvSize  = 12;
    static constexpr size_t BlockBits = 128;
    static constexpr size_t BlockSize = BlockBits / CHAR_BIT;
    static constexpr size_t BlockMask = (1 << constexpr_math::log(BlockSize)) - 1;

    [[gnu::pure]]
    constexpr static inline size_t adjust_size(size_t const org_size) noexcept {
        return (org_size & BlockMask) == 0 ? org_size : org_size + (BlockSize - (org_size & BlockMask));
    }

private:

    EVP_CIPHER_CTX * ctx{nullptr};
    int ok{1};

    uint8_t _key[KeySize];
    uint8_t _iv[IvSize];

    uint8_t hashblock[BlockSize] = {};

    int offset{0};

    void init(uint8_t const * key, uint8_t const * iv) noexcept {
        memcpy(_key, key, KeySize);
        memcpy(_iv, iv, IvSize);

        ERR_clear_error();

        ok = (ctx = EVP_CIPHER_CTX_new())
             && EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, reinterpret_cast<unsigned char const *>(_key), reinterpret_cast<unsigned char const *>(_iv));
    }

    void copy(HashSsl const & other) noexcept {
        ERR_clear_error();

        if (!(ctx = EVP_CIPHER_CTX_new())) {
            ok = 0;
            return;
        }
        
        EVP_CIPHER_CTX_copy(ctx, other.ctx);

        ok = other.ok;
        memcpy(hashblock, other.hashblock, BlockSize);
        offset = other.offset;
    }


public:

    HashSsl(uint8_t const * key, uint8_t const * iv) noexcept {
        init(key, iv);
    }

    HashSsl(CipherSsl const & cipher) noexcept {
        init(cipher._key, cipher._iv);
    }

    HashSsl(HashSsl const & other) noexcept {
        copy(other);
    }

    HashSsl(HashSsl && other) noexcept : ctx(other.ctx), ok{other.ok}, offset{other.offset} {
        memcpy(_key, other._key, KeySize);
        memcpy(_iv, other._iv, IvSize);
        memcpy(hashblock, other.hashblock, BlockSize);
        other.ctx = nullptr;
        other.ok = 0;
    }

    HashSsl & operator=(CipherSsl const & cipher) noexcept {
        init(cipher._key, cipher._iv);

        return *this;
    }

    HashSsl & operator=(HashSsl const & other) noexcept {
        copy(other);

        return *this;
    }

    HashSsl & operator=(HashSsl && other) noexcept {
        ctx = other.ctx;
        memcpy(_key, other._key, KeySize);
        memcpy(_iv, other._iv, IvSize);
        memcpy(hashblock, other.hashblock, BlockSize);
        ok = other.ok;
        offset = other.offset;
        other.ctx = nullptr;
        other.ok = 0;

        return *this;
    }

    void update(uint8_t const * src, size_t size) {
        int len{0};
        size_t s = size < BlockSize ? size : BlockSize - offset;
        if (!(ok = EVP_EncryptUpdate(ctx, hashblock + offset, &len, reinterpret_cast<unsigned char const *>(src), s))) return;
        auto pos = s;
        size -= s;
        for (; size >= BlockSize; size -= BlockSize, pos += BlockSize) {
            if (!(ok = EVP_EncryptUpdate(ctx, hashblock, &len, reinterpret_cast<unsigned char const *>(src) + pos, BlockSize))) return;
        }
        if (size > 0) {
            if(!(ok = EVP_EncryptUpdate(ctx, hashblock, &offset, reinterpret_cast<unsigned char const *>(src) + pos, size))) return;
        }
    }

    void operator() (uint8_t const * src, size_t size) {
        update(src, size);
    }

    operator bool() {
        return is_ok();
    }

    bool is_ok() {
        return ok;
    }

    void end() {
        if (!ctx) {
            memset(hashblock, 0, BlockSize);
            return;
        }
        int len;
        std::cout << "HashSsl: " << this << '\n';
        ok = EVP_DecryptFinal_ex(ctx, hashblock + offset, &len);
        assert(len > BlockSize);
    }

    int compare(uint8_t * mac) {
        return memcmp(hashblock, mac, BlockSize);
    }

    void get(uint8_t * mac) {
        memcpy(mac, hashblock, BlockSize);
    }

    ~HashSsl() {
        if (ctx != nullptr) {
            EVP_CIPHER_CTX_free(ctx);
        }
    }

};
