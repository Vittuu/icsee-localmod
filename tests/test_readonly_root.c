/* Verifica que a excecao SSH nao aceita diretorios mutaveis ou outras raizes. */
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static int ro=1,magic=0x28cd3d45,fail_vfs=0,fail_fs=0;
static const char *mounts="rootfs / rootfs rw 0 0\n/dev/root / cramfs ro,relatime 0 0\n";
static int fake_statvfs(const char *p,struct statvfs *s){(void)p;s->f_flag=ro?ST_RDONLY:0;return fail_vfs?-1:0;}
static int fake_statfs(const char *p,struct statfs *s){(void)p;s->f_type=magic;return fail_fs?-1:0;}
static FILE *fake_fopen(const char *path,const char *mode){(void)mode;if(strcmp(path,"/proc/mounts")||!mounts)return NULL;return fmemopen((void*)mounts,strlen(mounts),"r");}
#define statvfs(p,s) fake_statvfs(p,s)
#define statfs(p,s) fake_statfs(p,s)
#define fopen(p,m) fake_fopen(p,m)
#include "readonly_cramfs_root.h"
int main(void){
 assert(localmod_readonly_factory_root("/",1108));
 assert(!localmod_readonly_factory_root("/var",1108));
 assert(!localmod_readonly_factory_root("/",1234));
 ro=0;assert(localmod_readonly_factory_root("/",1108));
 mounts="/dev/root / cramfs rw,relatime 0 0\n";assert(!localmod_readonly_factory_root("/",1108));
 mounts=NULL;assert(!localmod_readonly_factory_root("/",1108));
 mounts="/dev/root / cramfs ro 0 0\nrootfs / rootfs rw 0 0\n";assert(!localmod_readonly_factory_root("/",1108));
 mounts="/dev/root / cramfs ro 0 0\n";fail_vfs=1;assert(localmod_readonly_factory_root("/",1108));fail_vfs=0;
 ro=1;
 magic=0xef53;assert(!localmod_readonly_factory_root("/",1108));magic=0x28cd3d45;
 fail_vfs=1;mounts=NULL;assert(!localmod_readonly_factory_root("/",1108));fail_vfs=0;
 fail_fs=1;assert(!localmod_readonly_factory_root("/",1108));
 puts("readonly_factory_root_policy: 11 verificacoes passaram");return 0;
}
