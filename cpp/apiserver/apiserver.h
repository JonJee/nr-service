#pragma once

#include "blockchain.h"
#include "cache/lru_cache.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

typedef neb::lru_cache<neb::block_height_t,
                       std::vector<neb::transaction_info_t>>
    cache_t;
typedef neb::transaction_db<neb::nebulas_db> nebulas_transaction_db_t;
typedef std::shared_ptr<nebulas_transaction_db_t> nebulas_transaction_db_ptr_t;

class apiserver {
public:
  apiserver(const std::string &appname, size_t cache_size = 128);

  void
  on_api_example(const std::unordered_map<std::string, std::string> &params);

  std::string on_api_transaction(int64_t start_block, int64_t end_block);

private:
  void set_cache(cache_t &cache, neb::block_height_t height);

protected:
  std::string m_appname;
  std::shared_ptr<cache_t> m_cache_ptr;
  nebulas_transaction_db_ptr_t m_tx_ptr;

}; // end class apiserver

namespace py = pybind11;

PYBIND11_MODULE(nebserver, m) {
  py::class_<apiserver>(m, "apiserver")
      .def(py::init<const std::string &, int32_t>())
      .def("on_api_example", &apiserver::on_api_example)
      .def("on_api_transaction", &apiserver::on_api_transaction);
}

