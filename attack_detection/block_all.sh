SOURCE=$1

#sudo iptables -A PREROUTING -s $SOURCE -j DROP
echo `date +%s`" $SOURCE has been blocked"

exit
