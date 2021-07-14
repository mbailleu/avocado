#! /usr/bin/env python3

import os
import queue
import argparse
import json
import sys
import subprocess
import avocadoslack
import multiprocessing
import graphs
from shared import output_file_name, replace_type
from threading import Thread, Event
from typing import List, Dict, Tuple
from string import Template

generate_dir = ""
run_dir = {}
output_dir = ""
count = 0
current = 0

def clean_hugepages(servers: List[str]):
  for svr in servers:
    subprocess.call(["ssh", svr, "sudo", "-E", "rm", "/dev/hugepages/*"])

def run_benchmark(svr: List[str], benchmark: Dict[str, int]) -> List[Tuple[str, int]]:
  finished = Event()
  finished.clear()
  q = queue.Queue()
  
  def create_subprocess_str(svr: str, benchmark: Dict[str, int], outf) -> List[str]: 
    tmp_config = ["--key_size", "8",
                  "--value_size", str(benchmark["value_size"]),
                  "--n_cmds", str(benchmark["ops"]),
                  "--n_keys", str(benchmark["keys"]),
                  "--permille_reads", str(benchmark["ratio"]),
                  "--output", outf]
    bench_config = ["ssh", svr]
    if benchmark["env"] == "native":
      bench_config.extend(["Hugepagesize=2097152",
                           "sudo", "-E",
                           "{}/server".format(run_dir["native"]),
                           "$(<{}/config)".format(run_dir["native"])])
      bench_config.extend(tmp_config)

    elif benchmark["env"] == "scone":
      bench_config.extend(["docker", "exec", "scone-run-glibc",
                           "bash", "-c",
                           "'cd {} && ./run-server.sh $(< ../config-{}) ".format(run_dir["scone"], svr)
                           + ' '.join(tmp_config) + "'"])

    return bench_config

  def run_(svr: str, bechmark: Dict[str, int], output_file) -> Tuple[str,int]:
    global output_dir
    outf = "{}/{}".format(output_dir, output_file)
    with open("{}/{}".format("debug", output_file), "w+") as debug:
      exec_str = create_subprocess_str(svr, benchmark, outf)
      process = subprocess.Popen(exec_str,
                                 stdout = debug,
                                 stderr = subprocess.STDOUT)
    res = -1
    while (True):
      try:
        res = process.wait(10)
      except (TimeoutError, subprocess.TimeoutExpired) as err:
        if finished.isSet():
          try:
            res = process.wait(1)
          except (TimeoutError, subprocess.TimeoutExpired) as err:
            process.terminate()
            try:
              res = process.wait(1)
            except (TimeoutError, subprocess.TimeoutExpired) as err:
              process.kill()
          break
        continue
      break
    q.put((outf, res))

  global current
  current += 1
  print("[{}/{}] Run benchmark: {} on {}".format(current, count, benchmark, svr))
  threads = []
  output = [output_file_name(s,
                             benchmark["env"],
#                             "native",
                             len(svr),
                             benchmark["threads"],
                             benchmark["keys"],
                             benchmark["value_size"],
                             benchmark["ops"],
                             benchmark["ratio"])
            for s in svr]

  clean_hugepages(svr)

  for s, o in zip(svr, output):
    tmp = Thread(target = run_, args = (s, benchmark, o))
    tmp.start()
    threads.append(tmp)

  results = [q.get()]
  finished.set()

  for t in threads:
    t.join()

  while not (q.empty()):
    results.append(q.get())

  for output_file, res in results:
    if res == 0:
      tmp = {}
      with open(output_file) as f:
        tmp = json.load(f)
      tmp = replace_type(tmp)
      subprocess.call(["sudo", "-E", "rm", output_file])
      with open(output_file, 'w') as f:
        json.dump(tmp, f, indent=4)

  return results


def run_generator(svr: List[str], benchmark : Dict[str, int]) -> None:
  cur_dir = os.getcwd()
  os.chdir(generate_dir)
  args = ["./generate_config.py".format(generate_dir), 
          "-t", str(benchmark["threads"]),
          "-k", str(benchmark["keys"]),
          "-vsz", str(benchmark["value_size"]),
          "-d"]
  args.extend(svr)
  subprocess.call(args)
  os.chdir(cur_dir)

def generate_pdf(config):
  graphs.plot(config["xlabel"], config["ylabel"], config["group_key"], config["dependencies"], config["output"])
  graphs.pdf_to_png(config["output"], config["png"])

def main(sys_args: List[str]) -> int:
  parser = argparse.ArgumentParser(description='Run benchmarks')
  parser.add_argument("result_folder", type=str, help="folder to copy data")
  parser.add_argument("--run_dir", type=str, nargs='+', help="benchmark config to be run")
  parser.add_argument("generate_dir", type=str, help="location of generate script")
  parser.add_argument("output_dir", type=str, help="location to store bechmark results")
  parser.add_argument("config", type=str, nargs='+', help="benchmark config to be run")
  args = parser.parse_args(sys_args[1:])

  global generate_dir
  global run_dir
  global output_dir

  generate_dir = os.path.abspath(args.generate_dir)
  for dir in args.run_dir:
      f = dir.split(':')
      run_dir[f[0]] = os.path.abspath(f[1])
  output_dir = os.path.abspath(args.output_dir)

  org_configs = []
  for config_file in args.config:
    config = {}
    ok = False
    with open(config_file) as f:
      config = json.load(f)
    org_configs.append(config)


  configs = []
  for i in range(len(org_configs)):
    configs.append(org_configs[i])
    for j in range(i + 1, len(org_configs)):
      for b in org_configs[i]["benchmarks"]:
        try:
          org_configs[j]["benchmarks"].remove(b)
        except Exception:
          pass

  global count
  for c in configs:
    count += len(c["benchmarks"])

  for config in configs:
    svr = config["servers"]
    ok = True
    for b in config["benchmarks"]:
      run_generator(svr, b)
      results = run_benchmark(svr, b)
#      if not all([ x == 0 for (_,x) in results]):
 #       ok = False
        #avocadoslack.send_msg(avocadoslack.get_conversation_id("Maurice"), msg = "Error in benchmark {}".format(b))
    #config["png"] = "{}.png".format(os.path.splitext(config["output"])[0])
    #generate_pdf(config)
    #b = config["benchmarks"][0]
    #msg = Template(config["message"]).safe_substitute(b)
    #to = "Maurice" if ok == False else "avocado"
    #avocadoslack.send_msg(avocadoslack.get_conversation_id(to), file=config["png"], msg=msg)


if __name__ == "__main__":
  main(sys.argv)
