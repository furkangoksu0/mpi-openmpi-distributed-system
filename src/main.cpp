#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>

std::vector<int> readCSV(const std::string& filename) {
    std::vector<int> numbers;
    std::ifstream file(filename);
    std::string line;
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string value;
        while (std::getline(ss, value, ',')) {
            numbers.push_back(std::stoi(value));
        }
    }
    return numbers;
}

// Sadece MPI ile işlem
void processMPIOnly(const std::vector<int>& local_data, int chunk_size, 
                   int& local_sum, double& local_avg, int& local_max) {
    local_sum = 0;
    local_max = local_data[0];
    
    for (int i = 0; i < chunk_size; i++) {
        local_sum += local_data[i];
        if (local_data[i] > local_max) {
            local_max = local_data[i];
        }
    }
    local_avg = static_cast<double>(local_sum) / chunk_size;
}

// Sadece OpenMP ile işlem
void processOpenMPOnly(const std::vector<int>& data, 
                      int& total_sum, double& total_avg, int& total_max) {
    total_sum = 0;
    total_max = data[0];
    
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            #pragma omp parallel for reduction(+:total_sum)
            for (size_t i = 0; i < data.size(); i++) {
                total_sum += data[i];
            }
        }
        
        #pragma omp section
        {
            #pragma omp parallel for reduction(max:total_max)
            for (size_t i = 0; i < data.size(); i++) {
                if (data[i] > total_max) {
                    total_max = data[i];
                }
            }
        }
    }
    total_avg = static_cast<double>(total_sum) / data.size();
}

// Hibrit (MPI + OpenMP) işlem
void processHybrid(const std::vector<int>& local_data, int chunk_size,
                  int& local_sum, double& local_avg, int& local_max) {
    local_sum = 0;
    local_max = local_data[0];
    
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            #pragma omp parallel for reduction(+:local_sum)
            for (int i = 0; i < chunk_size; i++) {
                local_sum += local_data[i];
            }
        }
        
        #pragma omp section
        {
            #pragma omp parallel for reduction(max:local_max)
            for (int i = 0; i < chunk_size; i++) {
                if (local_data[i] > local_max) {
                    local_max = local_data[i];
                }
            }
        }
    }
    local_avg = static_cast<double>(local_sum) / chunk_size;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::vector<int> data;
    int data_size;

    // Sadece ana düğüm çıktı versin
    if (world_rank == 0) {
        data = readCSV("/app/src/dataset/numbers.csv");
        data_size = data.size();
        std::cout << "\n=== Test Başlıyor ===" << std::endl;
        std::cout << "Veri Boyutu: " << data_size << " sayı" << std::endl;
        std::cout << "MPI Düğüm Sayısı: " << world_size << std::endl;
        std::cout << "OpenMP Thread Sayısı: " << omp_get_max_threads() << std::endl;
        std::cout << "========================\n" << std::endl;
    }

    MPI_Bcast(&data_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int chunk_size = data_size / world_size;
    std::vector<int> local_data(chunk_size);

    // Tüm testler için toplam süreleri tutacak değişkenler
    double total_mpi_time = 0;
    double total_omp_time = 0;
    double total_hybrid_time = 0;
    const int TEST_COUNT = 5;

    for (int test = 1; test <= TEST_COUNT; test++) {
        if (world_rank == 0) {
            std::cout << "Test #" << test << " çalıştırılıyor..." << std::endl;
        }

        // 1. MPI Testi
        MPI_Barrier(MPI_COMM_WORLD); // Tüm düğümleri senkronize et
        double mpi_start = MPI_Wtime();
        
        MPI_Scatter(data.data(), chunk_size, MPI_INT,
                    local_data.data(), chunk_size, MPI_INT,
                    0, MPI_COMM_WORLD);

        int mpi_local_sum, mpi_local_max;
        double mpi_local_avg;
        processMPIOnly(local_data, chunk_size, mpi_local_sum, mpi_local_avg, mpi_local_max);

        int mpi_global_sum, mpi_global_max;
        double mpi_global_avg;
        
        MPI_Reduce(&mpi_local_sum, &mpi_global_sum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&mpi_local_max, &mpi_global_max, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&mpi_local_avg, &mpi_global_avg, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        
        double mpi_end = MPI_Wtime();
        double mpi_time = mpi_end - mpi_start;
        total_mpi_time += mpi_time;

        // 2. OpenMP Testi (sadece ana düğümde)
        double omp_time = 0;
        if (world_rank == 0) {
            int omp_sum, omp_max;
            double omp_avg;
            
            auto omp_start = std::chrono::high_resolution_clock::now();
            processOpenMPOnly(data, omp_sum, omp_avg, omp_max);
            auto omp_end = std::chrono::high_resolution_clock::now();
            
            omp_time = std::chrono::duration<double>(omp_end - omp_start).count();
            total_omp_time += omp_time;
        }

        // 3. Hibrit Test
        MPI_Barrier(MPI_COMM_WORLD);
        double hybrid_start = MPI_Wtime();
        
        int hybrid_local_sum, hybrid_local_max;
        double hybrid_local_avg;
        processHybrid(local_data, chunk_size, hybrid_local_sum, hybrid_local_avg, hybrid_local_max);

        int hybrid_global_sum, hybrid_global_max;
        double hybrid_global_avg;
        
        MPI_Reduce(&hybrid_local_sum, &hybrid_global_sum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&hybrid_local_max, &hybrid_global_max, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&hybrid_local_avg, &hybrid_global_avg, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        
        double hybrid_end = MPI_Wtime();
        double hybrid_time = hybrid_end - hybrid_start;
        total_hybrid_time += hybrid_time;

        // Sadece ana düğüm test sonuçlarını yazdırsın
        if (world_rank == 0) {
            printf("Test #%d Sonuçları:\n", test);
            printf("  MPI Süresi    : %.6f saniye\n", mpi_time);
            printf("  OpenMP Süresi : %.6f saniye\n", omp_time);
            printf("  Hibrit Süresi : %.6f saniye\n", hybrid_time);
            printf("------------------------\n");
        }
    }

    // Final sonuçları sadece ana düğümde göster
    if (world_rank == 0) {
        std::cout << "\n=== Final Sonuçları ===" << std::endl;
        printf("Ortalama MPI Süresi    : %.6f saniye\n", total_mpi_time / TEST_COUNT);
        printf("Ortalama OpenMP Süresi : %.6f saniye\n", total_omp_time / TEST_COUNT);
        printf("Ortalama Hibrit Süresi : %.6f saniye\n", total_hybrid_time / TEST_COUNT);
        std::cout << "======================" << std::endl;
    }

    MPI_Finalize();
    return 0;
} 