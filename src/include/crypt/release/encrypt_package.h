#pragma once

//#if !defined(NO_CIPHER)
//#define NO_CIPHER 1
//#endif


struct KeyIV {
public:
  static size_t constexpr KeySize = 16;
  static size_t constexpr IvSize = 12;
  alignas(16) std::byte  key[16];
  alignas(16) mutable std::byte iv[16];

  KeyIV(std::byte const * key, std::byte const * iv) {
    assert(reinterpret_cast<uintptr_t>(this->iv) % 16 == 0);
    memcpy(this->key, key, KeySize);
    memcpy(this->iv, iv, IvSize);
#if DEBUG
    memset(this->iv + IvSize, 0, 16 - IvSize);
#endif
  }

private:
  template<class INT>
  static void inc(std::byte * dst, std::byte * src, INT const a) noexcept {
    struct __attribute__((packed)) Val {
      uint64_t low;
      uint32_t high;
    };
    auto dst_val = reinterpret_cast<Val *>(dst);
    auto src_val = reinterpret_cast<Val *>(src);
    asm (
        "add   %[a], %[low]\n"
        "adc   $0,   %[high]\n"
        : [low]  "=rm"    (dst_val->low),
          [high] "=rm"    (dst_val->high)
        : [a]    "erm"    (a),
                 "[low]"  (src_val->low),
                 "[high]" (src_val->high)
        : "cc"
    );
  }

public:

  void get_packet_iv_aligned(std::byte * dst) const noexcept {
    alignas(16) std::byte raw[16];
    {
      inc(raw, dst, 1);
    } while(!avocado::compare_exchange_128(iv, raw, dst));
  }

  void get_packet_iv(std::byte * dst) const noexcept {
    if ((reinterpret_cast<uintptr_t>(dst) & 0xF) == 0) {
      return get_packet_iv_aligned(dst);
    }
    alignas(16) std::byte iv[16] /*= {0}*/;
    get_packet_iv_aligned(iv);
    std::copy(iv, iv + 16, dst);
  }
};

/**
 * Can de-/encrypt packet buffers the data structure of an encrypted buffer will look like
 * +-------+-----------+-------+
 * |   IV  |    data   |  MAC  |
 * +-------+-----------+-------+
 * 0       16          n-16     n
 *
 */

class [[nodiscard]] PacketSsl {
private:
  std::shared_ptr<KeyIV> key_iv;

public:
  enum Func {
    DECRYPT = 0,
    ENCRYPT = 1
  };

  static size_t constexpr KeySize = CipherSsl::KeySize;
  static size_t constexpr IvSize  = CipherSsl::IvSize;
  static size_t constexpr MacSize = CipherSsl::MacSize;
  static size_t constexpr BlockBits = CipherSsl::BlockBits;
  static size_t constexpr BlockSize = CipherSsl::BlockSize;
  static size_t constexpr BlockMask = CipherSsl::BlockMask;
  static size_t constexpr MetaDataSize = IvSize + MacSize;

  explicit PacketSsl(std::shared_ptr<KeyIV> key_iv) : key_iv(std::move(key_iv)) {}

  static PacketSsl create(std::byte const * key, std::byte const * iv) {
    return PacketSsl(std::make_shared<KeyIV>(key, iv));
  }

  static PacketSsl create(CipherSsl const & cipher) {
    return PacketSsl(std::make_shared<KeyIV>(reinterpret_cast<std::byte const *>(cipher._key), reinterpret_cast<std::byte const *>(cipher._iv)));
  }

  //returns the minimum size the buffer for encryption must have
  [[gnu::pure, nodiscard]]
  static size_t constexpr get_buffer_size(size_t const msg_sz) noexcept {
#if NO_CIPHER
    return msg_sz;
#else
    return msg_sz + MetaDataSize;
#endif
  }

  //returns the maximum size the message can have with the given data_sz
  [[gnu::pure, nodiscard]]
  static size_t constexpr get_message_size(size_t const buf_sz) noexcept {
#if NO_CIPHER
    return buf_sz;
#else
    assert(buf_sz > MetaDataSize);
    return buf_sz - MetaDataSize;
#endif
  }

  [[gnu::pure, nodiscard]]
  static size_t constexpr adjust_size(size_t const msg_sz) noexcept(noexcept(get_buffer_size(msg_sz))) { return get_buffer_size(msg_sz); }

private:

  template<Func func>
  static bool cipher_final(EVP_CIPHER_CTX * pState, unsigned char * p_dst, int * len, unsigned char * mac) noexcept {
    if constexpr (func == ENCRYPT) {
      return EVP_CipherFinal_ex(pState, p_dst, len)
             && EVP_CIPHER_CTX_ctrl(pState, EVP_CTRL_GCM_GET_TAG, MacSize, mac);
    }
    return EVP_CIPHER_CTX_ctrl(pState, EVP_CTRL_GCM_SET_TAG, MacSize, mac)
           && EVP_CipherFinal_ex(pState, p_dst, len);
  }

public:

  //En-/decrypts a buffer, the function is thread safe
  template<Func func, class DST, class SRC>
  bool cipher(DST * dst, SRC * src, size_t size) const noexcept {
    assert(dst != nullptr);
    assert(src != nullptr);
    int len = 0;
    auto p_dst = reinterpret_cast<unsigned char *>(const_cast<typename std::remove_const<DST>::type *>(dst));
    auto p_src = reinterpret_cast<unsigned char *>(const_cast<typename std::remove_const<SRC>::type *>(src));

    ERR_clear_error();

    unsigned char * iv;
    unsigned char * mac;
    if constexpr (func == ENCRYPT) {
      iv = p_dst;
      p_dst += IvSize;
      mac = p_dst + size;
      key_iv->get_packet_iv(reinterpret_cast<std::byte*>(iv));
    } else {
      iv = p_src;
      p_src += IvSize;
      size = get_message_size(size);
      mac = p_src + size;
    }

#if 0 //with C++20 we could do it like this
    std::unique_ptr<EVP_CIPHER_CTX, decltype([](EVP_CIPHER_CTX * p) {
          if (p != nullptr) {
            EVP_CIPHER_CTX_free(p);
          }
        })> pState(EVP_CIPHER_CTX_new());
#else
    struct CTX_DELETER {
      void operator()(EVP_CIPHER_CTX * p) const {
        if (p != nullptr) {
          EVP_CIPHER_CTX_free(p);
        }
      }
    };

    std::unique_ptr<EVP_CIPHER_CTX, CTX_DELETER> pState(EVP_CIPHER_CTX_new());
#endif

    return  pState
            && EVP_CipherInit_ex(pState.get(), EVP_aes_128_gcm(), nullptr, reinterpret_cast<unsigned char const *>(key_iv->key), iv, func)
            && EVP_CipherUpdate(pState.get(), p_dst, &len, p_src, size)
            && cipher_final<func>(pState.get(), p_dst, &len, mac);
  }

  //en-/decrypts message/buffer
  //equivalent to calling encrypt or decrypt however with the func parameter as determinator for encryption or decryption
  template<class DST, class SRC>
  auto cipher(DST * dst, SRC * src, size_t src_size, Func func) const noexcept -> decltype(cipher<ENCRYPT>(dst, src, src_size)) {
    if (func == ENCRYPT) {
      return cipher<ENCRYPT>(dst, src, src_size);
    }
    return cipher<DECRYPT>(dst, src, src_size);
  }

  //encrypts a message, thread safe
  template<class DST, class SRC>
  auto encrypt(DST * dst, SRC * src, size_t src_size) const noexcept -> decltype(cipher<ENCRYPT>(dst, src, src_size)) {
#if NO_CIPHER
    memcpy(dst, src, src_size);
    return true;
#else
    return cipher<ENCRYPT>(dst, src, src_size);
#endif
  }

  //decyrpts a buffer, thread safe
  template<class DST, class SRC>
  auto decrypt(DST * dst, SRC * src, size_t src_size) const noexcept -> decltype(cipher<DECRYPT>(dst, src, src_size)) {
#if NO_CIPHER
    memcpy(dst, src, src_size);
    return true;
#else
    return cipher<DECRYPT>(dst, src, src_size);
#endif
  }

};

