#pragma once

#include "blockchain/account_db.h"
#include "sql/mysql.hpp"
#include "sql/table.h"

namespace neb {

define_nt(date, std::string, "date");
define_nt(median, double, "median");
define_nt(weight, double, "weight");
define_nt(score, double, "score");
define_nt(in_degree, int32_t, "in_degree");
define_nt(out_degree, int32_t, "out_degree");
define_nt(degrees, int32_t, "degrees");
define_nt(in_val, double, "in_val");
define_nt(out_val, double, "out_val");
define_nt(in_outs, double, "in_outs");

typedef ntarray<date, address, median, weight, score, in_degree, out_degree,
                degrees, in_val, out_val, in_outs>
    nr_table_t;
typedef typename nr_table_t::row_type nr_info_t;

class nr_db_interface {
public:
  virtual void insert_document_to_collection(const nr_info_t &info) = 0;
  virtual std::vector<nr_info_t> read_nr_by_date(const std::string &date) = 0;
};

template <typename DB> class nr_db : public db<DB>, public nr_db_interface {
public:
  nr_db() {}
  nr_db(const std::string &url, const std::string &usrname,
        const std::string &passwd, const std::string &dbname)
      : db<DB>(url, usrname, passwd, dbname) {}

  virtual void insert_document_to_collection(const nr_info_t &info) {
    std::string date = info.template get<::neb::date>();
    std::string address = info.template get<::neb::address>();
    double median = info.template get<::neb::median>();
    double weight = info.template get<::neb::weight>();
    double score = info.template get<::neb::score>();
    int32_t in_degree = info.template get<::neb::in_degree>();
    int32_t out_degree = info.template get<::neb::out_degree>();
    int32_t degrees = info.template get<::neb::degrees>();
    double in_val = info.template get<::neb::in_val>();
    double out_val = info.template get<::neb::out_val>();
    double in_outs = info.template get<::neb::in_outs>();

    const std::string aql = boost::str(
        boost::format(
            "upsert {_key:'%1%'} "
            "insert {_key:'%1%', date:'%2%', address:'%3%', median:%4%, "
            "weight:%5%, score:%6%, in_degree:%7%, out_degree:%8%, "
            "degrees:%9%, in_val:%10%, out_val:%11%, in_outs:%12%} "
            "update {date:'%2%', address:'%3%', median:%4%, weight:%5%, "
            "score:%6%, in_degree:%7%, out_degree:%8%, degrees:%9%, "
            "in_val:%10%, out_val:%11%, in_outs:%12%} in nr") %
        (date + '-' + address) % date % address % median % weight % score %
        in_degree % out_degree % degrees % in_val % out_val % in_outs);
    this->aql_query(aql);
  }

  virtual std::vector<nr_info_t> read_nr_by_date(const std::string &date) {
    const std::string aql = boost::str(
        boost::format("for item in nr filter item.date=='%1%' return item") %
        date);
    auto resp_ptr = this->aql_query(aql);
    return parse_from_response(std::move(resp_ptr));
  }

  std::string to_string(const std::vector<nr_info_t> &rs) {
    boost::property_tree::ptree root;
    boost::property_tree::ptree arr;

    for (auto it = rs.begin(); it != rs.end(); it++) {
      const nr_info_t &info = *it;
      std::string date = info.template get<::neb::date>();
      std::string address = info.template get<::neb::address>();
      double median = info.template get<::neb::median>();
      double weight = info.template get<::neb::weight>();
      double score = info.template get<::neb::score>();
      int32_t in_degree = info.template get<::neb::in_degree>();
      int32_t out_degree = info.template get<::neb::out_degree>();
      int32_t degrees = info.template get<::neb::degrees>();
      double in_val = info.template get<::neb::in_val>();
      double out_val = info.template get<::neb::out_val>();
      double in_outs = info.template get<::neb::in_outs>();

      std::unordered_map<std::string, std::string> kv_pair(
          {{"date", date},
           {"address", address},
           {"median", std::to_string(median)},
           {"weight", std::to_string(weight)},
           {"score", std::to_string(score)},
           {"in_degree", std::to_string(in_degree)},
           {"out_degree", std::to_string(out_degree)},
           {"degrees", std::to_string(degrees)},
           {"in_val", std::to_string(in_val)},
           {"out_val", std::to_string(out_val)},
           {"in_outs", std::to_string(in_outs)}});

      boost::property_tree::ptree p;
      for (auto &ele : kv_pair) {
        p.put(ele.first, ele.second);
      }

      arr.push_back(std::make_pair(std::string(), p));
    }
    root.add_child("nr", arr);
    return ptree_to_string(root);
  }

private:
  void set_nr_info(nr_info_t &info, const VPackSlice &slice,
                   const std::string &key) {
    if (key.compare("date") == 0) {
      info.template set<::neb::date>(slice.copyString());
    }
    if (key.compare("address") == 0) {
      info.template set<::neb::address>(slice.copyString());
    }
    if (key.compare("median") == 0) {
      info.template set<::neb::median>(slice.isDouble() ? slice.getDouble()
                                                        : slice.getInt());
    }
    if (key.compare("weight") == 0) {
      info.template set<::neb::weight>(slice.isDouble() ? slice.getDouble()
                                                        : slice.getInt());
    }
    if (key.compare("score") == 0) {
      info.template set<::neb::score>(slice.isDouble() ? slice.getDouble()
                                                       : slice.getInt());
    }
    if (key.compare("in_degree") == 0) {
      info.template set<::neb::in_degree>(slice.getInt());
    }
    if (key.compare("out_degree") == 0) {
      info.template set<::neb::out_degree>(slice.getInt());
    }
    if (key.compare("degrees") == 0) {
      info.template set<::neb::degrees>(slice.getInt());
    }
    if (key.compare("in_val") == 0) {
      info.template set<::neb::in_val>(slice.isDouble() ? slice.getDouble()
                                                        : slice.getInt());
    }
    if (key.compare("out_val") == 0) {
      info.template set<::neb::out_val>(slice.isDouble() ? slice.getDouble()
                                                         : slice.getInt());
    }
    if (key.compare("in_outs") == 0) {
      info.template set<::neb::in_outs>(slice.isDouble() ? slice.getDouble()
                                                         : slice.getInt());
    }
  }

  std::vector<nr_info_t> parse_from_response(
      const std::unique_ptr<::arangodb::fuerte::Response> resp_ptr) {
    std::vector<nr_info_t> rs;

    auto documents = resp_ptr->slices().front().get("result");
    if (documents.isNone() || documents.isEmptyArray()) {
      return rs;
    }

    for (size_t i = 0; i < documents.length(); i++) {
      auto doc = documents.at(i);
      nr_info_t info;
      for (size_t j = 0; j < doc.length(); j++) {
        std::string key = doc.keyAt(j).copyString();
        set_nr_info(info, doc.valueAt(j), key);
      }
      rs.push_back(info);
    }
    return rs;
  }

  std::string ptree_to_string(const boost::property_tree::ptree &root) {
    std::stringstream ss;
    write_json(ss, root, false);
    LOG(INFO) << "write json: " << ss.str();
    return ss.str();
  }
};
} // namespace neb
