import glob
import csv
log_dir='/mnt/log-new/k7n12/'
output_fn='k7_n12_stats.csv'
static_workloads=['datamining','enterprise']
dynamic_workloads=['uniform','pareto']
small_flow = 100000 # 100kB
large_flow = 10000000 # 10 MB

def parse_log(fn, is_static):
    print(fn)
    rows = list()

    parts = fn.split('/')[-1]
    parts = parts.split('.log')[0]
    parts = parts.split('_')

    exp = 'ec_offload' if 'offload' in fn else 'ec'
    load     = parts[-6]
    workload = parts[-5]
    ecchance = parts[-4]
    flowsize = parts[-3]
    # k        = parts[-2]
    # n        = parts[-1]
    with open(fn, 'r') as f:
        lines = f.read().splitlines()
        if len(lines) > 1000:
            lines = lines[0:1000]
        # lines = [next(f) for _ in range(min(1000, len()))]
        lines = [l.split(' ') for l in lines]

        
        sizes = [int(x[1]) for x in lines]
        actual_avg_size = 0
        if len(sizes) > 0:
            actual_avg_size = sum(sizes)/len(sizes)

        diffs = [float(l[0]) for l in lines]
        avg_lat = 0
        if len(diffs) > 0:
            avg_lat = sum(diffs)/len(diffs)

        valid_tputs_str = filter(lambda x: x[2] != "inf", lines)
        valid_tputs = [float(l[2]) for l in valid_tputs_str]
        avg_tputs = 0
        if len(valid_tputs) > 0:
            avg_tputs = sum(valid_tputs)/len(valid_tputs)
        print(str(len(valid_tputs))+' '+str(len(lines)))

        row_ec = [str(exp+' '+workload+' '+ecchance+' '+load+' '+flowsize+' ec'), exp, load, workload, flowsize, ecchance, actual_avg_size, len(lines), avg_lat, avg_tputs, len(valid_tputs)]
        print(row_ec)
        rows.append(row_ec)
    with open(fn+"_bg", 'r') as f:
        lines = f.read().splitlines()
        if len(lines) > 1000:
            lines = lines[0:1000]
        # lines = f.read().splitlines()
        # lines = lines[0:1000]
        # lines = [next(f) for _ in range(1000)]
        lines = [l.split(' ') for l in lines]

        
        sizes = [int(x[1]) for x in lines]
        actual_avg_size = 0
        if len(sizes) > 0:
            actual_avg_size = sum(sizes)/len(sizes)

        diffs = [float(l[0]) for l in lines]
        avg_lat = 0
        if len(diffs) > 0:
            avg_lat = sum(diffs)/len(diffs)

        valid_tputs_str = filter(lambda x: x[2] != "inf", lines)
        valid_tputs = [float(l[2]) for l in valid_tputs_str]
        avg_tputs = 0
        if len(valid_tputs) > 0:
            avg_tputs = sum(valid_tputs)/len(valid_tputs)
        print(str(len(valid_tputs))+' '+str(len(lines)))

        row_bg = [str(exp+' '+workload+' '+ecchance+' '+load+' '+flowsize+' bg'), exp, load, workload, flowsize, ecchance, actual_avg_size, len(lines), avg_lat, avg_tputs, len(valid_tputs)]
        print(row_bg)
        rows.append(row_bg)


    return rows


if __name__ == '__main__':

    i = 0
    for fn_pattern in dynamic_workloads:
        #break
        avg_lats_ec = list()
        avg_lats_bg = list()
        log_files=glob.glob(log_dir+'*'+fn_pattern+'*.durations')
        for fn in log_files:
            row = parse_log(fn, False)
            avg_lats_ec.append(row[0])
            avg_lats_bg.append(row[1])
            i+=1
            if i % 10 == 0:
                print(i)
            #break
        with open(log_dir+fn_pattern+'_ec_'+output_fn, 'w') as f:
            writer = csv.writer(f)
            for r in sorted(avg_lats_ec):
                writer.writerow(r)
        with open(log_dir+fn_pattern+'_bg_'+output_fn, 'w') as f:
            writer = csv.writer(f)
            for r in sorted(avg_lats_bg):
                writer.writerow(r)

    for fn_pattern in static_workloads:
        avg_lats_ec = list()
        avg_lats_bg = list()
        log_files=glob.glob(log_dir+'*'+fn_pattern+'*.durations')
        for fn in log_files:
            row = parse_log(fn, True)
            avg_lats_ec.append(row[0])
            avg_lats_bg.append(row[1])
            i+=1
            if i % 10 == 0:
                print(i)
            #break
        with open(log_dir+fn_pattern+'_ec_'+output_fn, 'w') as f:
            writer = csv.writer(f)
            for r in sorted(avg_lats_ec):
                writer.writerow(r)
        with open(log_dir+fn_pattern+'_bg_'+output_fn, 'w') as f:
            writer = csv.writer(f)
            for r in sorted(avg_lats_bg):
                writer.writerow(r)


        
    