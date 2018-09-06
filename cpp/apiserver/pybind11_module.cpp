#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "api/account_apiserver.h"
#include "api/nr_apiserver.h"
#include "api/transaction_apiserver.h"
#include "apiserver.h"

PYBIND11_MODULE(nebserver, m) {
  google::InitGoogleLogging("nebserver");
  FLAGS_log_dir = std::string(std::getenv("GLOG_DIR"));

  pybind11::class_<apiserver>(m, "apiserver")
      .def(pybind11::init<const std::string &>())
      .def("on_api_example", &apiserver::on_api_example);

  pybind11::class_<transaction_apiserver>(m, "transaction_apiserver")
      .def(pybind11::init<const std::string &>())
      .def("on_api_transaction", &transaction_apiserver::on_api_transaction);

  pybind11::class_<account_apiserver>(m, "account_apiserver")
      .def(pybind11::init<const std::string &>())
      .def("on_api_account", &account_apiserver::on_api_account);

  pybind11::class_<nr_apiserver>(m, "nr_apiserver")
      .def(pybind11::init<const std::string &>())
      .def("on_api_nr", &nr_apiserver::on_api_nr);
}
