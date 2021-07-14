#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "store.h"
#include "common.h"

#include "rpc.h"

//#include <docopt.h>

#include "protocol/abd/protocol.h"

erpc::Rpc<erpc::CTransport> * rpc;
void req_handler(erpc::ReqHandle * req_handle, void *) {
    auto & resp = req_handle->pre_resp_msgbuf;
    rpc->resize_msg_buffer(&resp, Msg_size);
    std::sprintf(reinterpret_cast<char *>(resp.buf), "hello");
    std::cout << "here\n" << std::endl;
    rpc->enqueue_response(req_handle, &resp);
}

int main(int argc, char ** argv) {
    parse_args(argc - 1, argv + 1);
    std::string server_uri = SvrName + ":" + std::to_string(UDP_port);
    erpc::Nexus nexus(server_uri, 0, 0);
    nexus.register_req_func(Req_type, req_handler);

    rpc = new erpc::Rpc<erpc::CTransport>(&nexus, nullptr, 0, nullptr);
    rpc->run_event_loop(100000);

#if 0
    auto alloc = mm::Allocator<Avocado::Entry>::New("/dev/zero");
    uint8_t key[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf};
    CipherSsl crypt(key, key); 
    Avocado::Hashtable<mm::Allocator<Avocado::Entry>>  store(std::move(crypt), 10, *alloc);
    char const t[] = "Hello World";
    auto kv = reinterpret_cast<uint8_t const *>(t);
    store.put(5, 4, kv);
    store.put(4, 5, kv);
    auto ret = store.get(5, kv);
    std::cout << "Hello\n";
    std::cout << ret.key.get() << std::endl;
    return 0;
#endif
}
