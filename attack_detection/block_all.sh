SOURCE=$1

sudo iptables -A PREROUTING -s $SOURCE -j DROP
