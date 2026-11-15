"""Evolve 374-gene policies using real Godot episodes; report held-out results."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import json
import os
from pathlib import Path
import random
import statistics
import subprocess
import sys
from neural import Policy, GENES
ROOT=Path(__file__).resolve().parents[1]
TRAIN=(101,102,103,104)
VALIDATION=(501,502,503,504)
TEST=(901,902,903,904)

def atomic(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_suffix('.tmp');temp.write_text(json.dumps(data,allow_nan=False));temp.replace(path)

def tuples(value):
    return tuple(tuples(x) for x in value) if isinstance(value,list) else value

def breed(population,scores,rng):
    ranked=sorted(range(len(population)),key=scores.__getitem__,reverse=True)
    children=[population[i][:] for i in ranked[:2]]
    def select():return population[max(rng.sample(range(len(population)),min(3,len(population))),key=scores.__getitem__)]
    while len(children)<len(population):
        a,b=select(),select()
        children.append([max(-5,min(5,(a[i] if rng.random()<.5 else b[i])+(rng.gauss(0,.2) if rng.random()<.08 else 0))) for i in range(GENES)])
    return children

def episode(executable,weights,seed,output,serial,controller='neural',frames=18000,world='arctic',bots=5):
    folder=Path(output)/str(serial);folder.mkdir(parents=True,exist_ok=True)
    report=folder/'result.json';model=folder/'candidate.json';report.unlink(missing_ok=True)
    mode='nomemory' if controller=='neural_nomemory' else controller
    args=[str(Path(executable).resolve()),'--headless','--fixed-fps','60','--max-fps','0','--path',str(ROOT/'game'),'--','--autoplay','--world',world,'--bots',str(bots),'--seed',str(seed),'--controller',mode,'--frames',str(frames),'--report',str(report.resolve())]
    if controller.startswith('neural'):
        Policy(weights).save(model);args+=['--python',sys.executable,'--policy',str(model.resolve())]
    env=os.environ.copy();env['XDG_DATA_HOME']=str((folder/'user-data').resolve())
    if os.name=='nt':env['APPDATA']=str(folder/'app-data');env['LOCALAPPDATA']=str(folder/'local-data')
    with (folder/'engine.log').open('w') as log:
        result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,env=env,timeout=180)
    if result.returncode not in (0,2) or not report.exists():raise RuntimeError(f'Godot evaluation failed; see {folder}/engine.log')
    data=json.loads(report.read_text())
    if data['python_failures']:raise RuntimeError('Rejecting neural episode that used transport fallback')
    if data['navigation_polygons']<=0:raise RuntimeError('Invalid navigation world')
    return data

def samples(args,weights,seeds,output,serial,controller='neural'):
    def run(seed):return episode(args.engine,weights,seed,output,f'{serial}-{seed}',controller,args.frames,args.world,args.bots)
    with ThreadPoolExecutor(max_workers=args.workers) as pool:return list(pool.map(run,seeds))

def train(args):
    engine_sha256=hashlib.sha256(Path(args.engine).resolve().read_bytes()).hexdigest()
    output=Path(args.output).resolve();rng=random.Random(args.seed);history=[];start=0;champion=None;best=-float('inf')
    if args.resume:
        state=json.loads(Path(args.resume).read_text())
        if state.get('version')!=3 or state.get('engine_sha256')!=engine_sha256 or tuple(state['training_seeds'])!=TRAIN or tuple(state['validation_seeds'])!=VALIDATION or state.get('evaluation_frames',args.frames)!=args.frames or state.get('world','lab')!=args.world or state.get('bots',2)!=args.bots:raise ValueError('incompatible checkpoint or evaluation frame limit')
        population=[Policy(w).weights for w in state['population']];rng.setstate(tuples(state['rng_state']));history=state['history'];start=state['generation'];best=state['best'];champion=state['champion']
    else:population=[[rng.uniform(-1,1) for _ in range(GENES)] for _ in range(args.population)]
    for generation in range(start,start+args.generations):
        scores=[]
        # Each candidate owns its model/report directory; seed workers share no files.
        jobs=[(genome,seed) for genome in range(len(population)) for seed in TRAIN]
        def run(job):
            genome,seed=job
            return episode(args.engine,population[genome],seed,output/'episodes',f'{generation}-{genome}-{seed}',frames=args.frames,world=args.world,bots=args.bots)
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            evaluated=list(pool.map(run,jobs))
        for genome in range(len(population)):
            data=evaluated[genome*len(TRAIN):(genome+1)*len(TRAIN)]
            scores.append(statistics.mean(float(d['fitness']) for d in data))
        winner=max(range(len(population)),key=scores.__getitem__)
        validation=samples(args,population[winner],VALIDATION,output/'validation',str(generation))
        validation_score=statistics.mean(d['fitness'] for d in validation)
        if validation_score>best:best=validation_score;champion=population[winner][:]
        if generation==0:Policy(population[winner]).save(output/'initial-best.json')
        history.append(dict(generation=generation,best=max(scores),mean=statistics.mean(scores),stdev=statistics.pstdev(scores),validation=validation_score))
        print(json.dumps(history[-1]),flush=True)
        population=breed(population,scores,rng)
        Policy(champion).save(output/'champion.json')
        atomic(output/'latest.json',dict(version=3,engine_sha256=engine_sha256,feature_contract='policy16-v1',scenario='frostworks-2' if args.world=='arctic' else 'lab-1',world=args.world,bots=args.bots,evaluation_frames=args.frames,training_seeds=TRAIN,validation_seeds=VALIDATION,generation=generation+1,population=population,rng_state=rng.getstate(),history=history,best=best,champion=champion))
        with (output/'generations.csv').open('w',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=['generation','best','mean','stdev','validation']);writer.writeheader();writer.writerows(history)

def evaluate(args):
    weights=Policy.load(args.model).weights;output=Path(args.output).resolve();rows=[]
    for controller in ('neural','rule','random','neural_nomemory'):
        data=samples(args,weights,TEST,output/'episodes',controller,controller)
        rows.append(dict(controller=controller,world=args.world,bots=args.bots,episodes=len(data),seeds=TEST,arrival_rate=statistics.mean(d['arrived']/5 for d in data),fitness=statistics.mean(d['fitness'] for d in data),fitness_stdev=statistics.pstdev(d['fitness'] for d in data),mean_seconds=statistics.mean(d['elapsed'] for d in data),health_at_arrival=statistics.mean(d['health_at_arrival'] for d in data),damage_received=statistics.mean(d['damage_received'] for d in data),path_cost=statistics.mean(d['path_cost'] for d in data),cooperative_uses=statistics.mean(d['cooperative_uses'] for d in data)))
    atomic(output/'comparison.json',rows)
    with (output/'comparison.csv').open('w',newline='') as file:
        writer=csv.DictWriter(file,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    print(json.dumps(rows,indent=2))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('command',choices=['train','evaluate']);parser.add_argument('--engine',required=True);parser.add_argument('--output',default=str(ROOT/'reports/training'));parser.add_argument('--population',type=int,default=30);parser.add_argument('--generations',type=int,default=10);parser.add_argument('--workers',type=int,default=1,help='Use 1 on an 8 GB PC');parser.add_argument('--seed',type=int,default=42);parser.add_argument('--resume');parser.add_argument('--model');parser.add_argument('--frames',type=int,default=18000);parser.add_argument('--world',choices=['arctic','lab'],default='arctic');parser.add_argument('--bots',type=int,choices=range(9),default=5);args=parser.parse_args()
    if not 2<=args.population<=1000 or not 1<=args.generations<=10000 or not 1<=args.workers<=8:parser.error('population 2..1000; generations 1..10000; workers 1..8')
    if not 60<=args.frames<=180000:parser.error('frames 60..180000')
    if args.command=='evaluate' and not args.model:parser.error('--model required for evaluation')
    train(args) if args.command=='train' else evaluate(args)
if __name__=='__main__':main()
