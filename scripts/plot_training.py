"""Optional report plotting; requires matplotlib, never needed to play."""
import argparse,csv,json
from pathlib import Path

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--training',type=Path,required=True);parser.add_argument('--comparison',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    rows=list(csv.DictReader(args.training.open()));comparison=json.loads(args.comparison.read_text())
    fig,axes=plt.subplots(1,2,figsize=(12,4),layout='constrained')
    generations=[int(r['generation'])+1 for r in rows]
    for key,label in [('best','Best training fitness'),('mean','Mean training fitness'),('validation','Winner validation fitness')]:axes[0].plot(generations,[float(r[key]) for r in rows],marker='o',label=label)
    axes[0].set(xlabel='Generation',ylabel='Episode fitness',title='Actual Godot training episodes');axes[0].legend(fontsize=8)
    axes[1].bar([r['controller'] for r in comparison],[r['arrival_rate']*100 for r in comparison],color=['#368a7e','#e8b760','#647485','#a97795'])
    axes[1].set(ylabel='Survivors arriving (%)',ylim=(0,100),title='Held-out seeds 901–904');axes[1].tick_params(axis='x',labelrotation=15)
    args.output.parent.mkdir(parents=True,exist_ok=True);fig.savefig(args.output,dpi=160)
if __name__=='__main__':main()
