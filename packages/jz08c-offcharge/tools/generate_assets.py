#!/usr/bin/env python3
"""Generate deterministic 128x128 little-endian RGB565 screen backgrounds; stdlib only."""
import argparse, math, os, struct
W=H=128
FONT={
"A":[14,17,17,31,17,17,17],"B":[30,17,17,30,17,17,30],"C":[14,17,16,16,16,17,14],
"D":[30,17,17,17,17,17,30],"E":[31,16,16,30,16,16,31],"F":[31,16,16,30,16,16,16],
"G":[14,17,16,23,17,17,15],"H":[17,17,17,31,17,17,17],"I":[14,4,4,4,4,4,14],
"J":[7,2,2,2,2,18,12],"K":[17,18,20,24,20,18,17],"L":[16,16,16,16,16,16,31],
"M":[17,27,21,21,17,17,17],"N":[17,25,21,19,17,17,17],"O":[14,17,17,17,17,17,14],
"P":[30,17,17,30,16,16,16],"Q":[14,17,17,17,21,18,13],"R":[30,17,17,30,20,18,17],
"S":[15,16,16,14,1,1,30],"T":[31,4,4,4,4,4,4],"U":[17,17,17,17,17,17,14],
"V":[17,17,17,17,17,10,4],"W":[17,17,17,21,21,21,10],"X":[17,17,10,4,10,17,17],
"Y":[17,17,10,4,4,4,4],"Z":[31,1,2,4,8,16,31]," ": [0]*7,
}
def rgb565(c): r,g,b=c; return ((r>>3)<<11)|((g>>2)<<5)|(b>>3)
def canvas(c): return [[c for _ in range(W)] for __ in range(H)]
def rect(im,x,y,w,h,c):
    for yy in range(max(0,y),min(H,y+h)):
        for xx in range(max(0,x),min(W,x+w)): im[yy][xx]=c
def text(im,s,x,y,c):
    for ch in s.upper():
        for yy,row in enumerate(FONT.get(ch,[0]*7)):
            for xx in range(5):
                if row & (1<<(4-xx)): rect(im,x+xx,y+yy,1,1,c)
        x+=6
def logo(im,x,y,size):
    cyan=(0,181,226); white=(255,255,255); navy=(0,43,73)
    rect(im,x,y,size,size,cyan)
    cx=x+size//2; base=y+size*62//100
    # Simplified wireless arcs and open power ring from the OpenWrt 2020 icon.
    for r in (size*42//100,size*29//100,size*16//100):
        for py in range(y+size//8,base):
            for px in range(x+size//8,x+size-size//8):
                dx=px-cx; dy=base-py; q=math.isqrt(dx*dx+dy*dy)
                if dy>abs(dx)//3 and abs(q-r)<=max(1,size//24): im[py][px]=white
    rr=max(2,size//12)
    for py in range(base-rr,base+rr+1):
        for px in range(cx-rr,cx+rr+1):
            if (px-cx)**2+(py-base)**2<=rr*rr: im[py][px]=white
    outer=size*47//100; inner=size*38//100
    for py in range(y+size//4,y+size-size//10):
        for px in range(x+size//10,x+size-size//10):
            dx=px-cx; dy=py-(y+size//2); q=math.isqrt(dx*dx+dy*dy)
            if inner<=q<=outer and (dy>0 or abs(dx)>size//3): im[py][px]=navy
def save(im,path):
    with open(path,"wb") as f:
        for row in im: f.write(b"".join(struct.pack("<H",rgb565(c)) for c in row))
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--out-dir",required=True); a=ap.parse_args()
    os.makedirs(a.out_dir,exist_ok=True); white=(244,249,250)
    off=canvas((4,24,40))
    for y in range(H):
        for x in range(W): off[y][x]=(4,24+y*10//H,40+y*10//H)
    logo(off,40,7,48); text(off,"OPENWRT",34,63,white); text(off,"POWERING OFF",28,91,white)
    save(off,os.path.join(a.out_dir,"powering-off.rgb565"))
    charge=canvas((4,24,40))
    for y in range(H):
        for x in range(W): charge[y][x]=(4,24+y*18//H,40+y*18//H)
    logo(charge,52,5,24); text(charge,"CHARGING",37,36,white)
    save(charge,os.path.join(a.out_dir,"charging-background.rgb565"))
if __name__=="__main__": main()
