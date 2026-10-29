#define _POSIX_C_SOURCE 200809L
/* Painel local: somente loopback, autenticacao nativa, sem shell ou acesso MTD. */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "cJSON.h"
#include "tomcrypt.h"
#include "panel.h"
#define MAXJSON 262144
static int backend_port=34567, controls=0;
typedef struct { int fd; uint32_t session,seq; } Camera;
static cJSON *item(cJSON *j,const char *k){return cJSON_GetObjectItemCaseSensitive(j,k);}
static const char *str(cJSON *j){return cJSON_IsString(j)?j->valuestring:"";}
static void wipe(void *p,size_t n){volatile unsigned char *q=p;while(n--)*q++=0;}
static int exact(int fd,void *p,size_t n,int writing){char *q=p;while(n){ssize_t r=writing?send(fd,q,n,MSG_NOSIGNAL):recv(fd,q,n,0);if(r<0&&errno==EINTR)continue;if(r<=0)return -1;n-=r;q+=r;}return 0;}
static void le32(unsigned char *b,uint32_t v){for(int i=0;i<4;i++)b[i]=(unsigned char)(v>>(8*i));}
static uint32_t rd32(const unsigned char *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static int connect_local(int port){int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;struct timeval t={3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof t);setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof t);struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(port),.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};if(connect(fd,(void*)&a,sizeof a)){close(fd);return -1;}return fd;}
static cJSON *exchange(Camera *c,int code,cJSON *j){
 char *s=cJSON_PrintUnformatted(j);if(!s)return NULL;size_t n=strlen(s);unsigned char h[20]={255,0};
 le32(h+4,c->session);le32(h+8,c->seq++);h[14]=code&255;h[15]=(code>>8)&255;le32(h+16,n+2);
 int bad=exact(c->fd,h,20,1)||exact(c->fd,s,n,1)||exact(c->fd,"\n\0",2,1);wipe(s,n);free(s);if(bad)return NULL;
 char *body=malloc(MAXJSON+1);if(!body)return NULL;size_t used=0;int fragments=0,total=0;
 do{
  if(exact(c->fd,h,20,0)||h[0]!=255||h[1]>1||(h[14]|h[15]<<8)!=code+1){free(body);return NULL;}
  uint32_t len=rd32(h+16);if(len>MAXJSON-used){free(body);return NULL;}
  if(fragments==0){total=h[12]?h[12]:1;}if((total>1&&h[13]!=fragments)||fragments>=32){free(body);return NULL;}
  if(exact(c->fd,body+used,len,0)){free(body);return NULL;}used+=len;fragments++;c->session=rd32(h+4);
 }while(fragments<total);
 while(used&&(body[used-1]=='\0'||body[used-1]=='\n'))used--;body[used]=0;
 cJSON *out=cJSON_ParseWithLength(body,used+1);wipe(body,used);free(body);return out;
}
static int good(cJSON *j){cJSON *r=item(j,"Ret");return cJSON_IsNumber(r)&&(r->valueint==100||r->valueint==150);}
static cJSON *command(Camera *c,int code,const char *name,cJSON *value){cJSON *j=cJSON_CreateObject();char sid[16];snprintf(sid,sizeof sid,"0x%08X",c->session);cJSON_AddStringToObject(j,"Name",name);cJSON_AddStringToObject(j,"SessionID",sid);if(value)cJSON_AddItemToObject(j,name,value);cJSON *r=exchange(c,code,j);cJSON_Delete(j);return r;}
static int login(Camera *c,const char *username,const char *password){
 unsigned char digest[16];hash_state hs;md5_init(&hs);md5_process(&hs,(const unsigned char*)password,strlen(password));md5_done(&hs,digest);
 const char *alphabet="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";char folded[9];for(int i=0;i<8;i++)folded[i]=alphabet[(digest[i*2]+digest[i*2+1])%62];folded[8]=0;wipe(digest,sizeof digest);wipe(&hs,sizeof hs);
 c->fd=connect_local(backend_port);c->session=c->seq=0;if(c->fd<0){wipe(folded,sizeof folded);return -1;}
 cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"EncryptType","MD5");cJSON_AddStringToObject(j,"LoginType","DVRIP-Web");cJSON_AddStringToObject(j,"UserName",username);cJSON_AddStringToObject(j,"PassWord",folded);wipe(folded,sizeof folded);
 cJSON *r=exchange(c,1000,j);cJSON *secret=item(j,"PassWord");if(secret)wipe(secret->valuestring,strlen(secret->valuestring));cJSON_Delete(j);int ok=good(r);if(ok)c->session=(uint32_t)strtoul(str(item(r,"SessionID")),NULL,16);cJSON_Delete(r);return ok?0:-1;
}
static void logout(Camera *c){if(c->fd>=0){cJSON *r=command(c,1002,"Logout",NULL);cJSON_Delete(r);close(c->fd);c->fd=-1;}}
static cJSON *error(const char *msg){cJSON *r=cJSON_CreateObject();cJSON_AddStringToObject(r,"erro",msg);return r;}
static void redact(cJSON *j){for(cJSON *p=j?j->child:NULL;p;p=p->next){if(p->string&&(!strcmp(p->string,"SessionID")||!strcmp(p->string,"SerialNo")||!strcmp(p->string,"PassWord"))){if(cJSON_IsString(p))wipe(p->valuestring,strlen(p->valuestring));cJSON_SetValuestring(p,"[omitido]");}else if(cJSON_IsObject(p)||cJSON_IsArray(p))redact(p);}}
static int allowed_read(const char *n){const char *names[]={"SystemInfo","StorageInfo","WorkState","Record","Camera.Param","Camera.ParamEx","Camera.WhiteLight","Uart.PTZPreset","Simplify.Encode",NULL};for(int i=0;names[i];i++)if(!strcmp(n,names[i]))return 1;return 0;}
static cJSON *ptz(Camera *c,const char *dir,int preset){cJSON *v=cJSON_CreateObject(),*p=cJSON_CreateObject(),*aux=cJSON_CreateObject();cJSON_AddStringToObject(v,"Command",dir);cJSON_AddNumberToObject(p,"Channel",0);cJSON_AddNumberToObject(p,"Step",1);cJSON_AddNumberToObject(p,"Preset",preset);cJSON_AddNumberToObject(p,"Tour",0);cJSON_AddStringToObject(p,"MenuOpts","Enter");cJSON_AddStringToObject(p,"Pattern","Start");cJSON_AddNumberToObject(aux,"Number",0);cJSON_AddStringToObject(aux,"Status","On");cJSON_AddItemToObject(p,"AUX",aux);cJSON_AddItemToObject(v,"Parameter",p);return command(c,1400,"OPPTZControl",v);}
static int hhmm(const char *s,int end){if(strlen(s)!=5||s[2]!=':'||!isdigit((unsigned char)s[0])||!isdigit((unsigned char)s[1])||!isdigit((unsigned char)s[3])||!isdigit((unsigned char)s[4]))return -1;int h=(s[0]-48)*10+s[1]-48,m=(s[3]-48)*10+s[4]-48;if(m>59||h>23+(!!end)||(h==24&&m))return -1;return h*60+m;}
static int date_ok(const char *s){if(strlen(s)!=19)return 0;for(int i=0;i<19;i++){char e=i==4||i==7?'-':i==10?' ':i==13||i==16?':':0;if(e?s[i]!=e:!isdigit((unsigned char)s[i]))return 0;}return 1;}
static cJSON *patch(Camera *c,cJSON *req){
 const char *n=str(item(req,"name")),*f=str(item(req,"field"));cJSON *v=item(req,"value");int valid=0;
 if(!strcmp(n,"Record")&&!strcmp(f,"RecordMode"))valid=!strcmp(str(v),"ManualRecord")||!strcmp(str(v),"ClosedRecord")||!strcmp(str(v),"ConfigRecord");
 if(!strcmp(n,"Camera.ParamEx")&&!strcmp(f,"OneKeyOpenWL"))valid=cJSON_IsBool(v);
 if(!strcmp(n,"Camera.Param")&&!strcmp(f,"DayNightColor"))valid=!strcmp(str(v),"0x00000000")||!strcmp(str(v),"0x00000001")||!strcmp(str(v),"0x00000002")||!strcmp(str(v),"0x00000003");
 if(!valid)return error("Alteracao fora da lista permitida");
 cJSON *old=command(c,1042,n,NULL),*a=item(old,n),*channel=cJSON_GetArrayItem(a,0);if(!good(old)||!cJSON_IsObject(channel)||!item(channel,f)){cJSON_Delete(old);return error("Formato nativo nao reconhecido");}
 cJSON *copy=cJSON_Duplicate(a,1);cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(copy,0),f,cJSON_Duplicate(v,1));cJSON *r=command(c,1040,n,copy);if(!good(r)){cJSON_Delete(old);return r?r:error("Falha ao aplicar");}cJSON_Delete(r);
 cJSON *check=command(c,1042,n,NULL);cJSON *actual=item(cJSON_GetArrayItem(item(check,n),0),f);if(!good(check)||!cJSON_Compare(actual,v,1)){cJSON_Delete(check);cJSON *back=command(c,1040,n,cJSON_Duplicate(a,1));cJSON_Delete(back);cJSON_Delete(old);return error("Leitura divergente; restauracao solicitada");}cJSON_Delete(old);return check;
}
static cJSON *schedule(Camera *c,cJSON *req){
 const char *start=str(item(req,"start")),*end=str(item(req,"end"));int b=hhmm(start,0),e=hhmm(end,1);cJSON *days=item(req,"days");int motion=cJSON_IsTrue(item(req,"motion"));
 if(b<0||e<=b||cJSON_GetArraySize(days)!=7)return error("Horario ou dias invalidos; use um periodo no mesmo dia");for(int d=0;d<7;d++)if(!cJSON_IsBool(cJSON_GetArrayItem(days,d)))return error("Dias invalidos");
 cJSON *old=command(c,1042,"Record",NULL),*a=item(old,"Record"),*ch=cJSON_GetArrayItem(a,0);if(!good(old)||!cJSON_IsObject(ch)||!item(ch,"TimeSection")||!item(ch,"Mask")){cJSON_Delete(old);return error("Agenda nativa nao reconhecida");}
 cJSON *copy=cJSON_Duplicate(a,1),*target=cJSON_GetArrayItem(copy,0),*times=cJSON_CreateArray(),*masks=cJSON_CreateArray();
 for(int d=0;d<7;d++){cJSON *t=cJSON_CreateArray(),*m=cJSON_CreateArray();int enabled=cJSON_IsTrue(cJSON_GetArrayItem(days,d));for(int x=0;x<6;x++){char section[32];snprintf(section,sizeof section,"%d %s:00-%s:00",enabled&&x==0,start,end);cJSON_AddItemToArray(t,cJSON_CreateString(section));cJSON_AddItemToArray(m,cJSON_CreateString(enabled&&x==0?(motion?"0x00000002":"0x00000001"):"0x00000000"));}cJSON_AddItemToArray(times,t);cJSON_AddItemToArray(masks,m);}
 cJSON_ReplaceItemInObjectCaseSensitive(target,"TimeSection",times);cJSON_ReplaceItemInObjectCaseSensitive(target,"Mask",masks);cJSON_ReplaceItemInObjectCaseSensitive(target,"RecordMode",cJSON_CreateString("ConfigRecord"));
 cJSON *expected=cJSON_Duplicate(copy,1),*r=command(c,1040,"Record",copy);if(!good(r)){cJSON_Delete(expected);cJSON_Delete(old);return r?r:error("Falha ao salvar agenda");}cJSON_Delete(r);cJSON *check=command(c,1042,"Record",NULL);if(!good(check)||!cJSON_Compare(expected,item(check,"Record"),1)){cJSON_Delete(check);r=command(c,1040,"Record",cJSON_Duplicate(a,1));cJSON_Delete(r);check=error("Agenda divergente; restauracao solicitada");}cJSON_Delete(expected);cJSON_Delete(old);return check;
}
static cJSON *dispatch(Camera *c,const char *path,cJSON *req){
 if(!strcmp(path,"/api/health")){cJSON *r=cJSON_CreateObject();const char *paths[]={"/proc/meminfo","/proc/self/status","/proc/mounts","/var/localmod-v3/ssh-launch-state.txt","/var/localmod-v3/ssh-debug.log","/var/localmod-v3/ssh.log",NULL};for(int i=0;paths[i];i++){char text[4097];FILE *f=fopen(paths[i],"r");if(!f)continue;size_t n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);cJSON_AddStringToObject(r,paths[i],text);}const char *dirs[]={"/","/var","/var/localmod-v3","/var/localmod-v3/auth",NULL};cJSON *perms=cJSON_AddObjectToObject(r,"permissoes");for(int i=0;dirs[i];i++){struct stat st;struct statvfs fs;if(stat(dirs[i],&st))continue;cJSON *v=cJSON_AddObjectToObject(perms,dirs[i]);cJSON_AddNumberToObject(v,"uid",st.st_uid);cJSON_AddNumberToObject(v,"gid",st.st_gid);cJSON_AddNumberToObject(v,"mode",st.st_mode&07777);if(!statvfs(dirs[i],&fs))cJSON_AddBoolToObject(v,"readonly",!!(fs.f_flag&ST_RDONLY));}cJSON_AddBoolToObject(r,"controls_enabled",controls);return r;}
 if(!strcmp(path,"/api/read")){const char *n=str(item(req,"name"));if(!allowed_read(n))return error("Consulta nao permitida");int code=!strcmp(n,"SystemInfo")||!strcmp(n,"StorageInfo")||!strcmp(n,"WorkState")?1020:1042;return command(c,code,n,NULL);}
 if(!strcmp(path,"/api/files")){const char *b=str(item(req,"begin")),*e=str(item(req,"end"));if(!date_ok(b)||!date_ok(e)||strcmp(b,e)>=0)return error("Periodo invalido");cJSON *v=cJSON_CreateObject();cJSON_AddStringToObject(v,"BeginTime",b);cJSON_AddStringToObject(v,"EndTime",e);cJSON_AddNumberToObject(v,"Channel",0);cJSON_AddStringToObject(v,"DriverTypeMask","0x0000FFFF");cJSON_AddStringToObject(v,"Event","*");cJSON_AddStringToObject(v,"StreamType","0x00000000");cJSON_AddStringToObject(v,"Type","h264");return command(c,1440,"OPFileQuery",v);}
 if(!controls)return error("Controles desativados nesta rodada de teste");
 if(!strcmp(path,"/api/light")){const char *action=str(item(req,"action"));if(strcmp(action,"KeepOpen")&&strcmp(action,"Close")&&strcmp(action,"Auto")&&strcmp(action,"Intelligent"))return error("Modo de luz invalido");cJSON *v=cJSON_CreateObject();cJSON_AddStringToObject(v,"Action",action);return command(c,1450,"OPWhiteLightControl",v);}
 if(!strcmp(path,"/api/patch"))return patch(c,req);
 if(!strcmp(path,"/api/schedule"))return schedule(c,req);
 if(!strcmp(path,"/api/ptz")){const char *d=str(item(req,"direction"));if(strcmp(d,"DirectionUp")&&strcmp(d,"DirectionDown")&&strcmp(d,"DirectionLeft")&&strcmp(d,"DirectionRight"))return error("Direcao invalida");cJSON *r=ptz(c,d,65535);struct timespec t={0,250000000};nanosleep(&t,NULL);cJSON *stop=ptz(c,d,-1);if(!good(stop)){cJSON_Delete(r);r=error("Parada nao confirmada; verificar camera");}cJSON_Delete(stop);return r;}
 if(!strcmp(path,"/api/preset")){const char *op=str(item(req,"action"));cJSON *p=item(req,"number");if(!cJSON_IsNumber(p)||p->valuedouble!=p->valueint||p->valueint<1||p->valueint>16||(strcmp(op,"SetPreset")&&strcmp(op,"GotoPreset")))return error("Posicao invalida");return ptz(c,op,p->valueint);}
 return error("Rota nao encontrada");
}
static void reply(int fd,int status,const char *type,const char *data){char h[1024];const char *msg=status==200?"OK":status==401?"Unauthorized":"Bad Request";int n=snprintf(h,sizeof h,"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'; base-uri 'none'\r\n%s\r\n",status,msg,type,strlen(data),status==401?"WWW-Authenticate: Basic realm=\"Camera local\"\r\n":"");exact(fd,h,n,1);exact(fd,(void*)data,strlen(data),1);}
static char *header(char *headers,const char *name){size_t n=strlen(name);for(char *p=strstr(headers,"\r\n");p;p=strstr(p+2,"\r\n")){p+=2;if(!strncasecmp(p,name,n)&&p[n]==':'){p+=n+1;while(*p==' ')p++;return p;}p-=2;}return NULL;}
static void serve(int fd){
 char request[12288]={0};size_t used=0;char *body=NULL;while(used<sizeof(request)-1){ssize_t n=recv(fd,request+used,sizeof(request)-1-used,0);if(n<=0)return;used+=n;request[used]=0;body=strstr(request,"\r\n\r\n");if(body)break;}if(!body){reply(fd,400,"text/plain","Cabecalho invalido");return;}body+=4;
 *(body-2)=0;
 char method[8],path[96],version[16];if(sscanf(request,"%7s %95s %15s",method,path,version)!=3){reply(fd,400,"text/plain","Requisicao invalida");return;}
 if(!strcmp(method,"GET")&&!strcmp(path,"/")){reply(fd,200,"text/html; charset=utf-8",panel_html);return;}
 char *len=header(request,"Content-Length"),*auth=header(request,"Authorization"),*ctype=header(request,"Content-Type"),*guard=header(request,"X-Localmod"),*transfer=header(request,"Transfer-Encoding");
 if(strcmp(method,"POST")||!len||transfer||!ctype||strncmp(ctype,"application/json\r\n",18)||!guard||strncmp(guard,"1\r\n",3)){reply(fd,400,"text/plain","POST JSON local obrigatorio");return;}
 char *end;long size=strtol(len,&end,10);if(size<2||size>8192||(size_t)(body-request+size)>=sizeof request||strncmp(end,"\r\n",2)){reply(fd,400,"text/plain","Tamanho invalido");return;}
 size_t have=used-(body-request);if(have>(size_t)size){reply(fd,400,"text/plain","Corpo ambiguo");return;}if(exact(fd,body+have,size-have,0))return;body[size]=0;
 if(!auth||strncmp(auth,"Basic ",6)){reply(fd,401,"text/plain","Autenticacao obrigatoria");return;}auth+=6;char *ae=strstr(auth,"\r\n");if(!ae||ae-auth>512){reply(fd,401,"text/plain","Credencial invalida");return;}
 unsigned char decoded[384];unsigned long dn=sizeof(decoded)-1;if(base64_decode((unsigned char*)auth,ae-auth,decoded,&dn)!=CRYPT_OK){reply(fd,401,"text/plain","Credencial invalida");return;}decoded[dn]=0;char *colon=strchr((char*)decoded,':');if(!colon||colon==(char*)decoded||colon-(char*)decoded>64||memchr(decoded,0,dn)){wipe(decoded,sizeof decoded);reply(fd,401,"text/plain","Credencial invalida");return;}*colon=0;
 Camera c={.fd=-1};int ok=login(&c,(char*)decoded,colon+1);wipe(decoded,sizeof decoded);wipe(auth,ae-auth);if(ok){logout(&c);reply(fd,401,"text/plain","Camera indisponivel ou credencial recusada");return;}
 cJSON *req=cJSON_ParseWithLengthOpts(body,size+1,NULL,1),*r=NULL;if(memchr(body,0,size)||!cJSON_IsObject(req))r=error("JSON invalido");else r=dispatch(&c,path,req);cJSON_Delete(req);logout(&c);if(!r)r=error("Sem resposta nativa");redact(r);char *out=cJSON_PrintUnformatted(r);reply(fd,200,"application/json",out?out:"{}");free(out);cJSON_Delete(r);wipe(request,sizeof request);
}
int main(int argc,char **argv){int port=18080;struct in_addr bindip={.s_addr=htonl(INADDR_LOOPBACK)};for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--enable-controls"))controls=1;else if(!strcmp(argv[i],"--port")&&i+1<argc)port=atoi(argv[++i]);else if(!strcmp(argv[i],"--backend-port")&&i+1<argc)backend_port=atoi(argv[++i]);else if(!strcmp(argv[i],"--bind")&&i+1<argc){if(inet_pton(AF_INET,argv[++i],&bindip)!=1)return 2;uint32_t ip=ntohl(bindip.s_addr);if(ip!=INADDR_LOOPBACK&&(ip&0xffff0000)!=0xc0a80000)return 2;}else return 2;}if(port<1024||port>65535||backend_port<1024||backend_port>65535)return 2;signal(SIGPIPE,SIG_IGN);int fd=socket(AF_INET,SOCK_STREAM,0),one=1;if(fd<0)return 3;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(port),.sin_addr=bindip};if(bind(fd,(void*)&a,sizeof a)||listen(fd,4))return 4;for(;;){int client=accept(fd,NULL,NULL);if(client<0)continue;struct timeval t={3,0};setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof t);setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof t);serve(client);close(client);}return 0;}
