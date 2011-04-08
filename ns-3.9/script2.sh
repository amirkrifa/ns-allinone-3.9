#!/bin/bash
rm -rf res2.txt
touch res2.txt
i=1
while [ $i -le 20 ]; do
	sum=0
	number=0	
	for x in `cat res.txt|grep "^$i "|cut -d' ' -f2`
	do
		sum=`expr $x + $sum`
		number=`expr $number + 1`
	done
	if [ $number -ne 0 ]
	then
		avg=`expr $sum / $number`
		mod=`expr $sum % $number`	
		echo "$i $avg  =$mod/$number">>res2.txt	
	fi	
	let i=i+1
done 
