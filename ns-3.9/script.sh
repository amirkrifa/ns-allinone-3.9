#!/bin/bash

# Used with script 2 to calculate average delivery rate function of the number of times
# that a channel is requested

rm -rf res.txt
touch res.txt
i=1
while [ $i -le 49 ]; do
	sum=0
	number=0	
	for x in `cat test.txt|grep "Content$i-"|cut -d' ' -f2`
	do
		sum=`expr $x + $sum`
		number=`expr $number + 1`
	done
	avg=`expr $sum / $number`
	echo "$number $avg">>res.txt		
	let i=i+1
done 
