import numpy as np, subprocess
sr=44100
def run(x,params,name='t'):
    x.astype('<f4').tofile(f'{name}_in.f32')
    subprocess.run(['./rp',f'{name}_in.f32',f'{name}_out.f32']+[f'{k}={v}' for k,v in params.items()],check=True)
    return np.fromfile(f'{name}_out.f32','<f4').astype(float)
def sine(f,dur=3.0,amp=0.3,harm=()):
    t=np.arange(int(dur*sr))/sr;x=np.sin(2*np.pi*f*t)
    for k,a in harm: x+=a*np.sin(2*np.pi*k*f*t)
    return amp*x*np.minimum(1,t/0.002)
# osc straight through: tiny mixer level (linear), filter wide open, flat contours
CLEAN=dict(osc1Lvl=0.1,osc3On=0,cutoff=5000,envAmt=6,fS=1,res=0,keyTrk2=0,dyn=0,tail=0,aS=1,vintage=0)
def spec(y,a,b):
    seg=y[int(a*sr):int(b*sr)];w=np.blackman(len(seg));F=np.abs(np.fft.rfft(seg*w))/(w.sum()/2);f=np.fft.rfftfreq(len(seg),1/sr);return f,F
def harmonics(y,f0,a,b,K=20):
    f,F=spec(y,a,b);df=f[1];H=[]
    for k in range(1,K+1):
        if k*f0>sr/2-200: break
        i=int(round(k*f0/df));w=max(3,int(3/ (df*(b-a)))+3);H.append(F[max(0,i-w):i+w+1].max())
    H=np.array(H)
    # inharmonic: everything not within +-6 bins of a harmonic
    mask=np.ones(len(F),bool)
    for k in range(1,int(sr/2/f0)+1):
        i=int(round(k*f0/df));mask[max(0,i-8):i+9]=False
    mask[:int(20/df)]=False
    return H,F[mask].max() if mask.any() else 0,f[mask][np.argmax(F[mask])] if mask.any() else 0
