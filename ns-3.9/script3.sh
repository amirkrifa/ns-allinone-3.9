#!/bin/bash
# Used with script 4 to calculate the average delivery rate function of channel size 
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
	chsize=`cat test.txt|grep "Content$i-"|head -1|cut -d' ' -f3`
	avg=`expr $sum / $number`
	echo "$number $avg $chsize">>res.txt		
	let i=i+1
done 
