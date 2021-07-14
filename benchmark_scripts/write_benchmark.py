#! /usr/bin/env python3

import json
import argparse
import sys
from shared import output_file_name
from typing import List, Dict, Union

def get_list(cmdline: str) -> List[int]:
  return [int(x) for x in cmdline.split(':')]

def get_list_str(cmdline : str) -> List[str]:
  return cmdline.split(':')

def join(env : List[str], threads: List[int], values: List[int], ops: List[int], keys: List[int], ratios: List[int]) -> List[Dict[str,Union[int,str]]]:
  res = []
  for e in env:
    for t in threads:
      for v in values:
        for o in ops:
          for k in keys:
            for r in ratios:
              res.append({
                "env"        : e,
                "threads"    : t,
                "value_size" : v,
                "ops"        : o,
                "keys"       : k,
                "ratio"      : r,
              })
  return res

def deps(cmds):
  #TODO see further down
  res = []
  for b in cmds["benchmarks"]:
    for svr in cmds["servers"]:
      res.append("data/{}".format(output_file_name(svr,
                           b["env"],
                           len(cmds["servers"]),
                           b["threads"],
                           b["keys"],
                           b["value_size"],
                           b["ops"],
                           b["ratio"])))
  return res

def main(sys_args : List[str]) -> int:
  parser = argparse.ArgumentParser(description='Creates Benchmark config for run')
  parser.add_argument("output_file", type=str, help="output_file")
  parser.add_argument("output_pdf", type=str, help="Pdf to be generated")
  parser.add_argument("env", type=str, help="Run natively or in scone")
  parser.add_argument("xlabel", type=str, help="x axis")
  parser.add_argument("ylabel", type=str, help="y axis")
  parser.add_argument("group_key", type=str, help="key data to group by")
  parser.add_argument("threads", type=str, help="threads count to be tested")
  parser.add_argument("values", type=str, help="Value size to be tested")
  parser.add_argument("ops", type=str, help="Number of operations")
  parser.add_argument("key", type=str, help="Number of distinct keys")
  parser.add_argument("ratio", type=str, help="Read ratio in promille")
  parser.add_argument("servers", type=str, nargs='+', help="Servers invold in the experiment")
  parser.add_argument("-m", type=str, default = "", help="Message to be send to slack. Supports placeholder {threads}, {value_size}, {ops}, {keys} {ratio}")
  args = parser.parse_args(sys_args[1:])
  env = get_list_str(args.env)
  threads = get_list(args.threads)
  values = get_list(args.values)
  ops = get_list(args.ops)
  keys = get_list(args.key)
  ratios = get_list(args.ratio)
  cmd = {
      "benchmarks" : join(env, threads, values, ops, keys, ratios),
      "servers"    : args.servers,
      "message"    : args.m,
      "output"     : args.output_pdf,
      "xlabel"     : args.xlabel,
      "ylabel"     : args.ylabel,
      "group_key"  : args.group_key,
  }
  cmd["dependencies"] = deps(cmd)

  with open(args.output_file, "w") as f:
    json.dump(cmd, f, indent=4)

if __name__ == "__main__":
    main(sys.argv)
