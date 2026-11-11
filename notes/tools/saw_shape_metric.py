import numpy as np,sys
import m1c
from m1c import SG,f0_ac,x,sr
from util import run,CLEAN
def analyse(y,label,quiet=False):
    R=[]
    for t,a,b in SG:
        xs=x[a:b];ys=y[a:b];fd,cd=f0_ac(xs)
        if cd<0.8: continue
        w=np.blackman(len(ys));F=np.abs(np.fft.rfft(ys*w,8*len(ys)));f=np.fft.rfftfreq(8*len(ys),1/sr)
        H=np.array([F[(f>k*fd*0.96)&(f<k*fd*1.04)].max() for k in range(1,9)])
        d=20*np.log10(H/H[0])-20*np.log10(1/np.arange(1,9))
        R.append((t,fd,np.abs(d[1:]).mean(),d))
    dev=np.array([r[2] for r in R]);D=np.array([r[3] for r in R])
    print(f'[{label}] 음 {len(R)}개: 이상적 톱니 대비 배음 2~8 평균 편차 중앙 {np.median(dev):.1f} dB, p75 {np.percentile(dev,75):.1f}, p90 {np.percentile(dev,90):.1f}, 최대 {dev.max():.1f} dB')
    if not quiet: print('   배음별 편차 중앙(최소~최대), 2~6:',' '.join(f'{np.median(D[:,k]):+.1f}({D[:,k].min():+.0f}~{D[:,k].max():+.0f})' for k in range(1,6)))
    return R
if __name__=='__main__':
    y0=run(x,CLEAN,'e0');y1=run(x,{**CLEAN,'fundTrack':1},'e1')
    R0=analyse(y0,'지금 엔진');R1=analyse(y1,'기본음 추적')
    for r0,r1 in zip(R0,R1): print(f't={r0[0]:6.2f}s DI {r0[1]:6.1f}Hz | 지금 {r0[2]:4.1f}dB | 추적 {r1[2]:4.1f}dB')
