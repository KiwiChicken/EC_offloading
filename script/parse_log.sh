#!/bin/bash
outfn="data.csv"
log_dir="/mnt/log-new/k7n12/"

# if [ ! -f $outfn ]; then
#     echo "Data file not found, creatinmg and write header"
#     touch $outfn
#     echo "exp,workload,load,flow_size,avg_lat"
# fi

rm $log_dir*.durations

for log in $log_dir*.log
do
    echo $log
    head -n10000 $log | grep ^FPGAFlow | awk '{print $11" "$5" "$16}' > $log.durations
    head -n10000 $log | grep ^Flow | awk '{print $11" "$5" "$16}' > $log.durations_bg
    # grep ^FPGAFlow $log | awk '{print $11" "$5" "$16}' > $log.durations
    # grep ^Flow $log | awk '{print $11" "$5" "$16}' > $log.durations_bg
    #just sanity check here
    cnt=$(wc -l $log.durations)
    start_sum=$(awk '{s+=$1} END {print s}' $log.durations)
    end_sum=$(awk '{s+=$2} END {print s}' $log.durations)
    echo "$cnt $start_sum $end_sum"
done
