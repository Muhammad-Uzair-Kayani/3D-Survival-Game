"""Run integration checks against the source-built module, not a stock Godot binary."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]

def run(executable,case,frames=360,python=False,expect_fallback=False):
    output=ROOT/'reports';output.mkdir(exist_ok=True);report=output/(case+'.json')
    report.unlink(missing_ok=True)
    args=[str(executable),'--headless','--fixed-fps','60','--max-fps','0','--path',str(ROOT/'game'),'--','--smoke','--case',case,'--frames',str(frames),'--report',str(report)]
    args+=['--bots','1' if case=='combat' else '2' if case=='autoplay' else '5' if case in ('arctic_hostile','compound_hostile') else '0']
    if case.startswith('python_fault_'):args+=['--autoplay']
    if python:args+=['--python',sys.executable]
    if case=='fallback':args+=['--python',str(output/'missing-python-executable')]
    env=os.environ.copy();env['XDG_DATA_HOME']=str(output/'user-data')
    if os.name=='nt':
        env['APPDATA']=str(output/'user-data');env['LOCALAPPDATA']=str(output/'local-data')
    with (output/(case+'.log')).open('w') as log:
        p=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,env=env,timeout=300 if case in ("compound","compound_hostile") else 90)
    if p.returncode!=0 or not report.exists():raise RuntimeError(f'{case} failed; inspect {output/(case+".log")}')
    log_text=(output/(case+'.log')).read_text(errors='replace')
    if 'SCRIPT ERROR' in log_text or ('ERROR:' in log_text and case not in ('python_fault_malformed','fallback')):raise RuntimeError(f'{case}: engine/script error; inspect log')
    result=json.loads(report.read_text())
    if not result.get('test_passed') or result['navigation_polygons']<1:raise RuntimeError(f'{case}: invalid test report')
    if python and not expect_fallback and result['python_failures']:raise RuntimeError('Neural test used fallback')
    if (case=='fallback' or expect_fallback) and result['python_failures']<1:raise RuntimeError('Expected explicit authored fallback')
    print(case,'passed; navigation polygons:',result['navigation_polygons'])
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--engine',type=Path,required=True);p.add_argument('--autoplay',action='store_true');a=p.parse_args();engine=a.engine.resolve()
    for case in ('physics','combat','gate','gate_save','gate_negative','save','assets','social','melee','lifecycle'):run(engine,case)
    run(engine,'python',frames=120,python=True)
    run(engine,'fallback',frames=120)
    run(engine,'python_exit',frames=240,python=True,expect_fallback=True)
    for fault in ('malformed','stale','timeout','duplicate'):run(engine,'python_fault_'+fault,frames=240,python=True,expect_fallback=True)
    invalid_env=os.environ.copy();invalid_env['XDG_DATA_HOME']=str(ROOT/'reports/user-data')
    for values in (['--bots','9'],['--seed','abc'],['--controller','unknown'],['--world','unknown'],['--frames'],['--unknown']):
        invalid=subprocess.run([str(engine),'--headless','--path',str(ROOT/'game'),'--',*values],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=invalid_env,timeout=15)
        if invalid.returncode!=64:raise RuntimeError('Invalid CLI arguments were not rejected: '+str(values))
    for case,frames in (('arctic_crossfire',240),('arctic_equipment',240),('arctic_social',240),('arctic_interactions',480),('arctic_animation',500),('profiles',360)):
        run(engine,case,frames=frames)
    for case,frames in (('compound_animation',120),('compound_security',320),('compound_routes',180),('compound_inventory',180),('compound_engagement',900),('compound_nav',3600),('compound_policy',180),('compound_save',180),('compound_learning',180),('compound_orders',180),('compound_beacon',300)):
        run(engine,case,frames=frames)
    if a.autoplay:
        compound=run(engine,'compound',frames=30000)
        if compound['phase']!='Victory' or compound['arrived']!=5:raise RuntimeError('Frostworks peaceful team did not all arrive')
        compound_hostile=run(engine,'compound_hostile',frames=30000)
        if compound_hostile['shots_hit']<1:raise RuntimeError('Frostworks hostile run had no combat')
        arctic=run(engine,'arctic',frames=30000)
        if arctic['phase']!='Victory' or arctic['arrived']!=5:raise RuntimeError('Arctic peaceful team did not all arrive')
        hostile=run(engine,'arctic_hostile',frames=18000)
        active=[actor for actor in hostile['actors'] if actor['id']>=10 and actor['health']>0]
        if hostile['shots_hit']<1 or sum(actor['travelled']>5 for actor in hostile['actors'] if actor['id']>=10)<3:raise RuntimeError('Arctic distractors did not interact with the world')
        peaceful=run(engine,'peaceful',frames=18000)
        if peaceful['phase']!='Victory' or peaceful['arrived']!=5:raise RuntimeError('Peaceful team did not all arrive')
        (ROOT/'reports/peaceful-first.json').write_text(json.dumps(peaceful))
        repeated=run(engine,'peaceful',frames=18000)
        def fingerprint(data):
            return (data['phase'],data['arrived'],data['shots_hit'],data['journal'],[(a['health'],a['state'],[round(v,3) for v in a['position']]) for a in data['actors']])
        if fingerprint(peaceful)!=fingerprint(repeated):raise RuntimeError('Same-build seeded run diverged')
        print('same-build seeded replay passed')
        for case in ('solo','autoplay'):
            r=run(engine,case,frames=18000)
            if r['phase']!='Victory':raise RuntimeError('Authored survivors did not complete '+case)
if __name__=='__main__':main()
