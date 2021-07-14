from typing import Union, Any

def output_file_name(svr, native, n_svr, threads, keys, value_size, ops, ratio) -> str:
  return "{}_{}_n{}_t{}_k{}_v{}_o{}_r{}".format(svr, native, n_svr, threads, keys, value_size, ops, ratio)

def guess_type(value : str) -> Union[int,float,str]:
  try:
    return int(value)
  except ValueError:
    try:
      return float(value)
    except ValueError:
      return value

def _replace(value):
  if isinstance(value, list):
    return [replace_type(v) for v in value]
  return guess_type(value)

def replace_type(value : Any) -> Any:
  if not isinstance(value, dict):
    return _replace(value)
  
  res = {}
  for k, v in value.items():
    new_key = guess_type(k)
    res[new_key] = replace_type(v)
  return res
