import sys
from matplotlib import pyplot as plt

opt_vals = []
with open(sys.argv[1], 'r') as infile:
    for line in infile.readlines():
        line = line.strip()
        if 'e-' in line and 'ms' not in line:
            opt_vals.append(float(line))

unopt_vals = []
with open(sys.argv[2], 'r') as infile:
    for line in infile.readlines():
        line = line.strip()
        if 'e-' in line and 'ms' not in line:
            unopt_vals.append(float(line))

plt.title("Joules per instruction (JPI) v/s Execution time")
plt.xlabel("Sample number")
plt.ylabel("JPI")
plt.autoscale(enable=True, axis='both', tight=True)
plt.grid()
plt.plot(unopt_vals, label="Power unoptimized")
plt.plot(opt_vals, label="Power optimized")
plt.legend()
plt.savefig(sys.argv[3])
