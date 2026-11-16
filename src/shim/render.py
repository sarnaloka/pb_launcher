import sys
from PIL import Image, ImageDraw, ImageFont
def font(size,bold): return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf' if bold else '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', max(6,int(size*0.86)))
for name in sys.argv[1:]:
    im=Image.new('L',(1072,1448),255); d=ImageDraw.Draw(im)
    for ln in open(name,encoding='utf8'):
        k,x,y,w,h,color,size,bold,flags,text=ln.rstrip('\n').split('\t',9); x,y,w,h,color,size,bold,flags=map(int,(x,y,w,h,color,size,bold,flags))
        g=0 if color==0 else (170 if color==0xaaaaaa else (85 if color==0x555555 else 255))
        if k=='F': d.rectangle([x,y,x+w-1,y+h-1],fill=g)
        elif k=='R': d.rectangle([x,y,x+w-1,y+h-1],outline=g)
        elif k=='L': d.line([x,y,w,h],fill=g)
        elif k=='T':
            f=font(size,bold); t=text
            if flags&512:
                while d.textlength(t,font=f)>w and len(t)>1: t=t[:-2]+'…'
            tw=d.textlength(t,font=f); tx=x if flags&1 else (x+(w-tw)/2 if flags&2 else x+w-tw)
            ty=y+(h-size)/2 if flags&32 else y
            d.text((tx,ty),t,font=f,fill=g)
    im.save(name.replace('.txt','.png'))
