# Client de prova per al paquet FTP. Des de sim/, amb el simulador corrent test/tour_ftp.cmd en paral·lel:
#   (./uitest < ../test/tour_ftp.cmd > /tmp/ftp.txt &) ; python3 -I ../test/ftp_client.py 9
# Cal un sim/flash nou amb flash/Llibres i flash/applications.
import ftplib, io, os, sys, time
R = []
def ok(name, cond): R.append((name, bool(cond))); print(("OK  " if cond else "FAIL") + " " + name, flush=True)
def err(fn):
    try: fn(); return None
    except ftplib.all_errors as e: return str(e)
f = None
for i in range(60):
    try: f = ftplib.FTP(); f.connect('127.0.0.1', 2121, timeout=10); break
    except OSError: time.sleep(0.25)
ok("connecta", f is not None)
print(f.getwelcome())
pin = open('flash/system/config/escriptori_ftp.txt').read().strip()
e = err(lambda: f.login('lector', '0000' if pin != '0000' else '1111')); ok("contrasenya dolenta rebutjada", e and e.startswith('530'))
f.close(); f = ftplib.FTP(); f.connect('127.0.0.1', 2121, timeout=10)
f.login('lector', pin); ok("entra amb la contrasenya", True)
ok("PWD /", f.pwd() == '/')
names = f.nlst(); print("arrel:", names)
ok("system amagada", 'system' not in names and 'Llibres' in names)
lines = []; f.retrlines('LIST', lines.append); print(lines[:3])
ok("LIST format unix", all(l[0] in 'd-' and len(l.split()) >= 9 for l in lines))
ok("CWD system denegat", (err(lambda: f.cwd('/system')) or '').startswith('550'))
ok("CWD ../../ no surt", (err(lambda: f.cwd('../../etc')) or '').startswith('550'))
f.cwd('/Llibres'); ok("CWD Llibres", f.pwd() == '/Llibres')
data = os.urandom(300000) + "àèíòú".encode()
f.storbinary('STOR prova àccents.epub', io.BytesIO(data)); ok("STOR", True)
ok("SIZE", f.size('prova àccents.epub') == len(data))
buf = io.BytesIO(); f.retrbinary('RETR prova àccents.epub', buf.write); ok("RETR igual", buf.getvalue() == data)
ok("fitxer al disc", open('flash/Llibres/prova àccents.epub', 'rb').read() == data and not os.path.exists('flash/Llibres/prova àccents.epub.part'))
ok("MDTM", f.sendcmd('MDTM prova àccents.epub').startswith('213 '))
f.mkd('Nova'); ok("MKD", os.path.isdir('flash/Llibres/Nova'))
f.rename('prova àccents.epub', 'Nova/reanomenat.epub'); ok("RNFR/RNTO", os.path.exists('flash/Llibres/Nova/reanomenat.epub'))
ok("RMD no buida falla", (err(lambda: f.rmd('Nova')) or '').startswith('550'))
f.delete('Nova/reanomenat.epub'); f.rmd('Nova'); ok("DELE i RMD", not os.path.exists('flash/Llibres/Nova'))
ok("STOR a applications denegat", (err(lambda: f.storbinary('STOR /applications/x.app', io.BytesIO(b'x'))) or '').startswith('553'))
ok("DELE a system denegat", (err(lambda: f.delete('/system/config/escriptori_ftp.txt')) or '').startswith('550') and os.path.exists('flash/system/config/escriptori_ftp.txt'))
ok("RETR fora de l'arrel", (err(lambda: f.retrbinary('RETR /../../../etc/passwd', lambda b: None)) or '').startswith('550'))
ok("PORT rebutjat", (err(lambda: f.sendcmd('PORT 127,0,0,1,1,1')) or '').startswith('502'))
f.set_pasv(True)
f.sendcmd('EPSV') ; ok("EPSV", True)
f.quit()
time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 0)
f = ftplib.FTP(); f.connect('127.0.0.1', 2121, timeout=10); f.login('lector', pin)
f.storbinary('STOR /Llibres/segon.epub', io.BytesIO(b'hola' * 1000)); ok("STOR amb una altra pantalla", os.path.exists('flash/Llibres/segon.epub'))
f.quit()
print("RESUM", sum(1 for _, c in R if c), "de", len(R))
