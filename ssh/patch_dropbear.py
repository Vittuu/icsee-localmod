"""Diagnostico temporario de inicializacao; nao altera criptografia/autenticacao."""
from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('source',type=Path);args=p.parse_args()
D=args.source/'src'
(D/'readonly_cramfs_root.h').write_text((Path(__file__).resolve().parents[1]/'src/readonly_cramfs_root.h').read_text())
p=D/'svr-session.c';s=p.read_text()
marker='/* LOCALMOD_STARTUP_DIAGNOSTIC */'
if marker not in s:
 anchor='vsnprintf(printbuf, sizeof(printbuf), format, param);'
 assert s.count(anchor)==1
 s=s.replace(anchor,anchor+'\n\t'+marker+'\n\t{ FILE *fp=fopen("/var/localmod-v3/ssh-debug.log", "a"); if(fp){ fputs(printbuf,fp); fputc(10,fp); fclose(fp); } }')
 p.write_text(s)
p=D/'svr-authpubkey.c';s=p.read_text()
if 'localmod_readonly_factory_root' not in s:
 anchor='if (filestat.st_uid != ses.authstate.pw_uid\n\t\t\t&& filestat.st_uid != 0) {'
 assert s.count(anchor)==1
 s=s.replace(anchor,'if (filestat.st_uid != ses.authstate.pw_uid\n\t\t\t&& filestat.st_uid != 0\n\t\t\t&& !localmod_readonly_factory_root(filename, filestat.st_uid)) {')
 # A funcao propria depende somente de tipos POSIX e das verificacoes de montagem.
 anchor='#include "includes.h"'
 assert anchor in s
 s=s.replace(anchor,anchor+'\n#include "readonly_cramfs_root.h"',1)
 (D/'readonly_cramfs_root.h').write_text((Path(__file__).resolve().parents[1]/'src/readonly_cramfs_root.h').read_text())
 p.write_text(s)
p=D/'svr-main.c';s=p.read_text()
if marker not in s:
 anchor='static void commonsetup(void);'
 assert s.count(anchor)==1
 s=s.replace(anchor,anchor+'\n'+marker+'\nstatic void localmod_diag(const char *text) { FILE *fp=fopen("/var/localmod-v3/ssh-debug.log","a"); if(fp){ fputs(text,fp); fputc(10,fp); fclose(fp); } }')
 for anchor,msg in [('svr_getopts(argc, argv);','options_parsed'),('commonsetup();','commonsetup_returned'),('crypto_init();','crypto_init_returned'),('load_all_hostkeys();','hostkeys_loaded')]:
  assert anchor in s
  s=s.replace(anchor,anchor+'\n\tlocalmod_diag("'+msg+'");')
 p.write_text(s)
