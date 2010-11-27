#!/bin/sh

if [ -z "$1" ];
then
echo "Usage: "$0" 192.168.206.1/24"
exit
fi


# Number of seconds snort should run for
seconds=6

# Removing previous snort files
sudo rm -f log/alert

# Adding the subnetwork value in myrules.conf
subnet=$1
echo "var HOME_NET "$subnet > subnet.txt 
cat subnet.txt myrules.conf > myrules_temp.conf

# Starting snort and then sleeping for a few seconds
sudo snort -d -l ./log -c myrules_temp.conf  -A fast -D
sleep $seconds 

# Killing snort - there should be a better way of doing this
sudo killall snort

# Outputting the snort results
if [ `cat log/alert | wc -l` -eq 0  ];
then
echo "** Boring network, no attacks detected."
else
cat log/alert
fi

# Cleaning up and Exiting
rm -f subnet.txt
rm -f myrules_temp.conf
exit
