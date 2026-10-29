#!/bin/sh
# Prototipo reversivel para limitar egress pelo roteamento, sem tocar nos drivers.
# Nao e firewall: DHCP/RA podem recolocar a rota entre duas verificacoes.
PATH=/sbin:/usr/sbin:/bin:/usr/bin
export PATH
umask 077
ROOT=${LOCALROUTE_STATE:-/var/localmod-routes}
CONFIG=${LOCALROUTE_CONFIG:-/mnt/mtd/Config}
PROC=${LOCALROUTE_PROC:-/proc}
PIDPROC=${LOCALROUTE_PID_PROC:-$PROC}
ROUTE=${LOCALROUTE_ROUTE:-route}
SELF=${LOCALROUTE_SELF:-/mnt/custom/localmod/local-routes.sh}
INTERVAL=${LOCALROUTE_INTERVAL:-10}
JOURNAL="$ROOT/removed.tsv"
PIDFILE="$ROOT/watcher.pid"
LOCK="$ROOT/lock"
ZERO6=00000000000000000000000000000000

mkdir -p "$ROOT" || exit 1
touch "$JOURNAL" || exit 1
status() { printf '%s\n' "$1" > "$ROOT/status"; printf '%s\n' "$1"; }
log() { printf '%s\n' "$1" >> "$ROOT/events.log"; }
enabled() {
    [ -f "$CONFIG/localmod.routes.enabled" ] && [ ! -f "$CONFIG/localmod.disabled" ]
}

inventory() {
    # Parser independente de formatacao localizada do comando route.
    awk '
    function hex(s, i,n,c) {n=0; s=tolower(s); for(i=1;i<=length(s);i++){c=index("0123456789abcdef",substr(s,i,1)); if(!c)return -1; n=n*16+c-1} return n}
    function ip4(s) {return hex(substr(s,7,2)) "." hex(substr(s,5,2)) "." hex(substr(s,3,2)) "." hex(substr(s,1,2))}
    NR>1 && $2=="00000000" && $8=="00000000" && hex($4)%2==1 && $1!="lo" {
      if($1 !~ /^[A-Za-z0-9_.:-]+$/ || length($1)>15 || length($3)!=8 || hex($3)<0 || $7 !~ /^[0-9]+$/){bad=1;next}
      print "4\t" ip4($3) "\t" $1 "\t" $7
    }
    END {if(bad)exit 1}' "$PROC/net/route" || return 1
    if [ -f "$PROC/net/ipv6_route" ]; then
        awk '
        function hex(s, i,n,c) {n=0; s=tolower(s); for(i=1;i<=length(s);i++){c=index("0123456789abcdef",substr(s,i,1));if(!c)return -1;n=n*16+c-1}return n}
        function ip6(s, i,r){r="";for(i=1;i<=32;i+=4)r=r (i>1?":":"") substr(s,i,4);return r}
        $1=="00000000000000000000000000000000" && $2=="00" && hex($9)%2==1 && $10!="lo" {
          if($3!="00000000000000000000000000000000" || $4!="00" || $10 !~ /^[A-Za-z0-9_.:-]+$/ || length($10)>15 || length($5)!=32 || hex($5)<0 || hex($6)<0){bad=1;next}
          if($5=="00000000000000000000000000000000") gw="::";else gw=ip6($5)
          print "6\t" gw "\t" $10 "\t" hex($6)
        }
        END {if(bad)exit 1}' "$PROC/net/ipv6_route" || return 1
    fi
}

preflight() {
    [ -f "$PROC/version" ] && [ -r "$PROC/net/route" ] || return 10
    command -v "$ROUTE" >/dev/null 2>&1 || return 11
    "$ROUTE" -n >/dev/null 2>&1 || return 12
    if [ -d "$PROC/sys/net/ipv6" ] || [ -e "$PROC/net/if_inet6" ]; then
        [ -r "$PROC/net/ipv6_route" ] || return 13
        "$ROUTE" -A inet6 -n >/dev/null 2>&1 || return 14
    fi
    # Exige uma rota IPv4 de rede conectada para preservar o acesso LAN.
    awk 'NR>1 && $2!="00000000" && $3=="00000000" && $8!="00000000" {found=1} END{exit !found}' "$PROC/net/route" || return 15
    inventory > "$ROOT/inventory.tsv" || return 16
    return 0
}

apply_record() {
    action=$1
    family=$2
    gateway=$3
    device=$4
    metric=$5
    case $device in ''|*[!A-Za-z0-9_.:-]*) return 1;; esac
    case $metric in ''|*[!0-9]*) return 1;; esac
    case $family in
        4)
            case $gateway in ''|*[!0-9.]*) return 1;; esac
            if [ "$gateway" = 0.0.0.0 ]; then
                "$ROUTE" "$action" default metric "$metric" dev "$device" >/dev/null 2>&1
            else
                "$ROUTE" "$action" default gw "$gateway" metric "$metric" dev "$device" >/dev/null 2>&1
            fi
            ;;
        6)
            case $gateway in ''|*[!0-9a-fA-F:]*) return 1;; esac
            if [ "$gateway" = :: ]; then
                "$ROUTE" -A inet6 "$action" ::/0 metric "$metric" dev "$device" >/dev/null 2>&1
            else
                "$ROUTE" -A inet6 "$action" ::/0 gw "$gateway" metric "$metric" dev "$device" >/dev/null 2>&1
            fi
            ;;
        *) return 1;;
    esac
}

contains_record() {
    grep -F -x -q "$1" "$2"
}

remove_defaults() {
    preflight || { status route_unprotected_preflight_failed; return 1; }
    while IFS="$(printf '\t')" read -r family gateway device metric; do
        [ -n "$family" ] || continue
        record=$(printf '%s\t%s\t%s\t%s' "$family" "$gateway" "$device" "$metric")
        if ! contains_record "$record" "$JOURNAL"; then
            count=$(awk 'END{print NR+0}' "$JOURNAL")
            [ "$count" -lt 32 ] || { status route_unprotected_journal_full; return 1; }
            # Registrar ANTES de apagar cobre encerramento entre comando e gravacao.
            printf '%s\n' "$record" >> "$JOURNAL" || return 1
        fi
        apply_record del "$family" "$gateway" "$device" "$metric" || {
            status route_unprotected_delete_failed
            return 1
        }
    done < "$ROOT/inventory.tsv"
    inventory > "$ROOT/current.tsv" || return 1
    [ ! -s "$ROOT/current.tsv" ] || { status route_unprotected_defaults_remaining; return 1; }
    status route_local_only
}

restore_defaults() {
    inventory > "$ROOT/current.tsv" || { status route_restore_inventory_failed; return 1; }
    : > "$ROOT/remaining.tsv"
    result=0
    while IFS="$(printf '\t')" read -r family gateway device metric; do
        [ -n "$family" ] || continue
        record=$(printf '%s\t%s\t%s\t%s' "$family" "$gateway" "$device" "$metric")
        # Nao duplica uma rota que DHCP/RA ja tenha recolocado com os mesmos valores.
        if ! contains_record "$record" "$ROOT/current.tsv"; then
            if ! apply_record add "$family" "$gateway" "$device" "$metric"; then
                printf '%s\n' "$record" >> "$ROOT/remaining.tsv"
                result=1
            fi
        fi
    done < "$JOURNAL"
    mv "$ROOT/remaining.tsv" "$JOURNAL" || return 1
    [ "$result" -eq 0 ] && status route_stopped || status route_restore_failed
    return "$result"
}

process_tick() {
    [ -r "$PIDPROC/$1/stat" ] || return 1
    sed 's/^[^(]*(.*) //' "$PIDPROC/$1/stat" | awk '{print $20}'
}

cleanup_watcher() {
    [ -f "$PIDFILE" ] || return 0
    read -r old_pid old_tick < "$PIDFILE"
    if [ "$old_pid" = "$$" ] && [ "$old_tick" = "$tick" ]; then
        rm -f "$PIDFILE"
    fi
}

stop_watcher() {
    [ -f "$PIDFILE" ] || return 0
    read -r pid tick < "$PIDFILE"
    case $pid:$tick in *[!0-9:]*|:*|*:) return 1;; esac
    current=$(process_tick "$pid") || { rm -f "$PIDFILE"; return 0; }
    if [ "$current" != "$tick" ]; then
        log 'PID antigo foi reutilizado; nenhum processo foi sinalizado.'
        rm -f "$PIDFILE"
        return 0
    fi
    kill "$pid" 2>/dev/null || return 1
    rm -f "$PIDFILE"
}

lock() { mkdir "$LOCK" 2>/dev/null; }
unlock() { rmdir "$LOCK" 2>/dev/null; }

case ${1:-status} in
    preflight)
        preflight; rc=$?
        [ "$rc" -eq 0 ] && status route_backend_available_unverified || status "route_unprotected_preflight_$rc"
        exit "$rc"
        ;;
    status)
        inventory > "$ROOT/status-inventory.tsv" || { status route_unprotected_inventory_failed; exit 1; }
        if [ -s "$JOURNAL" ] && [ ! -s "$ROOT/status-inventory.tsv" ]; then
            status route_local_only
        elif [ -s "$ROOT/status-inventory.tsv" ]; then
            status route_unprotected_defaults_present
            exit 1
        else
            status route_not_active
            exit 1
        fi
        ;;
    start)
        enabled || { status route_disabled; exit 0; }
        lock || { status route_operation_busy; exit 1; }
        trap 'unlock' 0
        trap 'exit 1' 1 2 15
        stop_watcher || { status route_stop_watcher_failed; exit 1; }
        if ! remove_defaults; then
            # Recuperar qualquer rota removida antes de falhar nesta tentativa.
            restore_defaults
            exit 1
        fi
        log 'Rotas padrao removidas; isto nao equivale a um firewall.'
        ;;
    watch)
        # Chamado separadamente: nao criar daemons implicitamente com start.
        enabled || { status route_disabled; exit 0; }
        case $INTERVAL in ''|*[!0-9]*) exit 2;; esac
        [ "$INTERVAL" -ge 5 ] && [ "$INTERVAL" -le 300 ] || exit 2
        lock || exit 1
        if [ -f "$PIDFILE" ]; then unlock; status route_watcher_already_present; exit 1; fi
        tick=$(process_tick $$) || { unlock; exit 1; }
        printf '%s %s\n' $$ "$tick" > "$PIDFILE"
        unlock
        trap 'cleanup_watcher; exit 0' 1 2 15
        while enabled; do
            if lock; then
                preflight
                ready=$?
                if [ "$ready" -eq 15 ]; then
                    # DHCP pode demorar no boot ou apos uma reconexao Wi-Fi.
                    # Permanecer ativo evita abandonar a politica na reconexao.
                    status route_waiting_for_lan
                    unlock
                    sleep "$INTERVAL"
                    continue
                fi
                if ! remove_defaults; then
                    restore_defaults
                    status route_unprotected_apply_failed
                    unlock
                    log 'Watcher interrompido apos falha de rota; verificar status e rollback.'
                    cleanup_watcher
                    exit 1
                fi
                unlock
            fi
            sleep "$INTERVAL"
        done
        cleanup_watcher
        log 'Watcher encerrado pela marca disabled; executar stop para restaurar rotas.'
        ;;
    stop)
        lock || { status route_operation_busy; exit 1; }
        trap 'unlock' 0
        trap 'exit 1' 1 2 15
        stop_watcher || { status route_stop_watcher_failed; exit 1; }
        restore_defaults
        ;;
    *) printf '%s\n' 'Uso: local-routes.sh preflight|start|watch|status|stop'; exit 2;;
esac
