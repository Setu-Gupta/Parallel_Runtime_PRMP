#!/bin/bash

# Get sudo permission in the beginning so that the password prompt gets suppressed for future calls
sudo true;
cur_dir=$(pwd)
git checkout power_optimization;

# Run for the power optimized case
git checkout PowerOpt;
make clean;
make -j;
cd tests/iterative_averages;
make clean;
make -j;
cd bin/release;
sudo LD_LIBRARY_PATH=$PCMROOT/build/lib:$ARGOBOTS_INSTALL_DIR/lib:$ARGOLIB_INSTALL_DIR/release/lib ARGOLIB_WORKERS=17 ./iterative > stats_opt_iter;

# Run for the unoptimized power case
cd $cur_dir;
git checkout PowerUnopt;
make clean;
make -j;
cd tests/iterative_averages;
make clean;
make -j;
cd bin/release;
sudo LD_LIBRARY_PATH=$PCMROOT/build/lib:$ARGOBOTS_INSTALL_DIR/lib:$ARGOLIB_INSTALL_DIR/release/lib ARGOLIB_WORKERS=17 ./iterative > stats_unopt_iter;

# Come back to the root directory
cd $cur_dir;
git checkout power_optimization;

# Move the generated stats to plotting directory and generate plots
mv tests/iterative_averages/bin/release/stats_* plotting_scripts/.;
cd plotting_scripts;
python3 plotter.py stats_opt_iter stats_unopt_iter power_plot_iter.png;
