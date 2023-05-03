import glob
import csv
log_dir='/mydata/log/'
output_fn='lats.csv'
static_workloads=['datamining','enterprise']
dynamic_workloads=['uniform','pareto']
small_flow = 100000 # 100kB
large_flow = 10000000 # 10 MB

def parse_log(fn, is_static):
    print(fn)
    parts = fn.split('/')[-1]
    parts = parts.split('.log')[0]
    parts = parts.split('_')

    exp = parts[0]
    load = parts[1]
    workload = parts[2]
    avg_size = parts[3]

    with open(fn, 'r') as f:
        lines = f.read().splitlines()
        lines = [l.split(' ') for l in lines]

        
        sizes = [int(x[1]) for x in lines]
        actual_avg_size = sum(sizes)/len(sizes)

        diffs = [float(l[0]) for l in lines]
        avg_lat = sum(diffs)/len(diffs)

        row = [str(exp+' '+workload+' '+avg_size+' '+load), exp, load, workload, avg_size, actual_avg_size, len(lines), avg_lat]
        print(row)
        if is_static:
            rows = list()
            #all
            rows.append(row)

            #small
            small_lines = [l for l in lines if int(l[1]) < small_flow]
            sizes = [int(x[1]) for x in small_lines]
            actual_avg_size = sum(sizes)/len(sizes)

            diffs = [float(l[0]) for l in small_lines]
            avg_lat = sum(diffs)/len(diffs)
            row = [str(exp+' '+workload+' '+avg_size+' small'+' '+load), exp, load, workload, avg_size, actual_avg_size, len(lines), avg_lat]
            print(row)
            rows.append(row)

            #large
            large_lines = [l for l in lines if int(l[1]) >= large_flow]
            sizes = [int(x[1]) for x in large_lines]
            actual_avg_size = sum(sizes)/len(sizes)

            diffs = [float(l[0]) for l in large_lines]
            avg_lat = sum(diffs)/len(diffs)
            row = [str(exp+' '+workload+' '+avg_size+' large'+load), exp, load, workload, avg_size, actual_avg_size, len(lines), avg_lat]
            print(row)
            rows.append(row)

            return rows
        else:
            return row

    return None

if __name__ == '__main__':

    i = 0
    for fn_pattern in dynamic_workloads:
        #break
        avg_lats = list()
        log_files=glob.glob(log_dir+'*'+fn_pattern+'*.durations')
        for fn in log_files:
            row = parse_log(fn, False)
            avg_lats.append(row)
            i+=1
            if i % 10 == 0:
                print(i)
            #break
        with open(log_dir+fn_pattern+'_'+output_fn, 'w') as f:
            writer = csv.writer(f)
            for r in sorted(avg_lats):
                writer.writerow(r)

    for fn_pattern in static_workloads:
        avg_lats = list()
        log_files=glob.glob(log_dir+'*'+fn_pattern+'*.durations')
        for fn in log_files:
            row = parse_log(fn, True)
            avg_lats.extend(row)
            i+=1
            if i % 10 == 0:
                print(i)
            #break
        with open(log_dir+fn_pattern+'_'+output_fn, 'w') as f:
            writer = csv.writer(f)
            for r in sorted(avg_lats):
                writer.writerow(r)

        
    