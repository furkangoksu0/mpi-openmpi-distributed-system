#!/bin/bash

# OpenMP thread sayısını ayarla
export OMP_NUM_THREADS=4

# Derleme
mpic++ -fopenmp /app/src/main.cpp -o /app/main

# MPI ile çalıştırma
mpirun -np 3 --allow-run-as-root /app/main 