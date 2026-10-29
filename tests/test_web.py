"""Exercita o binario ARM real contra backend DVRIP simulado; nunca acessa camera."""
import base64
import copy
import hashlib
import http.client
import json
import os
from pathlib import Path
import socket
import socketserver
import struct
import subprocess
import threading
import time

ROOT=Path(__file__).resolve().parents[1]
BINARY=ROOT/os.environ.get('LOCALMOD_TEST_BINARY','build/localmod-web.arm')
HEADER=struct.Struct('<BB2xII2BHI')
STATE={'Record':[{'RecordMode':'ConfigRecord','TimeSection':[], 'Mask':[], 'PacketLength':5}],
       'Camera.Param':[{'DayNightColor':'0x00000003','PictureFlip':'0x00000001'}],
       'Camera.ParamEx':[{'OneKeyOpenWL':False,'Other':5}]}
CALLS=[]
def exact(sock,n):
    out=b''
    while len(out)<n:
        part=sock.recv(n-len(out))
        if not part: raise EOFError
        out+=part
    return out
def folded(p):
    d=hashlib.md5(p.encode()).digest();a='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'
    return ''.join(a[(d[i]+d[i+1])%62] for i in range(0,16,2))
class Backend(socketserver.BaseRequestHandler):
    version=1
    def handle(self):
        try:
            while True:
                h=HEADER.unpack(exact(self.request,20));j=json.loads(exact(self.request,h[-1]).rstrip(b'\n\0'));code=h[-2];CALLS.append((code,copy.deepcopy(j)))
                if code==1000:
                    result={'Ret':100 if j['UserName']=='tester' and j['PassWord']==folded('test-only-password') else 203,'SessionID':'0x00000020'}
                elif code==1040:
                    STATE[j['Name']]=j[j['Name']];result={'Ret':100}
                elif code==1042: result={'Ret':100,j['Name']:STATE.get(j['Name'],[]),'SessionID':'0x00000020'}
                elif code==1020: result={'Ret':100,j['Name']:{'Status':'test','SerialNo':'secret-serial'},'SessionID':'0x00000020'}
                elif code==1440: result={'Ret':100,'OPFileQuery':[{'FileName':'/test/example.h264'}]}
                else: result={'Ret':100}
                raw=json.dumps(result).encode()+b'\n\0'
                # Duas partes no mesmo JSON verificam remontagem sem misturar pacotes.
                parts=[raw[:len(raw)//2],raw[len(raw)//2:]] if code==1020 else [raw]
                for i,part in enumerate(parts):self.request.sendall(HEADER.pack(255,self.version,32,0,len(parts),i,code+1,len(part))+part)
        except (EOFError,OSError):pass

def main():
    backend=socketserver.TCPServer(('127.0.0.1',0),Backend);threading.Thread(target=backend.serve_forever,daemon=True).start()
    probe=socket.socket();probe.bind(('127.0.0.1',0));port=probe.getsockname()[1];probe.close()
    child=subprocess.Popen(['qemu-arm',str(BINARY),'--port',str(port),'--backend-port',str(backend.server_address[1]),'--enable-controls'],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    tests=[]
    def api(route,payload,auth=True,guard=True):
        c=http.client.HTTPConnection('127.0.0.1',port,timeout=10);h={'Content-Type':'application/json'}
        if auth:h['Authorization']='Basic '+base64.b64encode(b'tester:test-only-password').decode()
        if guard:h['X-Localmod']='1'
        c.request('POST','/api/'+route,json.dumps(payload),h);r=c.getresponse();body=r.read();status=r.status;c.close();return status,body
    def check(name,condition):
        assert condition,name
        tests.append(name)
    try:
        for _ in range(60):
            try:
                with socket.create_connection(('127.0.0.1',port),.1):break
            except OSError:time.sleep(.05)
        c=http.client.HTTPConnection('127.0.0.1',port);c.request('GET','/');r=c.getresponse();check('panel_served',r.status==200 and b'v3' in r.read());c.close()
        check('reject_unauthenticated',api('read',{'name':'SystemInfo'},False)[0]==401)
        check('reject_cross_origin_simple_request',api('read',{'name':'SystemInfo'},guard=False)[0]==400)
        status,raw=api('read',{'name':'SystemInfo'});check('fragmented_reply_reassembled',status==200 and json.loads(raw)['Ret']==100)
        check('redacted_serial_and_session',b'secret-serial' not in raw and b'0x00000020' not in raw)
        Backend.version=0
        check('legacy_protocol_version_zero',api('read',{'name':'SystemInfo'})[0]==200)
        Backend.version=2
        check('unsupported_protocol_version_rejected',api('read',{'name':'SystemInfo'})[0]==401)
        Backend.version=1
        _,raw=api('health',{});check('authenticated_extension_diagnostics',json.loads(raw)['controls_enabled'] is True)
        _,raw=api('light',{'action':'KeepOpen'});check('native_white_light_command',json.loads(raw).get('Ret')==100)
        _,raw=api('light',{'action':'Invalid'});check('invalid_light_mode_blocked','erro' in json.loads(raw))
        n=len(CALLS);_,raw=api('read',{'name':'Account1'});check('account_config_blocked',b'permitida' in raw and not any(c==1042 for c,j in CALLS[n:]))
        _,raw=api('patch',{'name':'NetWork.Nat','field':'Enable','value':True});check('network_writes_blocked',b'permitida' in raw)
        _,raw=api('patch',{'name':'Camera.ParamEx','field':'OneKeyOpenWL','value':True});check('white_light_verified',json.loads(raw)['Camera.ParamEx'][0]['OneKeyOpenWL'] is True and STATE['Camera.ParamEx'][0]['Other']==5)
        _,raw=api('patch',{'name':'Record','field':'RecordMode','value':'ClosedRecord'});check('record_mode_verified',json.loads(raw)['Record'][0]['RecordMode']=='ClosedRecord')
        _,raw=api('schedule',{'start':'07:30','end':'22:00','motion':True,'days':[True]*7});check('schedule_verified',json.loads(raw)['Record'][0]['TimeSection'][0][0]=='1 07:30:00-22:00:00' and STATE['Record'][0]['Mask'][0][0]=='0x00000002')
        _,raw=api('schedule',{'start':'25:00','end':'26:00','days':[True]*7});check('invalid_schedule_blocked','erro' in json.loads(raw))
        n=len(CALLS);api('ptz',{'direction':'DirectionLeft'});ptz=[j['OPPTZControl']['Parameter']['Preset'] for c,j in CALLS[n:] if c==1400];check('ptz_start_followed_by_stop',ptz==[65535,-1])
        _,raw=api('preset',{'action':'SetPreset','number':999});check('invalid_preset_blocked','erro' in json.loads(raw))
        _,raw=api('files',{'begin':'2026-10-29 00:00:00','end':'2026-10-29 23:59:59'});check('native_archive_query',json.loads(raw)['OPFileQuery'][0]['FileName'].startswith('/test/'))
        _,raw=api('files',{'begin':'bad','end':'bad'});check('invalid_archive_range_blocked','erro' in json.loads(raw))
        s=socket.create_connection(('127.0.0.1',port));s.sendall(b'POST /api/read HTTP/1.1\r\nContent-Type: application/json\r\nX-Localmod: 1\r\nContent-Length: 999999999\r\n\r\n');check('oversized_request_rejected',b'400' in s.recv(100));s.close()
        check('server_survives_invalid_request',child.poll() is None)
        report={'passed':len(tests),'tests':tests,'hardware_tested':False,'binary':BINARY.name}
        (ROOT/'build/web-test-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
    finally:
        child.terminate();child.wait(timeout=5);backend.shutdown();backend.server_close()
if __name__=='__main__':main()
