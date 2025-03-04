import aiohttp
import asyncio
import sys
import time
import argparse

follow_latencies = []
register_latencies = []

async def upload_follow(session, addr, user_0, user_1):
  global follow_latencies
  payload = {'user_name': 'username_' + user_0, 'followee_name': 'username_' + user_1}
  start_time = time.perf_counter()
  async with session.post(addr + "/user_follow", data=payload) as resp:
    end_time = time.perf_counter()
    latency = end_time - start_time
    follow_latencies += [end_time - start_time]
    return await resp.text()

async def upload_register(session, addr, user):
  global register_latencies
  payload = {'first_name': 'first_name_' + user, 'last_name': 'last_name_' + user,
             'username': 'username_' + user, 'password': 'password_' + user, 'user_id': user}
  start_time = time.perf_counter()
  async with session.post(addr + "/user_register", data=payload) as resp:
    end_time = time.perf_counter()
    latency = end_time - start_time
    register_latencies += [latency]
    return await resp.text()


def getNodes(file):
  line = file.readline()
  word = line.split()[0]
  return int(word)

def getEdges(file):
  edges = []
  lines = file.readlines()
  for line in lines:
    edges.append(line.split())
  return edges

def printResults(results):
  result_type_count = {}
  for result in results:
    try:
      result_type_count[result] += 1
    except KeyError:
      result_type_count[result] = 1
  for result_type, count in result_type_count.items():
    if result_type.startswith("Success"):
      print("Succeeded:", count)
    elif "500 Internal Server Error" in result_type:
      print("Failed:", count, "Error:", "Internal Server Error")
    elif result_type.strip() == '{}':
        print("Succeeded:", count)
    else:
      print("Failed:", count, "Error:", result_type.strip())

async def register(addr, nodes):
  idx = 0
  tasks = []
  conn = aiohttp.TCPConnector(limit=200)
  async with aiohttp.ClientSession(connector=conn) as session:
    print("Registering Users...")
    for i in range(1, nodes + 1):
      task = asyncio.ensure_future(upload_register(session, addr, str(i)))
      tasks.append(task)
      idx += 1
      if idx % 200 == 0:
        _ = await asyncio.gather(*tasks)
        print(idx)
    results = await asyncio.gather(*tasks)
    printResults(results)


async def follow(addr, edges):
  idx = 0
  tasks = []
  conn = aiohttp.TCPConnector(limit=200)
  async with aiohttp.ClientSession(connector=conn) as session:
    print("Adding follows...")
    for edge in edges:
      task = asyncio.ensure_future(upload_follow(session, addr, edge[0], edge[1]))
      tasks.append(task)
      task = asyncio.ensure_future(upload_follow(session, addr, edge[1], edge[0]))
      tasks.append(task)
      idx += 1
      if idx % 200 == 0:
        _ = await asyncio.gather(*tasks)
        print(idx)
    results = await asyncio.gather(*tasks)
    printResults(results)

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--dataset', help='Location of the dataset', type=str, default='datasets/social-graph/socfb-Reed98/socfb-Reed98.mtx')
  parser.add_argument('--addr', help='IP Address of the server', type=str, default='127.0.0.1:8080')
  parser.add_argument('--rlfile', help='File to store register latencies', type=str, default='register_latencies.txt')
  parser.add_argument('--flfile', help='File to store follow latencies', type=str, default='follow_latencies.txt')
  args = parser.parse_args()
  filename = args.dataset
  with open(filename, 'r') as file:
    nodes = getNodes(file)
    edges = getEdges(file)

  addr = "http://" + args.addr
  loop = asyncio.get_event_loop()
  future = asyncio.ensure_future(register(addr, nodes))
  loop.run_until_complete(future)
  with open(args.rlfile, 'w+') as outf:
      outf.write('\n'.join([str(l) for l in register_latencies]))
  future = asyncio.ensure_future(follow(addr, edges))
  loop.run_until_complete(future)
  with open(args.flfile, 'w+') as outf:
      outf.write('\n'.join([str(l) for l in follow_latencies]))
