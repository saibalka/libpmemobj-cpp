// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/**
 * radix_tree_basic.cpp -- example which shows how to use
 * pmem::obj::experimental::radix_tree.
 */

#include <libpmemobj++/experimental/inline_string.hpp>
#include <libpmemobj++/experimental/radix_tree.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/pool.hpp>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>

#include <cstdlib>  // for srand(), rand()
#include <ctime>    // for time()
#include <vector>   // std::vector
#include <map>

// #define count  1000000
// #define count  50000
#define SAMPLE 100

struct data {
  unsigned long index;
  unsigned long data_1, data_2;
};

typedef struct timing_info_s {
  long long unsigned int start;
  long long unsigned int end;
  long long unsigned int diff;
} timing_info_t;

timing_info_t timing_info, timing_info_map;

int count = 10000;

using value_type =  struct data;

using kv_type = pmem::obj::experimental::radix_tree<
	unsigned long, pmem::obj::persistent_ptr<value_type>>;

std::vector<unsigned long> key_vector;
std::vector<unsigned long> ne_key_vector;

struct root {
	pmem::obj::persistent_ptr<kv_type> kv;
};

std::map<unsigned long, std::shared_ptr<value_type>> mymap;

inline int64_t rdtsc_s(void)
{
  unsigned a, d;
  asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
  asm volatile("rdtsc" : "=a" (a), "=d" (d));
  return ((unsigned long)a) | (((unsigned long)d) << 32);
}

inline int64_t rdtsc_e(void)
{
  unsigned a, d;
  asm volatile("rdtscp" : "=a" (a), "=d" (d));
  asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
  return ((unsigned long)a) | (((unsigned long)d) << 32);
}

void print_one_sec_rdtsc()
{
  timing_info.start = rdtsc_s(); 
  sleep(1);
  timing_info.end = rdtsc_e();
  timing_info.diff = (timing_info.end - timing_info.start) >> 1;
}

void
show_usage(char *argv[])
{
	std::cerr << "usage: " << argv[0] << " file-name [count]" << std::endl;
}

void
insert_ne_elements_kv(void) {
  unsigned long key;
  unsigned key1;

  for (int i = 0; i < count; i++) {
    if (i % SAMPLE == 0 ) {
      key = rand();
      key <<= 32;
      do {
        key1 = rand();
      } while ((key1 & 0x1) == 0);
      key = key | key1;
      ne_key_vector.emplace_back(key);
    }
  }
}

void
insert_elements_kv(pmem::obj::pool<root> pop)
{
	auto r = pop.root();

  unsigned long key;
  unsigned key1;

  std::cerr << "Inserting " << count << " elements" << std::endl;
  timing_info = {0, 0, 0};
  timing_info_map = {0, 0, 0};
//   std::cerr << "diff1: " << timing_info.diff << " diff2: "
//             << timing_info_map.diff << std::endl;
  for (int i = 0; i < count; i++) {
//   for (int i = 0; i < 100000; i++) {
    key = rand();
    key <<= 32;
    do {
        key1 = rand();
    } while ((key1 & 0x1));
    key = key | key1;

    if (i % SAMPLE == 0 ) {
//        std::cerr << "key: " << key << " index: " << i << std::endl;
      key_vector.emplace_back(key);
    }
    if (i && (i % (SAMPLE * 5000) == 0)) {
      std::cerr << "Inserted " << i << " elements" << std::endl;
    }
    timing_info.start = rdtsc_s();
	  pmem::obj::transaction::run(pop, [&] {
      auto value = pmem::obj::make_persistent<value_type>();
      value->index = i;

		  r->kv->try_emplace(key, value);
	  });
    timing_info.end = rdtsc_e();
    timing_info.diff += (timing_info.end - timing_info.start) >> 1;

    timing_info_map.start = rdtsc_s();
    auto map_value = std::make_shared<value_type>();
    map_value->index = i;
    mymap.emplace(key, map_value);
    timing_info_map.end = rdtsc_e();
    timing_info_map.diff += (timing_info_map.end - timing_info_map.start) >> 1;
  }
  std::cerr << "Average insert time: (peristent radix tree): "
            <<  timing_info.diff/count << std::endl;
//             << timing_info.diff << " iter: " << count << " avg: " <<  timing_info.diff/count << std::endl;
  std::cerr << "Average insert time (map): "
//             << timing_info_map.diff << " iter: " << count << " avg: " <<  timing_info_map.diff/count << std::endl;
            << timing_info_map.diff / count << std::endl;
//   std::cerr << "size: " << key_vector.size() << std::endl;
}

void
lookup_elements_kv(pmem::obj::pool<root> pop) {
	auto r = pop.root();

  timing_info.start = rdtsc_s();
  for (auto &key : key_vector) {
//   for (auto key : key_vector) {
//     std::cerr << "key: " << key << std::endl;
     r->kv->find(key);
//      auto it = r->kv->find(key);
//      assert(it != r->kv->end());
//     std::cerr << "key: " << key << " index: " << it->value()->index << std::endl;
  }
  timing_info.end = rdtsc_e();
  timing_info.diff = (timing_info.end - timing_info.start) >> 1;
  timing_info.diff = timing_info.diff / key_vector.size();
  std::cerr << "Average access time (Persistent radix tree): "
            << timing_info.diff << std::endl;

  timing_info.start = rdtsc_s();
  for (auto &key : key_vector) {
//   for (auto key : key_vector) {
//     std::cerr << "key: " << key << std::endl;
     mymap.find(key);
//      auto it = mymap.find(key);
//     auto it = r->kv->find(key);
//      assert(it != mymap.end());
//     std::cerr << "key: " << key << " index: " << it->value()->index << std::endl;
  }
  timing_info.end = rdtsc_e();
  timing_info.diff = (timing_info.end - timing_info.start) >> 1;
  timing_info.diff = timing_info.diff / key_vector.size();
  std::cerr << "Average access time (map): " << timing_info.diff << std::endl;
}

void
lookup_ne_elements_kv(pmem::obj::pool<root> pop) {
	auto r = pop.root();

  timing_info.start = rdtsc_s();
  for (auto &key : ne_key_vector) {
//   for (auto key : key_vector) {
//     std::cerr << "key: " << key << std::endl;
      r->kv->find(key);
//       auto it = r->kv->find(key);
//       assert(it == r->kv->end());
//     std::cerr << "key: " << key << " index: " << it->value()->index << std::endl;
  }
  timing_info.end = rdtsc_e();
  timing_info.diff = (timing_info.end - timing_info.start) >> 1;
  timing_info.diff = timing_info.diff / ne_key_vector.size();
  std::cerr << "[Key not present] Average access time (Persistent radix tree): "
            << timing_info.diff << std::endl;

  timing_info.start = rdtsc_s();
  for (auto &key : ne_key_vector) {
//   for (auto key : key_vector) {
//     std::cerr << "key: " << key << std::endl;
      mymap.find(key);
//       auto it = mymap.find(key);
//     auto it = r->kv->find(key);
//       assert(it == mymap.end());
//     std::cerr << "key: " << key << " index: " << it->value()->index << std::endl;
  }
  timing_info.end = rdtsc_e();
  timing_info.diff = (timing_info.end - timing_info.start) >> 1;
  timing_info.diff = timing_info.diff / ne_key_vector.size();
  std::cerr << "[Key not present] Average access time (map): "
            << timing_info.diff << std::endl;
}
void
remove_all_elements_kv(pmem::obj::pool<root> pop) {
  auto r = pop.root();

  for (auto it = r->kv->cbegin(); it != r->kv->cend();) {
	  pmem::obj::transaction::run(pop, [&] {
      pmem::obj::delete_persistent<value_type>(it->value());
      auto tmp_it = it;
      it++;
      r->kv->erase(tmp_it);
    });
  }
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		show_usage(argv);
		return 1;
	}

	const char *path = argv[1];
  if (argc > 2) {
    count = atoi(argv[2]);
    if (count <= 0) {
      count = 10000;
    }
  }

  srand(time(0)); // seed the random number generator
  key_vector.reserve(count / SAMPLE);

  print_one_sec_rdtsc();
  std::cerr << "1 Sec = " << timing_info.diff << std::endl;
  std::cerr << "Key Size: " << sizeof(unsigned long) << std::endl;
  std::cerr << "Value Size: " << sizeof(struct data) << std::endl;

	pmem::obj::pool<root> pop;

	try {
		pop = pmem::obj::pool<root>::open(path, "radix");
		auto r = pop.root();

		if (r->kv == nullptr) {
			pmem::obj::transaction::run(pop, [&] {
				r->kv = pmem::obj::make_persistent<kv_type>();
			});
		}
	} catch (pmem::pool_error &e) {
		std::cerr << e.what() << std::endl;
		std::cerr
			<< "To create pool run: pmempool create obj --layout=radix -s 100M path_to_pool"
			<< std::endl;
	} catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
	}

  insert_ne_elements_kv();
	try {
		std::cerr << "Inserting elements..." << std::endl;
		insert_elements_kv(pop);
		std::cerr << "Looking up " << key_vector.size() << " elements..."
              << std::endl;
    lookup_elements_kv(pop);
		std::cerr << "[Key mot present] Looking up " << ne_key_vector.size()
              << " elements..." << std::endl;
    lookup_ne_elements_kv(pop);
		std::cerr << "Removing elements..." << std::endl;
    remove_all_elements_kv(pop);

		pop.close();
	} catch (const std::logic_error &e) {
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
