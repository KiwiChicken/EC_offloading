#!/bin/bash
outfn="data.csv"
log_dir="/mydata/log/"

if [ ! -f $outfn ]; then
    echo "Data file not found, creatinmg and write header"
    touch $outfn
    echo "exp,workload,load,flow_size,avg_lat"
fi

#rm $log_dir*.durations

for log in $log_dir*.log
do
    echo $log
    grep ^Flow $log | awk '{print $11" "$5}' > $log.durations
    #just sanity check here
    cnt=$(wc -l $log.durations)
    start_sum=$(awk '{s+=$1} END {print s}' $log.durations)
    end_sum=$(awk '{s+=$2} END {print s}' $log.durations)
    echo "$cnt $start_sum $end_sum"
done
