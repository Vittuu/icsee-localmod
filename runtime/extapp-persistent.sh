#!/bin/sh
# Candidato persistente: iniciar apenas recursos adicionados, sem mudar drivers.
PATH=/sbin:/usr/sbin:/bin:/usr/bin
export PATH
umask 077
ulimit -c 0
[ -f /mnt/mtd/Config/localmod.disabled ] && exit 0
RAM=/var/localmod-v3
BASE=/mnt/custom/localmod
KEYS=/mnt/mtd/Config/localmod
mkdir -p "$RAM/auth" "$RAM/config" || exit 1
chmod 700 "$RAM" "$RAM/auth" "$RAM/config"
export LOCALROUTE_STATE="$RAM/routes"
export LOCALROUTE_CONFIG="$RAM/config"
export LOCALROUTE_SELF="$BASE/local-routes.sh"
export LOCALROUTE_INTERVAL=5
touch "$RAM/config/localmod.routes.enabled"
/bin/sh "$BASE/local-routes.sh" watch > "$RAM/route-guard.log" 2>&1 &
# Detectar o endereco entregue pelo Wi-Fi; nao depender de uma concessao DHCP.
tries=0
while [ "$tries" -lt 120 ]; do
    ip=$(ifconfig eth3 2>/dev/null | awk '/inet addr:/{sub(/.*inet addr:/,"");sub(/ .*/,"");print;exit}')
    case "$ip" in 192.168.*) break;; esac
    tries=$((tries + 1))
    sleep 1
done
[ "$tries" -lt 120 ] || exit 2
printf '%s\n' "$ip" > "$RAM/bind-ip.txt"
"$BASE/localmod-web" --bind "$ip" --enable-controls > "$RAM/web.log" 2>&1 &
echo "$!" > "$RAM/web.pid"
# Nunca gerar chave de host efemera nem aceitar autenticacao por senha.
[ -s "$KEYS/host_key" ] && [ -s "$KEYS/authorized_keys" ] || exit 3
cp "$KEYS/host_key" "$RAM/auth/host_key" || exit 4
cp "$KEYS/authorized_keys" "$RAM/auth/authorized_keys" || exit 4
chmod 600 "$RAM/auth/host_key" "$RAM/auth/authorized_keys"
exec /usr/lib/localmod/dropbearmulti dropbear -F -m -p "$ip:2222" -r "$RAM/auth/host_key" -D "$RAM/auth" -P "$RAM/ssh.pid" -I 600 -T 3 > "$RAM/ssh.log" 2>&1
