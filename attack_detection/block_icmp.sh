
SOURCE=$1

sudo iptables -A PREROUTING -p icmp --icmp-type 8 -s $SOURCE -j DROP

#sudo iptables -A INPUT -p icmp --icmp-type 8 -s $SOURCE -j DROP
