import sys
from matplotlib import pyplot as plt

vals = []
with open(sys.argv[1], 'r') as infile:
    for line in infile.readlines():
        line = line.strip()
        if 'e-' in line and 'ms' not in line:
            vals.append(float(line))

plt.title("Joules per instruction (JPI) v/s Execution time")
plt.xlabel("Sample number")
plt.ylabel("JPI")
plt.autoscale(enable=True, axis='both', tight=True)
plt.grid()
plt.plot(vals)
plt.savefig(sys.argv[2])
