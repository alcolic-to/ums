// NOLINTBEGIN

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

#include "file.hpp"
#include "io.hpp"
#include "ums.hpp"
#include "util.hpp"

using namespace ums;
using namespace std::chrono_literals;

void test_write_read_file(uint64_t io_size)
{
    const fs::path file_path{"io_file"};

    std::vector<char> io_data(io_size, 'a');

    {
        File_handle file{file_path};
        cos_write_file(file, {io_data.data(), io_data.size()}, 0);

        std::vector<char> read_vec(io_size);
        cos_read_file(file, {read_vec.data(), read_vec.size()}, 0);

        ASSERT_TRUE(io_data == read_vec);
    }

    fs::remove(file_path);
}

void seq_writes_and_reads(uint64_t io_size, uint32_t iterations, bool forward = true)
{
    const fs::path file_path{"io_file"};
    std::vector<std::vector<char>> io_data(iterations, std::vector<char>(io_size));

    {
        File_handle file{file_path};

        for (uint32_t i = 0; i < iterations; ++i) {
            std::ranges::fill(io_data[i], char('a' + i));
            uint64_t offset = io_size * (forward ? i : (iterations - 1 - i));
            cos_write_file(file, {io_data[i].data(), io_data[i].size()}, offset);
        }

        for (uint32_t i = 0; i < iterations; ++i) {
            std::vector<char> read_vec(io_size);
            uint64_t offset = io_size * (forward ? i : (iterations - 1 - i));
            cos_read_file(file, {read_vec.data(), read_vec.size()}, offset);
            ASSERT_TRUE(io_data[i] == read_vec);
        }
    }

    fs::remove(file_path);
}

void random_writes_and_reads(uint64_t io_size, uint32_t iterations)
{
    const fs::path file_path{"io_file"};
    std::vector<std::vector<char>> io_data(iterations, std::vector<char>(io_size));

    std::vector<uint32_t> v_rand(iterations);
    std::generate(v_rand.begin(), v_rand.end(), [&] { return random() % iterations; });

    {
        File_handle file{file_path};

        for (uint32_t i = 0; i < iterations; ++i) {
            uint32_t idx = v_rand[i];
            std::vector<char>& io_v = io_data[idx];
            std::ranges::fill(io_v, char('a' + idx));
            cos_write_file(file, {io_v.data(), io_v.size()}, io_size * idx);
        }

        for (uint32_t i = 0; i < iterations; ++i) {
            uint32_t idx = v_rand[i];
            std::vector<char> read_vec(io_size);
            cos_read_file(file, {read_vec.data(), read_vec.size()}, io_size * idx);
            ASSERT_TRUE(io_data[idx] == read_vec);
        }
    }

    fs::remove(file_path);
}

TEST(IO, sanity_test)
{
    const fs::path file_path{"io_file"};
    std::string text{"Let's assure that I/O is working on a machine."};

    {
        std::ofstream f1{file_path};
        f1 << text;
    }

    std::string str_file{file_to_string(file_path.string())};

    ASSERT_TRUE(text == str_file);

    fs::remove(file_path);

    std::vector<char> data(100, 'a');

    {
        std::ofstream f2{file_path};
        f2.write(data.data(), data.size());
    }

    std::vector<char> vec_file{file_to_vector(file_path.string())};

    ASSERT_TRUE(data == vec_file);

    fs::remove(file_path);
}

TEST(IO, sanity_write_test)
{
    auto test = [] {
        constexpr uint64_t io_size = 1 * 1024;
        const fs::path file_path{"io_file"};

        std::vector<char> io_data(io_size, 'a');

        {
            File_handle file{file_path};
            cos_write_file(file, {io_data.data(), io_data.size()}, 0);
        }

        auto vec{file_to_vector(file_path.string())};

        ASSERT_TRUE(vec == io_data);

        fs::remove(file_path);
    };

    init_ums(test);
}

TEST(IO, sanity_read_test)
{
    auto test = [] {
        constexpr uint64_t io_size = 1 * 1024;
        const fs::path file_path{"io_file"};

        std::vector<char> io_data(io_size, 'a');

        {
            std::ofstream f{file_path};
            f.write(io_data.data(), io_data.size());
        }

        std::vector<char> read_vec(io_size);

        {
            File_handle file{file_path};
            cos_read_file(file, {read_vec.data(), read_vec.size()}, 0);
        }

        ASSERT_TRUE(io_data == read_vec);

        fs::remove(file_path);
    };

    init_ums(test);
}

TEST(IO, sanity_write_read_test)
{
    auto test = [] {
        constexpr uint64_t io_size = 1 * 1024;
        const fs::path file_path{"io_file"};

        std::vector<char> io_data(io_size, 'a');

        {
            File_handle file{file_path};
            cos_write_file(file, {io_data.data(), io_data.size()}, 0);

            std::vector<char> read_vec(io_size);
            cos_read_file(file, {read_vec.data(), read_vec.size()}, 0);

            ASSERT_TRUE(io_data == read_vec);
        }

        fs::remove(file_path);
    };

    init_ums(test);
}

TEST(IO, io_with_different_sizes)
{
    auto test = [] {
        for (uint64_t io_size = 1024; io_size <= 10 * 1024 * 1024; io_size <<= 1U)
            test_write_read_file(io_size);
    };

    init_ums(test);
}

TEST(IO, io_seq_writes_and_reads)
{
    auto test = [] {
        for (uint32_t i = 1; i <= 10; ++i) {
            for (uint32_t io_size = 512; io_size <= 1 * 1024 * 1024; io_size <<= 1U) {
                seq_writes_and_reads(io_size, i, true);
                seq_writes_and_reads(io_size, i, false);
            }
        }
    };

    init_ums(test);
}

TEST(IO, io_random_writes_and_reads)
{
    auto test = [] {
        for (uint32_t i = 1; i <= 10; ++i)
            for (uint32_t io_size = 512; io_size <= 1 * 1024 * 1024; io_size <<= 1U)
                random_writes_and_reads(io_size, i);
    };

    init_ums(test);
}

// NOLINTEND
