#pragma once


#include <msg_buffer.h>

#if !defined(NDEBUG)

#include <iosfwd>

std::ostream & operator<<(std::ostream & os, erpc::MsgBuffer const & buf) {
  return os << "MsgBuffer::Size(" << buf.get_data_size() << ") MsgBuffer::buf(" << static_cast<void const *>(buf.buf) << ')';
}

#endif //NDEBUG
