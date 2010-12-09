#!/bin/bash

if [ $# != 3 ];
then
echo "Usage: "$0" interval_size(s) measurements output_file"
exit
fi

INTERVAL=$1
MEASUREMENTS=$2
OUTPUT=$3

for i in `seq 1 $MEASUREMENTS`;
do
    cat /proc/acpi/battery/BAT0/state | grep "present rate" | awk '{print $3}' >> $OUTPUT
    sleep $INTERVAL
done
