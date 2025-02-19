// Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//
//
// This file contains the CQLServiceImpl class that implements the CQL server to handle requests
// from Cassandra clients using the CQL native protocol.

#pragma once

#include <vector>

#include <boost/compute/detail/lru_cache.hpp>

#include "yb/client/client_fwd.h"

#include "yb/util/object_pool.h"

#include "yb/yql/cql/cqlserver/cql_processor.h"
#include "yb/yql/cql/cqlserver/cql_server_options.h"
#include "yb/yql/cql/cqlserver/cql_service.service.h"
#include "yb/yql/cql/cqlserver/cql_statement.h"
#include "yb/yql/cql/cqlserver/system_query_cache.h"
#include "yb/yql/cql/ql/parser/parser_fwd.h"
#include "yb/yql/cql/ql/util/cql_message.h"

namespace yb {

class JsonWriter;

namespace cqlserver {

extern const char* const kRoleColumnNameSaltedHash;
extern const char* const kRoleColumnNameCanLogin;

class CQLMetrics;
class CQLProcessor;
class CQLServer;
class StatementMetrics;

class CQLServiceImpl : public CQLServerServiceIf,
                       public GarbageCollector,
                       public std::enable_shared_from_this<CQLServiceImpl> {
 public:
  // Constructor.
  CQLServiceImpl(CQLServer* server, const CQLServerOptions& opts);
  ~CQLServiceImpl();

  void CompleteInit();

  void Shutdown() override;

  void FillEndpoints(const rpc::RpcServicePtr& service, rpc::RpcEndpointMap* map) override;

  // Processing all incoming request from RPC and sending response back.
  void Handle(yb::rpc::InboundCallPtr call) override;

  // Either gets an available processor or creates a new one.
  Result<CQLProcessor*> GetProcessor();

  // Return CQL processor at pos as available.
  void ReturnProcessor(const CQLProcessorListPos& pos);

  // Allocate a prepared statement. If the statement already exists, return it instead.
  std::shared_ptr<CQLStatement> AllocatePreparedStatement(
      const ql::CQLMessage::QueryId& id, const std::string& query, ql::QLEnv* ql_env);

  // Look up a prepared statement by its id. Nullptr will be returned if the statement is not found.
  Result<std::shared_ptr<const CQLStatement>> GetPreparedStatement(
      const ql::CQLMessage::QueryId& id, SchemaVersion version);

  std::shared_ptr<ql::Statement> GetAuthPreparedStatement() const { return auth_prepared_stmt_; }

  // Delete the prepared statement from the cache.
  void DeletePreparedStatement(const std::shared_ptr<const CQLStatement>& stmt);

  // Check that the password and hash match.  Leverages shared LRU cache.
  bool CheckPassword(const std::string plain, const std::string expected_bcrypt_hash);

  // Return the memory tracker for prepared statements.
  const MemTrackerPtr& prepared_stmts_mem_tracker() const {
    return prepared_stmts_mem_tracker_;
  }

  const MemTrackerPtr& processors_mem_tracker() const {
    return processors_mem_tracker_;
  }

  const MemTrackerPtr& requests_mem_tracker() const {
    return requests_mem_tracker_;
  }

  // Return the YBClient to communicate with either master or tserver.
  client::YBClient* client() const;

  // Return the YBClientCache.
  const std::shared_ptr<client::YBMetaDataCache>& metadata_cache() const;

  // Return the CQL metrics.
  const std::shared_ptr<CQLMetrics>& cql_metrics() const { return cql_metrics_; }

  ThreadSafeObjectPool<ql::Parser>& parser_pool() { return parser_pool_; }

  // Return the messenger.
  rpc::Messenger* messenger() { return messenger_; }

  client::TransactionPool& TransactionPool();

  server::Clock* clock();

  std::shared_ptr<SystemQueryCache> system_cache() { return system_cache_; }

  void DumpStatementMetricsAsJson(JsonWriter* jw);

  // Get the list of prepared statements and metrics in an inmemory vector.
  void GetPreparedStatementMetrics(std::vector<std::shared_ptr<StatementMetrics>>* metrics);

  // Update the counters for the prepared stmt. Acquires the "prepared_stmts_mutex_".
  void UpdatePrepStmtCounters(const ql::CQLMessage::QueryId& query_id, double execute_time_in_msec);

  // Returns the counters corresponding to the query with the given query id.
  // Returns nullptr if query doesn't exist in the prepared_stmt_map_.
  std::shared_ptr<StmtCounters> GetWritableStmtCounters(const std::string& query_id);

 private:
  constexpr static int kRpcTimeoutSec = 5;

  // Insert a prepared statement at the front of the LRU list. "prepared_stmts_mutex_" needs to be
  // locked before this call.
  void InsertLruPreparedStatementUnlocked(const std::shared_ptr<CQLStatement>& stmt);

  // Move a prepared statement to the front of the LRU list. "prepared_stmts_mutex_" needs to be
  // locked before this call.
  void MoveLruPreparedStatementUnlocked(const std::shared_ptr<CQLStatement>& stmt);

  // Delete a prepared statement from the cache and the LRU list. "prepared_stmts_mutex_" needs to
  // be locked before this call.
  void DeletePreparedStatementUnlocked(const std::shared_ptr<const CQLStatement> stmt);

  // Delete the least recently used prepared statement from the cache to free up memory.
  void CollectGarbage(size_t required) override;

  // Executes the update counters for both prepared and unprepared statements.
  // "prepared_stmts_mutex_" needs to be locked before this call.
  void UpdateCountersUnlocked(double execute_time_in_msec,
      std::shared_ptr<StmtCounters> stmt_counters);

  // CQLServer of this service.
  CQLServer* const server_;

  // A cache to reduce opening tables or (user-defined) types again and again.
  mutable std::shared_ptr<client::YBMetaDataCache> metadata_cache_;
  mutable std::atomic<bool> is_metadata_initialized_ = { false };
  mutable std::mutex metadata_init_mutex_;

  // List of CQL processors (in-use and available). In-use ones are at the beginning and available
  // ones at the end.
  CQLProcessorList processors_;

  // Next available CQL processor.
  CQLProcessorListPos next_available_processor_;

  // Mutex that protects access to processors_.
  std::mutex processors_mutex_;

  // Prepared statements cache.
  CQLStatementMap prepared_stmts_map_;

  // Prepared statements LRU list (least recently used one at the end).
  CQLStatementList prepared_stmts_list_;

  // Mutex that protects the prepared statements and the LRU list.
  std::mutex prepared_stmts_mutex_;

  std::shared_ptr<ql::Statement> auth_prepared_stmt_;

  // Tracker to measure and limit memory usage of prepared statements.
  MemTrackerPtr prepared_stmts_mem_tracker_;

  MemTrackerPtr processors_mem_tracker_;

  // Tracker to measure the memory usage of CQL requests.
  MemTrackerPtr requests_mem_tracker_;

  // Password and hash cache. Stores each password-hash pair as a compound key;
  // see implementation for rationale.
  boost::compute::detail::lru_cache<std::string, bool> password_cache_
    GUARDED_BY(password_cache_mutex_);
  std::mutex password_cache_mutex_;

  std::shared_ptr<SystemQueryCache> system_cache_;

  // Metrics to be collected and reported.
  yb::rpc::RpcMethodMetrics metrics_;

  std::shared_ptr<CQLMetrics> cql_metrics_;

  ThreadSafeObjectPool<ql::Parser> parser_pool_;

  // Used to requeue the cql_inbound call to handle the response callback(s).
  rpc::Messenger* messenger_ = nullptr;

  int64_t num_allocated_processors_ = 0;
};

}  // namespace cqlserver
}  // namespace yb
