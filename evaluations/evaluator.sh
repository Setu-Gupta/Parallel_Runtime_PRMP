#!/bin/bash

echo ================================SEQUENTIAL=========================================
for benchmark in ./seq*
do
        cd $benchmark
        make clean > /dev/null
        make > /dev/null
        cd bin/release
        bin=$(ls)
        ./$bin
        cd ../..
        make clean > /dev/null
        cd ..
done
echo ================================END=========================================
echo ""
echo ""

echo ================================PARALLEL=========================================
for benchmark in ./argolib*
do
        cd $benchmark
        for i in Baseline_Argolib M1_sub M1_opt M2_sub M2_opt
        do
                echo ================================$i=========================================
                cd ../..
                git checkout $i 2>/dev/null > /dev/null
                make clean > /dev/null
                make > /dev/null
                git checkout evaluation 2>/dev/null > /dev/null
                cd evaluations/$benchmark
                make clean > /dev/null
                make > /dev/null
                cd bin/release
                bin=$(ls)
                ARGOLIB_WORKERS=20 ARGOLIB_RANDOMWS=1 ./$bin
                cd ../..
                make clean > /dev/null
                echo ++++++++++++++++++++++++++++++++$i+++++++++++++++++++++++++++++++++++++++++
        done

        cd ../..
        make clean > /dev/null
        cd evaluations
        echo ""
done
echo ================================END=========================================
