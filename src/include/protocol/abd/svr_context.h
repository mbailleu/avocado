#pragma once

#include "crypt/encrypt_package.h"
#include "rpc_context.h"

namespace avocado {

class ABD;

struct SvrContext : public RpcContext {
  ABD * parent;
  SvrContext(PacketSsl const & cipher) : RpcContext(cipher) {}
};

} //avocado
