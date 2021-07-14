#include <iostream>

#include "store.h"
#include "common.h"

#include "rpc.h"

erpc::Rpc<erpc::CTransport> * rpc;
erpc::MsgBuffer req;
erpc::MsgBuffer resp;

void cont_func(void *, void *) { std::cout << "CLIENT: " << reinterpret_cast<char *>(resp.buf) << '\n'; }

void sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {}

int main(int argc, char ** argv) {
    parse_args(argc - 1, argv + 1);
    std::string server_uri = SvrName + ":" + std::to_string(UDP_port);
    std::string client_uri = ClnNames[0] + ":" + std::to_string(UDP_port);

    erpc::Nexus nexus(client_uri, 0, 0);

    rpc = new erpc::Rpc<erpc::CTransport>(&nexus, nullptr, 0, sm_handler);
    int session_num = rpc->create_session(server_uri, 0);
    std::cout << session_num << '\n';

    while(!rpc->is_connected(session_num)) rpc->run_event_loop_once();

    req = rpc->alloc_msg_buffer_or_die(Msg_size);
    resp = rpc->alloc_msg_buffer_or_die(Msg_size);

    rpc->enqueue_request(session_num, Req_type, &req, &resp, cont_func, nullptr);
    rpc->run_event_loop(100);

    delete rpc;
}
