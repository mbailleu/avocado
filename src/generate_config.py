#! /usr/bin/env python3

import sys
import socket
import argparse
from typing import List

config_str  = "-n {threads}\n" \
              "-u {ip}:{port}\n" \
              "-p {phys}\n" \
              "-k {n_keys}\n" \
              "-v {max_vsz}\n"

replica_str = "\n-a {ip}:{port}:{threads}"

known_svr = {
    "rose"   : "129.215.165.52",
    "martha" : "129.215.165.53",
    "donna"  : "129.215.165.54",
    "amy"    : "129.215.165.57",
    "clara"  : "129.215.165.58",
}

def get_host_name(server: str) -> str:
  res = socket.gethostbyname(server)
  if (res != "127.0.0.1"):
    return res
  s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  s.connect(("8.8.8.8", 80))
  res = s.getsockname()[0]
  s.close()
  return res

def get_ip(server: str) -> str:
  res = known_svr.get(server, None)
  if res != None:
    return res
  res = get_host_name(server)
  known_svr[server] = res
  return res

def generate_str(i: int, args) -> str:
  res = config_str.format(threads = args.threads, ip = get_ip(args.servers[i]), port = args.port, phys = args.phys_port, n_keys = args.number_of_distinct_keys, max_vsz = args.max_value_size, output = "out_{}".format(args.servers[i]))
  res += args.dump_config
  for j in range(len(args.servers)):
    if j == i:
      continue
    res += replica_str.format(ip = get_ip(args.servers[j]), port = args.port, threads = args.threads)
  return res

def main(args: List[str]):
  parser = argparse.ArgumentParser(description="Generates configs for avocado setups")
  parser.add_argument('-p', "--port", type = int, default = 31850, help = "set the port on which the server should run (currently only supports the same port for all servers)")
  parser.add_argument('-t', "--threads", type = int, required = True, help = "set the number of threads the server should run (currently only supports the same number of threads for all servers)")
  parser.add_argument('-k', "--number-of-distinct-keys", type = int, required = True, help = "set the number of distinct keys to be used in the in memory kv")
  parser.add_argument('-vsz', "--max-value-size", type = int, required = True, help = "set the maximum value size")
  parser.add_argument("--phys-port", type = int, default = 0, help = "set the physical port")
  parser.add_argument("servers", type = str, nargs = '+', help = "list of server the config should be written for")
  parser.add_argument("-d", "--dump-config", action="store_const", const = "--dump-config\n", default="", help="dump config at start of application")
  args = parser.parse_args(args[1:])
  for i in range(len(args.servers)):
    with open("config-{}".format(args.servers[i]), "w") as f:
      print(generate_str(i, args), file = f)

if __name__ == "__main__":
  main(sys.argv)
