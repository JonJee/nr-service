#pragma once

#include "blockchain/nebulas/nebulas_currency.h"
#include "blockchain/transaction_db.h"
#include "sql/mysql.hpp"
#include "sql/table.h"

namespace neb {

define_nt(address, std::string, "address");
define_nt(balance, std::string, "balance");
define_nt(account_type, std::string, "type");
define_nt(create_at, std::string, "create_at");

typedef ntarray<address, balance, account_type, create_at, height>
    account_table_t;
typedef typename account_table_t::row_type account_info_t;

namespace internal {
template <typename TS> struct account_db_traits {};
template <> struct account_db_traits<nebulas_db> {
  static double normalize_value(double val) {
    nebulas::nas to_nas = nebulas::nas_cast<nebulas::nas>(nebulas::wei(val));
    return to_nas.value();
  }
};
template <> struct account_db_traits<eth_db> {
  static double normalize_value(double val) { return val; }
};
} // namespace internal

class account_db_interface {
public:
  virtual std::vector<account_info_t> read_account_from_db() = 0;
  virtual std::vector<account_info_t>
  read_account_by_address(const std::string &address) = 0;
  virtual account_balance_t get_account_balance(block_height_t height,
                                                const std::string &address) = 0;
  virtual double get_normalized_value(double value) = 0;
  virtual void set_height_address_val(int64_t start_block,
                                      int64_t end_block) = 0;
};

template <class DB>
class account_db : public db<DB>, public account_db_interface {
public:
  account_db() {}
  account_db(const std::string &url, const std::string &usrname,
             const std::string &passwd, const std::string &dbname)
      : db<DB>(url, usrname, passwd, dbname) {
    m_tdb_ptr =
        std::make_shared<transaction_db<DB>>(url, usrname, passwd, dbname);
  }

  virtual std::vector<account_info_t> read_account_from_db() {
    const std::string aql = "for record in account return record";
    auto resp_ptr = this->aql_query(aql);
    return parse_from_response(std::move(resp_ptr));
  }

  virtual std::vector<account_info_t>
  read_account_by_address(const std::string &address) {
    const std::string aql =
        boost::str(boost::format("for record in account filter "
                                 "record.address=='%1%' return record") %
                   address);
    auto resp_ptr = this->aql_query(aql);
    return parse_from_response(std::move(resp_ptr));
  }

  std::string to_string(const std::vector<account_info_t> &rs) {
    boost::property_tree::ptree root;
    boost::property_tree::ptree arr;

    for (auto it = rs.begin(); it != rs.end(); it++) {
      const account_info_t &info = *it;
      std::string address = info.template get<::neb::address>();
      std::string balance = info.template get<::neb::balance>();
      std::string account_type = info.template get<::neb::account_type>();
      std::string create_at = info.template get<::neb::create_at>();
      int64_t height = info.template get<::neb::height>();

      std::unordered_map<std::string, std::string> kv_pair(
          {{"address", address},
           {"balance", balance},
           {"type", account_type},
           {"create_at", create_at},
           {"height", std::to_string(height)}});

      boost::property_tree::ptree p;
      for (auto &ele : kv_pair) {
        p.put(ele.first, ele.second);
      }

      arr.push_back(std::make_pair(std::string(), p));
    }
    root.add_child("accounts", arr);
    return ptree_to_string(root);
  }

  virtual account_balance_t get_account_balance(block_height_t height,
                                                const std::string &address) {
    auto addr_it = m_addr_height_list.find(address);
    if (addr_it == m_addr_height_list.end()) {
      return 0;
    }
    auto height_it = std::lower_bound(addr_it->second.begin(),
                                      addr_it->second.end(), height);

    if (height_it == addr_it->second.end()) {
      height_it--;
      return m_height_addr_val[*height_it][address];
    }

    if (height_it == addr_it->second.begin()) {
      if (*height_it == height) {
        return m_height_addr_val[*height_it][address];
      } else {
        return 0;
      }
    }

    if (*height_it != height) {
      height_it--;
    }

    return m_height_addr_val[*height_it][address];
  }

  virtual double get_normalized_value(double value) {
    return ::neb::internal::account_db_traits<DB>::normalize_value(value);
  }

  virtual void set_height_address_val(int64_t start_block, int64_t end_block) {
    LOG(INFO) << "template account_db, init height address value begin";
    std::vector<transaction_info_t> txs =
        m_tdb_ptr
            ->read_success_and_failed_transaction_from_db_with_block_duration(
                start_block, end_block);
    LOG(INFO) << "template account_db, read transaction done";
    std::unordered_map<account_address_t, account_balance_t> addr_balance;

    for (auto it = txs.begin(); it != txs.end(); it++) {
      std::string from = it->template get<::neb::from>();
      std::string to = it->template get<::neb::to>();

      int64_t height = it->template get<::neb::height>();
      account_balance_t value = boost::lexical_cast<account_balance_t>(
          it->template get<::neb::tx_value>());

      if (addr_balance.find(from) == addr_balance.end()) {
        addr_balance.insert(std::make_pair(from, 0));
      }
      if (addr_balance.find(to) == addr_balance.end()) {
        addr_balance.insert(std::make_pair(to, 0));
      }

      int32_t status = it->template get<::neb::status>();
      if (status) {
        addr_balance[from] -= value;
        addr_balance[to] += value;
      }

      std::string gas_used = it->template get<::neb::gas_used>();
      if (gas_used.compare(std::string()) != 0) {
        std::string gas_price = it->template get<::neb::gas_price>();
        account_balance_t gas_val =
            boost::lexical_cast<account_balance_t>(gas_used) *
            boost::lexical_cast<account_balance_t>(gas_price);
        addr_balance[from] -= gas_val;
      }

      if (m_height_addr_val.find(height) == m_height_addr_val.end()) {
        std::unordered_map<std::string, account_balance_t> addr_val = {
            {from, addr_balance[from]}, {to, addr_balance[to]}};
        m_height_addr_val.insert(std::make_pair(height, addr_val));
      } else {
        std::unordered_map<std::string, account_balance_t> &addr_val =
            m_height_addr_val[height];
        if (addr_val.find(from) == addr_val.end()) {
          addr_val.insert(std::make_pair(from, addr_balance[from]));
        } else {
          addr_val[from] = addr_balance[from];
        }
        if (addr_val.find(to) == addr_val.end()) {
          addr_val.insert(std::make_pair(to, addr_balance[to]));
        } else {
          addr_val[to] = addr_balance[to];
        }
      }

      if (m_addr_height_list.find(from) == m_addr_height_list.end()) {
        std::vector<int64_t> v{height};
        m_addr_height_list.insert(std::make_pair(from, v));
      } else {
        std::vector<int64_t> &v = m_addr_height_list[from];
        // expect reading transactions order by height asc
        if (!v.empty() && v.back() < height) {
          v.push_back(height);
        }
      }

      if (m_addr_height_list.find(to) == m_addr_height_list.end()) {
        std::vector<int64_t> v{height};
        m_addr_height_list.insert(std::make_pair(to, v));
      } else {
        std::vector<int64_t> &v = m_addr_height_list[to];
        if (!v.empty() && v.back() < height) {
          v.push_back(height);
        }
      }
    }
    LOG(INFO) << "template account_db, init height address value finish";
  }

private:
  void set_account_info(account_info_t &info, const VPackSlice &slice,
                        const std::string &key) {
    if (key.compare("address") == 0) {
      info.template set<::neb::address>(slice.copyString());
    }
    if (key.compare("balance") == 0) {
      info.template set<::neb::balance>(slice.copyString());
    }
    if (key.compare("height") == 0) {
      info.template set<::neb::height>(slice.getInt());
    }
    if (key.compare("type") == 0) {
      info.template set<::neb::account_type>(slice.copyString());
    }
    if (key.compare("create_at") == 0) {
      info.template set<::neb::create_at>(slice.copyString());
    }
  }

  std::vector<account_info_t> parse_from_response(
      const std::unique_ptr<::arangodb::fuerte::Response> resp_ptr) {
    std::vector<account_info_t> rs;

    auto documents = resp_ptr->slices().front().get("result");
    if (documents.isNone() || documents.isEmptyArray()) {
      return rs;
    }

    for (size_t i = 0; i < documents.length(); i++) {
      auto doc = documents.at(i);
      account_info_t info;
      for (size_t j = 0; j < doc.length(); j++) {
        std::string key = doc.keyAt(j).copyString();
        set_account_info(info, doc.valueAt(j), key);
      }
      rs.push_back(info);
    }
    return rs;
  }

  std::string ptree_to_string(const boost::property_tree::ptree &root) {
    std::stringstream ss;
    write_json(ss, root, false);
    return ss.str();
  }

protected:
  std::shared_ptr<transaction_db<DB>> m_tdb_ptr;

  std::unordered_map<account_address_t, std::vector<block_height_t>>
      m_addr_height_list;

  std::unordered_map<block_height_t,
                     std::unordered_map<account_address_t, account_balance_t>>
      m_height_addr_val;
}; // end class account_db
} // namespace neb
