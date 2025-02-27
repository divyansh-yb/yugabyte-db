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

#include <boost/algorithm/string.hpp>

#include "yb/integration-tests/cql_test_base.h"
#include "yb/integration-tests/packed_row_test_base.h"

using namespace std::literals;

DECLARE_int32(timestamp_history_retention_interval_sec);

namespace yb {

class CqlPackedRowTest : public PackedRowTestBase<CqlTestBase<MiniCluster>> {
 public:
  virtual ~CqlPackedRowTest() = default;
};

Status CheckTableContent(
    CassandraSession* session, const std::string& expected,
    const std::string& where = "", const std::string& table_name = "t") {
  auto expr = "SELECT * FROM " + table_name;
  if (!where.empty()) {
    expr += " WHERE " + where;
  }
  auto content = VERIFY_RESULT(session->ExecuteAndRenderToString(expr));
  SCHECK_EQ(content, expected, IllegalState,
            Format("Wrong table '$0' content$1",
                   table_name, where.empty() ? "" : Format("(WHERE $0)", where)));
  return Status::OK();
}

TEST_F(CqlPackedRowTest, Simple) {
  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE t (key INT PRIMARY KEY, v1 TEXT, v2 TEXT) WITH tablets = 1"));
  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v1, v2) VALUES (1, 'one', 'two')"));

  ASSERT_OK(CheckTableContent(&session, "1,one,two", "key = 1"));

  ASSERT_OK(session.ExecuteQuery("UPDATE t SET v2 = 'three' where key = 1"));
  ASSERT_OK(CheckTableContent(&session, "1,one,three", "key = 1"));

  ASSERT_OK(session.ExecuteQuery("DELETE FROM t WHERE key = 1"));
  ASSERT_OK(CheckTableContent(&session, ""));

  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v2, v1) VALUES (1, 'four', 'five')"));
  ASSERT_OK(CheckTableContent(&session, "1,five,four", "key = 1"));

  ASSERT_NO_FATALS(CheckNumRecords(cluster_.get(), 4));
}

TEST_F(CqlPackedRowTest, Collections) {
  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE t (key INT PRIMARY KEY, v1 TEXT, v2 LIST<TEXT>, v3 TEXT) WITH tablets = 1"));
  ASSERT_OK(session.ExecuteQuery(
      "INSERT INTO t (key, v1, v2, v3) VALUES (1, 'one', ['two', 'three'], 'four')"));

  auto value = ASSERT_RESULT(session.ExecuteAndRenderToString(
      "SELECT v1, v2, v3 FROM t WHERE key = 1"));
  ASSERT_EQ(value, "one,[two, three],four");

  ASSERT_OK(session.ExecuteQuery("UPDATE t SET v2 = v2 + ['five'] where key = 1"));
  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT v1, v2, v3 FROM t WHERE key = 1"));
  ASSERT_EQ(value, "one,[two, three, five],four");

  ASSERT_OK(session.ExecuteQuery("DELETE FROM t WHERE key = 1"));
  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT * FROM t"));
  ASSERT_EQ(value, "");

  ASSERT_OK(session.ExecuteQuery(
      "INSERT INTO t (key, v3, v2) VALUES (1, 'six', ['seven', 'eight'])"));
  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT v1, v2, v3 FROM t WHERE key = 1"));
  ASSERT_EQ(value, "NULL,[seven, eight],six");

  ASSERT_OK(session.ExecuteQuery("UPDATE t SET v1 = 'nine' where key = 1"));
  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT v1, v2, v3 FROM t WHERE key = 1"));
  ASSERT_EQ(value, "nine,[seven, eight],six");

  ASSERT_OK(cluster_->CompactTablets());

  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT v1, v2, v3 FROM t WHERE key = 1"));
  ASSERT_EQ(value, "nine,[seven, eight],six");

  ASSERT_NO_FATALS(CheckNumRecords(cluster_.get(), 4));
}

TEST_F(CqlPackedRowTest, TTL) {
  constexpr auto kSmallTtl = 1 * kTimeMultiplier;
  constexpr auto kBigTtl = kSmallTtl * 2;
  constexpr auto kTimeout = 1s * kSmallTtl + 50ms * kTimeMultiplier;

  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE t (key INT PRIMARY KEY, value TEXT) WITH tablets = 1"));
  ASSERT_OK(session.ExecuteQueryFormat(
      "INSERT INTO t (key, value) VALUES (1, 'one') USING TTL $0", kSmallTtl));

  std::this_thread::sleep_for(kTimeout);

  auto value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT * FROM t"));
  ASSERT_EQ(value, "");

  ASSERT_OK(session.ExecuteQueryFormat(
      "INSERT INTO t (key, value) VALUES (2, 'two') USING TTL $0", kBigTtl));

  ASSERT_OK(session.ExecuteQueryFormat(
      "UPDATE t USING TTL $0 SET value = 'dva' WHERE key = 2", kSmallTtl));

  auto deadline = std::chrono::steady_clock::now() + kTimeout;

  ASSERT_OK(cluster_->CompactTablets());

  std::this_thread::sleep_until(deadline);

  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT * FROM t"));
  ASSERT_EQ(value, "2,NULL");

  std::this_thread::sleep_for(kTimeout);

  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT * FROM t"));
  ASSERT_EQ(value, "");

  ASSERT_OK(session.ExecuteQueryFormat(
      "INSERT INTO t (key, value) VALUES (3, 'three') USING TTL $0", kSmallTtl));

  ASSERT_OK(session.ExecuteQueryFormat(
      "UPDATE t USING TTL $0 SET value = 'tri' WHERE key = 3", kBigTtl));

  std::this_thread::sleep_for(kTimeout);

  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT * FROM t"));
  ASSERT_EQ(value, "3,tri");
}

TEST_F(CqlPackedRowTest, WriteTime) {
  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE t (key INT PRIMARY KEY, v1 TEXT, v2 TEXT) WITH tablets = 1"));
  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v1, v2) VALUES (1, 'one', 'odin')"));

  int64_t v1time, v2time;
  auto processor = [&v1time, &v2time](const CassandraRow& row) {
    row.Get(0, &v1time);
    row.Get(1, &v2time);
  };

  ASSERT_OK(session.ExecuteAndProcessOneRow(
      "SELECT writetime(v1), writetime(v2) FROM t", processor));
  ASSERT_EQ(v1time, v2time);

  ASSERT_OK(session.ExecuteQueryFormat("UPDATE t SET v2 = 'dva' WHERE key = 1"));

  ASSERT_OK(session.ExecuteAndProcessOneRow(
      "SELECT writetime(v1), writetime(v2) FROM t", processor));
  ASSERT_LT(v1time, v2time);
  auto old_v1time = v1time;
  auto old_v2time = v2time;

  ASSERT_OK(cluster_->CompactTablets());

  ASSERT_OK(session.ExecuteAndProcessOneRow(
      "SELECT writetime(v1), writetime(v2) FROM t", processor));
  ASSERT_EQ(old_v1time, v1time);
  ASSERT_EQ(old_v2time, v2time);
}

TEST_F(CqlPackedRowTest, RetainPacking) {
  // Set retention interval to 0, to repack all recently flushed entries.
  FLAGS_timestamp_history_retention_interval_sec = 0;

  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE t (key INT PRIMARY KEY, v1 INT) WITH tablets = 1"));

  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v1) VALUES (1, 1)"));
  ASSERT_OK(cluster_->FlushTablets());

  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v1) VALUES (2, 2)"));
  ASSERT_OK(cluster_->FlushTablets());

  ASSERT_OK(session.ExecuteQuery("ALTER TABLE t ADD v2 INT"));
  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v1, v2) VALUES (3, 3, 3)"));

  // The first compaction to compact all flushed entries with an old schema, so all disk entries
  // have only the most recent schema version.
  ASSERT_OK(cluster_->CompactTablets(docdb::SkipFlush::kTrue));

  // Compaction with flush to check that we did not lost used schema versions.
  ASSERT_OK(cluster_->CompactTablets());
}

TEST_F(CqlPackedRowTest, NonFullInsert) {
  FLAGS_timestamp_history_retention_interval_sec = 0;

  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE t (key INT PRIMARY KEY, v1 INT, v2 INT) WITH tablets = 1"));

  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v1, v2) VALUES (1, 1, 1)"));
  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v2) VALUES (1, 2)"));

  auto value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT * FROM t"));
  ASSERT_EQ(value, "1,1,2");
}

TEST_F(CqlPackedRowTest, LivenessColumnExpiry) {
  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE t (key INT PRIMARY KEY, v1 INT) WITH tablets = 1"));

  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v1) VALUES (1, 1) USING TTL 1"));
  ASSERT_OK(session.ExecuteQuery("UPDATE t SET v1 = 2 WHERE key = 1"));

  std::this_thread::sleep_for(1s);

  auto value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT * FROM t"));
  ASSERT_EQ(value, "1,2");

  ASSERT_OK(session.ExecuteQuery("UPDATE t SET v1 = NULL WHERE key = 1"));

  value = ASSERT_RESULT(session.ExecuteAndRenderToString("SELECT * FROM t"));
  ASSERT_EQ(value, "");
}

TEST_F(CqlPackedRowTest, BadCast) {
  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE test_cast (h int PRIMARY KEY, t text) WITH tablets = 1"));

  ASSERT_NOK(session.ExecuteQuery("INSERT INTO test_cast (h, t) values (2, cast(22 as text))"));
}

TEST_F(CqlPackedRowTest, TimestampOverExpired) {
  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery("CREATE TABLE t (h INT, r INT, v INT, PRIMARY KEY((h), r))"));

  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (h, r, v) values (1, 2, 3) USING TTL 1"));
  auto write_time = ASSERT_RESULT(session.FetchValue<int64_t>(
      "SELECT writetime(v) FROM t WHERE h = 1 AND r = 2"));
  ASSERT_OK(CheckTableContent(&session, "1,2,3", "h = 1 AND r = 2"));

  std::this_thread::sleep_for(1s);
  ASSERT_OK(CheckTableContent(&session, ""));

  ASSERT_OK(session.ExecuteQuery(
      "INSERT INTO t (h, r, v) values (1, 2, 4) USING TIMESTAMP " +
      std::to_string(write_time - 1)));
  ASSERT_OK(CheckTableContent(&session, ""));
}

TEST_F(CqlPackedRowTest, CompactUpdateToNull) {
  FLAGS_timestamp_history_retention_interval_sec = 0;

  auto session = ASSERT_RESULT(EstablishSession(driver_.get()));

  ASSERT_OK(session.ExecuteQuery(
      "CREATE TABLE t (key INT PRIMARY KEY, v1 INT) WITH tablets = 1"));

  ASSERT_OK(session.ExecuteQuery("INSERT INTO t (key, v1) VALUES (1, 1)"));
  ASSERT_OK(session.ExecuteQuery("UPDATE t SET v1 = NULL WHERE key = 1"));

  ASSERT_OK(cluster_->CompactTablets());

  auto lines = DumpDocDBToStrings(cluster_.get(), ListPeersFilter::kLeaders);
  ASSERT_EQ(lines.size(), 1);
  ASSERT_TRUE(boost::algorithm::ends_with(
      lines[0], Format("{ $0: NULL }\n", kFirstColumnId + 1))) << lines[0];
}

} // namespace yb
