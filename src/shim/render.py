import sys
from PIL import Image, ImageDraw, ImageFont
from PIL import ImageOps
def font(size,bold):
    serif=bold&2; bold=bold&1
    n='DejaVuSerif' if serif else 'DejaVuSans'
    return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/%s%s.ttf'%(n,'-Bold' if bold else ''), max(6,int(size*0.86)))
for name in sys.argv[1:]:
    im=Image.new('L',(1072,1448),255); d=ImageDraw.Draw(im)
    for ln in open(name,encoding='utf8'):
        k,x,y,w,h,color,size,bold,flags,text=ln.rstrip('\n').split('\t',9); x,y,w,h,color,size,bold,flags=map(int,(x,y,w,h,color,size,bold,flags))
        g=(((color>>16)&255)*299+((color>>8)&255)*587+(color&255)*114)//1000
        if k=='B': im.paste(Image.frombytes('L',(w,h),bytes.fromhex(text)),(x,y)); continue
        if k=='I':
            if w>0 and h>0: box=(max(0,x),max(0,y),min(1072,x+w),min(1448,y+h)); im.paste(ImageOps.invert(im.crop(box)),box[:2])
            continue
        if k=='F': d.rectangle([x,y,x+w-1,y+h-1],fill=g)
        elif k=='R': d.rectangle([x,y,x+w-1,y+h-1],outline=g)
        elif k=='L': d.line([x,y,w,h],fill=g)
        elif k=='T':
            f=font(size,bold); lh=size*1.22
            def wrap(t):                     # cada \n del text ja s'ha convertit en espai pel simulador
                if h < size*1.8: return [t]
                out,cur=[],''
                for w in t.split(' '):
                    c=(cur+' '+w).strip()
                    if cur and d.textlength(c,font=f)>W:
                        out.append(cur); cur=w
                    else: cur=c
                out.append(cur)
                return out
            W=w
            lines=wrap(text); maxl=max(1,int(h//lh))
            if len(lines)>maxl:
                lines=lines[:maxl]
                if flags&512: lines[-1]=lines[-1]+'…'
            for n,t in enumerate(lines):
                if flags&512:
                    while d.textlength(t,font=f)>w and len(t)>1: t=t[:-2]+'…'
                tw=d.textlength(t,font=f); tx=x if flags&1 else (x+(w-tw)/2 if flags&2 else x+w-tw)
                if flags&64 and len(lines)==1: ty=y+h-size*1.12
                elif flags&32 and len(lines)==1: ty=y+(h-size)/2
                else: ty=y+n*lh
                d.text((tx,ty),t,font=f,fill=g)
    im.save(name.replace('.txt','.png'))
