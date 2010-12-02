SOURCE=$1

#sudo iptables -A PREROUTING -s $SOURCE -j DROP
echo "$SOURCE has been blocked"

exit
