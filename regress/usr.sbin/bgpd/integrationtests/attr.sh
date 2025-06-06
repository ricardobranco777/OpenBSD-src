#!/bin/ksh
#	$OpenBSD: attr.sh,v 1.5 2025/06/05 18:30:10 claudio Exp $

set -e

BGPD=$1
BGPDCONFIGDIR=$2
RDOMAIN1=$3
RDOMAIN2=$4
PAIR1=$5
PAIR2=$6

RDOMAINS="${RDOMAIN1} ${RDOMAIN2}"
PAIRS="${PAIR1} ${PAIR2}"
PAIR1IP=10.12.57.1
PAIR2IP=10.12.57.2
PAIR2IP2=10.12.57.3
PAIR2IP3=10.12.57.4

error_notify() {
	echo cleanup
	pkill -T ${RDOMAIN1} bgpd || true
	pkill -T ${RDOMAIN2} -f exabgp || true
	sleep 1
	ifconfig ${PAIR2} destroy || true
	ifconfig ${PAIR1} destroy || true
	route -qn -T ${RDOMAIN1} flush || true
	route -qn -T ${RDOMAIN2} flush || true
	ifconfig lo${RDOMAIN1} destroy || true
	ifconfig lo${RDOMAIN2} destroy || true
	if [ $1 -ne 0 ]; then
		exit 1
	fi
}

run_exabgp() {
	local _t=$1

	shift
	env	exabgp.log.destination=stdout \
		exabgp.log.packets=true \
		exabgp.log.parser=true \
		exabgp.log.level=DEBUG \
		exabgp.api.cli=false \
		exabgp.daemon.user=build \
	    route -T ${RDOMAIN2} exec exabgp -1 ${1+"$@"} > ./exabgp.$_t.log
}

if [ ! -x /usr/local/bin/exabgp ]; then 
	echo install exabgp from ports for this test >&2
	exit 1
fi

if [ "$(id -u)" -ne 0 ]; then 
	echo need root privileges >&2
	exit 1
fi

trap 'error_notify $?' EXIT

echo check if rdomains are busy
for n in ${RDOMAINS}; do
	if /sbin/ifconfig | grep -v "^lo${n}:" | grep " rdomain ${n} "; then
		echo routing domain ${n} is already used >&2
		exit 1
	fi
done

echo check if interfaces are busy
for n in ${PAIRS}; do
	/sbin/ifconfig "${n}" >/dev/null 2>&1 && \
	    ( echo interface ${n} is already used >&2; exit 1 )
done

set -x

echo setup
ifconfig ${PAIR1} rdomain ${RDOMAIN1} ${PAIR1IP}/29 up
ifconfig ${PAIR2} rdomain ${RDOMAIN2} ${PAIR2IP}/29 up
ifconfig ${PAIR2} alias ${PAIR2IP2}/32
ifconfig ${PAIR2} alias ${PAIR2IP3}/32
ifconfig ${PAIR1} patch ${PAIR2}
ifconfig lo${RDOMAIN1} inet 127.0.0.1/8
ifconfig lo${RDOMAIN2} inet 127.0.0.1/8

echo run bgpd
route -T ${RDOMAIN1} exec ${BGPD} \
	-v -f ${BGPDCONFIGDIR}/bgpd.attr.conf

sleep 1

echo start exabgp
run_exabgp attr exabgp.attr.conf > exabgp.attr.out 2>&1 &

sleep 10

echo test RFC 7606 attribute handling
route -T ${RDOMAIN1} exec bgpctl sh rib in | tee attr.out
diff -u ${BGPDCONFIGDIR}/exabgp.attr.ok attr.out
echo OK

exit 0
