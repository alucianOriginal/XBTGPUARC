#include "miner_loop.hpp"

#include <CL/cl.h>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iomanip>

// Watchdog-Ersatz-Flag von extern (stratum_client.cpp)
extern std::atomic<bool> job_wurde_übernommen;

// Globale OpenCL Objekte
cl_context context = nullptr;
cl_command_queue queue = nullptr;
cl_kernel kernel = nullptr;
cl_program program = nullptr;
cl_device_id device = nullptr;

// Globale Abbruch-Flag
std::atomic<bool> abort_mining = false;

void stop_mining() {
    abort_mining = true;
}

void init_opencl() {
    if (context) return;

    cl_int err;
    cl_platform_id platform;
    clGetPlatformIDs(1, &platform, nullptr);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    queue = clCreateCommandQueueWithProperties(context, device, 0, &err);

    std::ifstream file("kernels/zhash.cl");
    if (!file) {
        std::cerr << "❌ Konnte Kernel-Datei nicht öffnen.\n";
        std::exit(1);
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const char* src = source.c_str();
    size_t size = source.size();

    program = clCreateProgramWithSource(context, 1, &src, &size, &err);
    err = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);

    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr);
        std::cerr << "❌ Build-Fehler:\n" << log.data() << "\n";
        std::exit(1);
    }

    kernel = clCreateKernel(program, "zhash_144_5", &err);
}

bool is_valid_candidate_pair(const std::vector<uint8_t>& hashA, const std::vector<uint8_t>& hashB) {
    if (hashA.empty() || hashB.empty()) return false;
    return (hashA[0] ^ hashB[0]) == 0;
}

void miner_loop(const MiningJob& job,
                const std::function<void(uint32_t nonce, const std::array<uint8_t, 32>& hash, const MiningJob& job)>& on_valid_share,
                int intensity) {
    abort_mining = false;
    init_opencl();

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist;

    std::vector<uint8_t> target = bits_to_target(std::stoul(job.bits, nullptr, 16));
    const int batch_size = intensity * 1024;
    std::cout << "🔢 Workload (batch_size): " << batch_size << "\n";

    std::vector<cl_uchar> input_buffer(512 * batch_size);
    std::vector<cl_uchar> output_buffer(32 * batch_size);
    std::vector<cl_uint> index_buffer(2 * batch_size, 0);

    cl_int err;
    cl_mem cl_input = clCreateBuffer(context, CL_MEM_READ_ONLY, input_buffer.size(), nullptr, &err);
    cl_mem cl_output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, output_buffer.size(), nullptr, &err);
    cl_mem cl_indexes = clCreateBuffer(context, CL_MEM_WRITE_ONLY, index_buffer.size() * sizeof(cl_uint), nullptr, &err);

    clSetKernelArg(kernel, 0, sizeof(int), &batch_size);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &cl_input);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &cl_output);
    clSetKernelArg(kernel, 3, sizeof(cl_mem), &cl_indexes);

    auto start_time = std::chrono::steady_clock::now();
    uint64_t total_hashes = 0;

    static bool erster_durchlauf = true;

    // ➤ Check: Wurde der letzte Job verarbeitet?
    if (!erster_durchlauf && !job_wurde_übernommen.load()) {
        std::cerr << "⚠️ WARNUNG: Neuer Job kam, aber vorheriger Job wurde nie gestartet!" << std::endl;
        // Optional: Logging oder Recovery
    }
    erster_durchlauf = false;

    job_wurde_übernommen.store(true);  // Jetzt wird mit dem aktuellen Job gearbeitet

    while (!abort_mining.load()) {
        uint32_t start_nonce = dist(rng);

        for (int i = 0; i < batch_size; ++i) {
            uint32_t nonce = start_nonce + i;
            cl_uchar* in = &input_buffer[i * 512];
            in[0] = nonce & 0xFF;
            in[1] = (nonce >> 8) & 0xFF;
            in[2] = (nonce >> 16) & 0xFF;
            in[3] = (nonce >> 24) & 0xFF;
        }

        clEnqueueWriteBuffer(queue, cl_input, CL_TRUE, 0, input_buffer.size(), input_buffer.data(), 0, nullptr, nullptr);
        size_t global_work_size = batch_size;
        clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_work_size, nullptr, 0, nullptr, nullptr);
        clFinish(queue);
        clEnqueueReadBuffer(queue, cl_output, CL_TRUE, 0, output_buffer.size(), output_buffer.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, cl_indexes, CL_TRUE, 0, index_buffer.size() * sizeof(cl_uint), index_buffer.data(), 0, nullptr, nullptr);

        total_hashes += batch_size;

        for (int i = 0; i < batch_size; ++i) {
            uint32_t idxA = index_buffer[i * 2];
            uint32_t idxB = index_buffer[i * 2 + 1];

            if (idxA == 0 && idxB == 0) continue;

            std::vector<uint8_t> hashA(output_buffer.begin() + idxA * 32, output_buffer.begin() + (idxA + 1) * 32);
            std::vector<uint8_t> hashB(output_buffer.begin() + idxB * 32, output_buffer.begin() + (idxB + 1) * 32);

            if (is_valid_candidate_pair(hashA, hashB)) {
                std::array<uint8_t, 32> final_hash;
                for (int j = 0; j < 32; ++j)
                    final_hash[j] = hashA[j] ^ hashB[j];

                if (is_valid_hash(final_hash, target)) {
                    std::cout << "✅ Gültiges Pair! Index " << idxA << " & " << idxB << " → Share!\n";
                    uint32_t nonce = start_nonce + idxA;
                    on_valid_share(nonce, final_hash, job);
                } else {
                    std::cout << "⚠️ Pair geprüft, aber unter Target\n";
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        if (elapsed >= 10) {
            double khs = total_hashes / (double)elapsed / 1000.0;
            std::cout << "⚙️ Aktuelle Leistung: " << std::fixed << std::setprecision(2) << khs << " kH/s\n";
            total_hashes = 0;
            start_time = now;
        }
    }

    std::cout << "🛑 Mining abgebrochen – neuer Job wird angenommen\n";
                }

