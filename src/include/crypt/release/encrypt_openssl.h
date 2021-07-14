#pragma once

//#if !defined(NO_CIPHER)
//#define NO_CIPHER 1
//#endif

class CipherSsl {
public:
    static size_t const KeySize = 16;
    static size_t const IvSize = 12;
    static size_t const MacSize = 16;
    static size_t const BlockBits = 128;
    static size_t const BlockSize = BlockBits / CHAR_BIT;
    static size_t const BlockMask = (1 << constexpr_math::log(BlockSize)) - 1;
    uint8_t _key[KeySize];
    uint8_t _iv[IvSize];

    CipherSsl(uint8_t const * key, uint8_t const * iv) {
        memcpy(_key, key, KeySize);
        memcpy(_iv, iv, IvSize);
    }

    [[gnu::pure, nodiscard]]
    constexpr static inline size_t adjust_size(size_t const org_size) noexcept {
        return org_size;
        //return (org_size & BlockMask) == 0 ? org_size : org_size + (BlockSize - (org_size & BlockMask)); 
    }

public:

    static bool encrypt_final(EVP_CIPHER_CTX * pState, unsigned char * dst, int * len, uint8_t * mac) noexcept {
        return EVP_EncryptFinal_ex(pState, dst + *len, len)
               && EVP_CIPHER_CTX_ctrl(pState, EVP_CTRL_GCM_GET_TAG, MacSize, mac);
    }

    static bool decrypt_final(EVP_CIPHER_CTX * pState, unsigned char * dst, int * len, uint8_t const * mac) noexcept {
        uint8_t l_tag[MacSize];
        memcpy(l_tag, mac, MacSize);

        return EVP_CIPHER_CTX_ctrl(pState, EVP_CTRL_GCM_SET_TAG, MacSize, l_tag)
                && EVP_DecryptFinal_ex(pState, dst + *len, len);
    }

private:
    template<class T1, class T2, class MAC_T,
             int init(CipherSsl const *, EVP_CIPHER_CTX *, EVP_CIPHER const *, ENGINE *, unsigned char const *, unsigned char const *),
             int update(CipherSsl const *, EVP_CIPHER_CTX *, unsigned char *, int *, unsigned char const *, int),
             bool end(CipherSsl const *, EVP_CIPHER_CTX *, unsigned char *, int *, MAC_T)>
    bool cipher(T1 * dst, T2 * src, size_t const size, MAC_T mac) const noexcept {
#if NO_CIPHER
        memcpy(dst, src, size);
        return true;
#else
        int len = 0;
        auto p_dst = reinterpret_cast<unsigned char *>(const_cast<typename std::remove_const<T1>::type *>(dst));

        EVP_CIPHER_CTX * pState = nullptr;
        auto _f = speicher::finally([&pState] {
                    if (pState != nullptr) {
                        EVP_CIPHER_CTX_free(pState);
                    }
                });

        ERR_clear_error();

        return ((pState = EVP_CIPHER_CTX_new()))
                && (init(this, pState, EVP_aes_128_gcm(), nullptr, reinterpret_cast<unsigned char const *>(_key), reinterpret_cast<unsigned char const *>(_iv)))
                && (update(this, pState, p_dst, &len, reinterpret_cast<unsigned char const *>(src), size))
                && (end(this, pState, p_dst + len, &len, mac));
#endif
    }

    template<int openssl_update(EVP_CIPHER_CTX *, unsigned char *, int *, unsigned char const *, int)>
    static int update(CipherSsl const *, EVP_CIPHER_CTX * ctx, unsigned char * dst, int * len, unsigned char const * src, int size) {
      return openssl_update(ctx, dst, len, src, size); 
    }

    template<int openssl_init(EVP_CIPHER_CTX *, EVP_CIPHER const *, ENGINE *, unsigned char const *, unsigned char const *)>
    static int init(CipherSsl const *, EVP_CIPHER_CTX * ctx, EVP_CIPHER const * cipher, ENGINE * engine, unsigned char const * key, unsigned char const * iv) {
      return openssl_init(ctx, cipher, engine, key, iv);
    }

    template<class MAC_T, bool openssl_end(EVP_CIPHER_CTX *, unsigned char *, int *, MAC_T)>
    static bool end(CipherSsl const *, EVP_CIPHER_CTX * ctx, unsigned char * dst, int * len, MAC_T mac) {
      return openssl_end(ctx, dst, len, mac);
    }

public:

    template<class T1, class T2>
    bool encrypt(T1 * dst, T2 * src, size_t const size, uint8_t * mac) const noexcept {
      return cipher<T1, T2, decltype(mac), init<EVP_EncryptInit_ex>, update<EVP_EncryptUpdate>, end<decltype(mac), encrypt_final>>(dst, src, size, mac);
    }

    template<class T1, class T2>
    bool decrypt(T1 * dst, T2 *src, size_t const size, uint8_t const * mac) const noexcept {
      return cipher<T1, T2, decltype(mac), init<EVP_DecryptInit_ex>, update<EVP_DecryptUpdate>, end<decltype(mac), decrypt_final>>(dst, src, size, mac);
    }

};

template< class MAC_T
        , int crypt_init(EVP_CIPHER_CTX *, EVP_CIPHER const *, ENGINE *, unsigned char const *, unsigned char const *)
        , int crypt_update(EVP_CIPHER_CTX *, unsigned char *, int *, unsigned char const *, int)
        , bool crypt_final(EVP_CIPHER_CTX *, unsigned char *, int *, MAC_T)>
struct [[nodiscard]] SplitCipherSsl {
  static size_t const KeySize   = CipherSsl::KeySize;
  static size_t const IvSize    = CipherSsl::IvSize;
  static size_t const MacSize   = CipherSsl::MacSize;
  static size_t const BlockBits = CipherSsl::BlockBits;
  static size_t const BlockSize = CipherSsl::BlockSize;
  static size_t const BlockMask = CipherSsl::BlockMask;

  bool valid;
  bool _final{false};

  EVP_CIPHER_CTX * pState{nullptr};

  [[gnu::pure, nodiscard]]
  static decltype(auto) inline constexpr adjust_size(size_t size) noexcept { return CipherSsl::adjust_size(size); }

  void init(uint8_t const * key, uint8_t const * iv) {
    ERR_clear_error();
    valid = (pState == EVP_CIPHER_CTX_new()) && crypt_init(pState, EVP_aes_128_gcm(), nullptr, reinterpret_cast<unsigned char const *>(key), reinterpret_cast<unsigned char const *>(iv));
  }

  SplitCipherSsl(CipherSsl const & cipher) {
    init(cipher._key, cipher._iv);
  }

  SplitCipherSsl(uint8_t const * key, uint8_t const * iv) {
    init(key, iv);
  }

  SplitCipherSsl(SplitCipherSsl const & other) : valid(other.valid), _final(other._final) {
    if (!_final && other.pState) {
      EVP_CIPHER_CTX_copy(pState, other.pState);
    }
  }

  SplitCipherSsl(SplitCipherSsl && other) : valid(other.valid), _final(other._final), pState(other.pState) {
    other.valid = false;
    other.pState = nullptr;
  }

  SplitCipherSsl & operator=(SplitCipherSsl const & other) {
    valid = other.valid;
    _final = other._final;
    if (pState != nullptr) {
      EVP_CIPHER_CTX_free(pState);
      pState = nullptr;
    }
    if (other.pState != nullptr) {
      EVP_CIPHER_CTX_copy(pState, other.pState);
    }
    return *this;
  }

  SplitCipherSsl & operator=(SplitCipherSsl && other) {
    valid = other.valid;
    _final = other._final;
    pState = other.pState;
    other.valid = false;
    other.pState = nullptr;
    return *this;
  }

  [[nodiscard]] constexpr bool is_valid() noexcept { return valid; }
  [[nodiscard]] constexpr bool is_final() noexcept { return _final; }

protected:

  template<class DST, class SRC>
  size_t update_(DST * dst, SRC const * src, size_t const n) noexcept { //could be a lambda
    auto const size = n * sizeof(SRC);
    size_t dst_pos{0};
    int len{0};
    auto p_dst = reinterpret_cast<unsigned char *>(dst);
    auto p_src = reinterpret_cast<unsigned char const *>(src);
    do {
      valid = crypt_update(pState, p_dst + dst_pos, &len, p_src + dst_pos, size - dst_pos);
    } while (len != 0 && dst_pos < size && valid);
    return dst_pos;
  }

  template<class DST>
  bool finalize_(DST * dst, uint8_t * mac) noexcept { //could be a lambda
    int len{0};
    auto p_dst = reinterpret_cast<unsigned char *>(dst);
    _final = true;
    return (valid = crypt_final(pState, p_dst, &len, mac));
  }

public:

  template<class DST, class SRC>
  [[nodiscard]] size_t update(DST * dst, SRC const * src, size_t const n) noexcept {
    if (valid && !_final) return update_(dst, src, n);
    return 0;
  }

  template<class DST>
  bool finalize(DST * dst, uint8_t * mac) noexcept {
    return valid && !_final && finalize_(dst, mac);
  }

  operator bool() const noexcept {
    return valid;
  }

};

using EncryptSsl = SplitCipherSsl<uint8_t *, EVP_EncryptInit_ex, EVP_EncryptUpdate, CipherSsl::encrypt_final>;
using DecryptSsl = SplitCipherSsl<uint8_t const *, EVP_DecryptInit_ex, EVP_DecryptUpdate, CipherSsl::decrypt_final>;

struct [[nodiscard]] HashSsl : public EncryptSsl {
  uint8_t mac[MacSize] = {0};

  HashSsl(CipherSsl const & cipher) : EncryptSsl(cipher) {}

  HashSsl(uint8_t const * key, uint8_t const * iv) : EncryptSsl(key, iv) {}

  HashSsl(HashSsl const & other) : EncryptSsl(other) {
    if (_final) {
      std::copy(other.mac, other.mac + MacSize, mac);
    }
  }

  HashSsl(HashSsl && other) : EncryptSsl(std::move(other)) {
    if (_final) {
      std::copy(other.mac, other.mac + MacSize, mac);
    }
  }

  HashSsl & operator=(HashSsl && other) {
    std::copy(other.mac, other.mac + MacSize, mac);
    EncryptSsl::operator=(std::move(other));
    return *this;
  }

  HashSsl & operator=(HashSsl const & other) {
    std::copy(other.mac, other.mac + MacSize, mac); 
    EncryptSsl::operator=(other);
    return *this;
  }

  ~HashSsl() = default;

  [[nodiscard]] constexpr bool is_valid() noexcept { return valid; }
  [[nodiscard]] constexpr bool is_final() noexcept { return _final; }

  template<class T>
  bool update(T const * src, size_t const n) noexcept {
    auto const size = sizeof(T) * n;
    uint8_t dst[size];
    return EncryptSsl::update(dst, reinterpret_cast<uint8_t const *>(src), size);
  }

  template<class T>
  bool update(T const & src, size_t const n) noexcept {
    return update(&src, n);
  }

  bool finalize() noexcept {
    uint8_t dst[BlockSize];
    return EncryptSsl::finalize(dst, mac);
  }

  inline bool equal(uint8_t const * other_mac) const noexcept {
    return !memcmp(mac, other_mac, MacSize);
  }

  inline bool operator== (HashSsl const & other) const noexcept {
    return _final && other._final && equal(other.mac);
  }

};

