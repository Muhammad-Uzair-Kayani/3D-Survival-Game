"""Persistent, bounded NDJSON inference over authenticated loopback TCP."""
import argparse
import json
import socket
import sys
import time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from neural import Policy

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--port',type=int,required=True)
    parser.add_argument('--token',required=True)
    parser.add_argument('--test-fault',choices=['malformed','stale','timeout','duplicate'])
    args=parser.parse_args()
    policy=Policy()
    with socket.create_connection(('127.0.0.1',args.port),timeout=3) as connection:
        connection.settimeout(None)
        stream=connection.makefile('rwb')
        def send(q):
            stream.write(json.dumps(q,allow_nan=False).encode()+b'\n');stream.flush()
        send(dict(v=1,type='connected',token=args.token))
        while True:
            line=stream.readline(65537)
            if not line:break
            if len(line)>65536:raise ValueError('request exceeds 64 KiB')
            q={}
            try:
                q=json.loads(line)
                if not isinstance(q,dict) or type(q.get('v')) is not int or q['v']!=1:
                    raise ValueError('schema version')
                for key in ('request_id','episode'):
                    if type(q.get(key)) is not int:raise ValueError('correlation ID')
                reply=dict(v=1,request_id=q['request_id'],episode=q['episode'])
                if q.get('type')=='shutdown':break
                if q['type']=='hello':
                    reply.update(type='hello',architecture=[16,16,6],features=16,actions=6,max_batch=32)
                elif q['type']=='reset':
                    reply.update(type='reset',seed=q['episode'])
                elif q['type']=='episode_result':
                    if not isinstance(q.get('metrics'),dict):raise ValueError('metrics dictionary required')
                    reply.update(type='recorded')
                elif q['type']=='save_checkpoint':
                    policy.save(q['path']);reply.update(type='saved')
                elif q['type']=='load_policy':
                    policy=Policy(q['weights']);reply['type']='loaded'
                elif q['type']=='infer_batch':
                    agents=q['agents']
                    if not isinstance(agents,list) or len(agents)>32:raise ValueError('invalid batch')
                    seen=set();actions=[]
                    for agent in agents:
                        actor=agent['id']
                        if type(actor) is not int or actor<=0 or actor in seen:raise ValueError('duplicate/invalid actor')
                        seen.add(actor)
                        actions.append(dict(id=actor,intent=policy.choose(agent['features'],agent['valid_actions'],agent.get('biases')),target_id=0))
                    reply.update(type='intents',actions=actions)
                    if args.test_fault=='malformed':stream.write(b'{broken json}\n');stream.flush();continue
                    if args.test_fault=='stale':reply['request_id']-=1
                    if args.test_fault=='timeout':time.sleep(1.5)
                    if args.test_fault=='duplicate' and len(actions)>1:actions[1]['id']=actions[0]['id']
                else:raise ValueError('unknown request')
            except (ValueError,KeyError,TypeError) as error:
                reply=dict(v=1,type='error',request_id=q.get('request_id') if isinstance(q,dict) else None,episode=q.get('episode') if isinstance(q,dict) else None,error=str(error))
            send(reply)
        stream.close()

if __name__=='__main__':
    try:main()
    except (OSError,ValueError) as error:
        print(f'Policy service: {error}',file=sys.stderr);raise SystemExit(1)
