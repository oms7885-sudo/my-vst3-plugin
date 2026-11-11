import numpy as np, subprocess, sys
S='/tmp/claude-0/-home-claude/4ba49e2c-9e25-5e0f-a365-7c7bd57f22a9/scratchpad'
fx=np.load(S+'/match/fx.npy'); sr=44100
X=np.fromfile(S+'/match/rec.f32','<f4').astype(float)
def run(exe,args=(),inp=S+'/match/rec.f32',out='o.f64'):
    subprocess.run([exe,inp,out]+list(args),check=True); d=np.fromfile(out,'<f8').reshape(-1,6); return d
def notes(d):
    """notes of the take from the engine's own triggers: (start sample, end sample, f0 from the reference)"""
    tr=np.where(np.diff(d[:,3])>0)[0]+1; N=[]
    for a,b in zip(tr,np.r_[tr[1:],len(X)]):
        k0=int(np.ceil((a+0.06*sr)/220)); k1=int((b-0.02*sr-2800)/220)
        if k1-k0<3: continue
        f=fx[k0:k1]; f=f[f>30]
        if len(f)<3: continue
        f0=np.median(f)
        if np.mean(np.abs(f/f0-1)<0.04)<0.8: continue      # the reference itself must be steady
        lev=np.sqrt(np.mean(X[a:b]**2)); N.append((a,b,f0,lev))
    return N
def course(d,N,step=0.01,span=0.04,upto=0.4):
    """share of notes on pitch (+-6 %) in each 10 ms step after the pick (rate over the next 40 ms)"""
    T=np.arange(0,upto,step); on=np.zeros(len(T)); up=np.zeros(len(T)); lo=np.zeros(len(T)); cnt=np.zeros(len(T)); o=d[:,0]
    for a,b,f0,lev in N:
        for j,t in enumerate(T):
            i0=a+int(t*sr); i1=i0+int(span*sr)
            if i1>=b-int(0.01*sr): break
            if t>=0.05:                 # past the attack, only where the reference itself says the note is sounding
                k0=max(0,int(np.floor((i0-600)/220))); k1=int(np.ceil((i1-2200)/220))+1; ref=fx[k0:max(k1,k0+1)]
                if not np.all(np.abs(ref/f0-1)<0.04): continue
            r=(o[i1]-o[i0])/span/f0; cnt[j]+=1; on[j]+=abs(r-1)<0.06; up[j]+=abs(r-2)<0.12; lo[j]+=r<=0.94
    return T,on/np.maximum(cnt,1),up/np.maximum(cnt,1),lo/np.maximum(cnt,1),cnt
def summary(d,N):
    T,on,up,lo,cnt=course(d,N); w=cnt/cnt.sum()
    early=T<0.05; late=T>=0.05
    return 'notes %d | first 50 ms: on %.2f up %.2f low %.2f | after: on %.3f up %.3f low %.3f'%(len(N),np.average(on[early],weights=cnt[early]),np.average(up[early],weights=cnt[early]),np.average(lo[early],weights=cnt[early]),np.average(on[late],weights=cnt[late]),np.average(up[late],weights=cnt[late]),np.average(lo[late],weights=cnt[late]))
if __name__=='__main__':
    cfg=[('old (tracking off)',"./oscd_new",['fundTrack=0']),('new, defaults','./oscd_new',[])]
    cfg+=[('new, '+a,'./oscd_new',[a]) for a in sys.argv[1:]]
    N=None
    for name,exe,a in cfg:
        d=run(exe,a)
        if N is None: N=notes(d)
        print('%-22s'%name,summary(d,N))
