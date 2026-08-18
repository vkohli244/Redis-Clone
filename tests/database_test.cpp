#include "database.h"
#include "test_support.h"

#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;

TEST(database_sets_gets_overwrites_and_deletes_values) {
  Database database;
  REQUIRE(!database.get("missing").has_value());
  REQUIRE_EQ(database.del("missing"), 0U);

  database.set("key", "first");
  REQUIRE_EQ(database.get("key"), std::optional<std::string>("first"));
  database.set("key", "second");
  REQUIRE_EQ(database.get("key"), std::optional<std::string>("second"));
  REQUIRE_EQ(database.del("key"), 1U);
  REQUIRE(!database.get("key").has_value());
  REQUIRE_EQ(database.del("key"), 0U);
}

TEST(database_expiry_is_lazy_and_expired_keys_are_not_deleted_twice) {
  Database database;
  database.set("read-expiry", "value", 1ms);
  database.set("delete-expiry", "value", 1ms);
  std::this_thread::sleep_for(5ms);

  REQUIRE(!database.get("read-expiry").has_value());
  REQUIRE_EQ(database.del("read-expiry"), 0U);
  REQUIRE_EQ(database.del("delete-expiry"), 0U);
  REQUIRE_EQ(database.del("delete-expiry"), 0U);
}

TEST(database_unexpired_ttl_value_can_be_replaced_by_persistent_value) {
  Database database;
  database.set("key", "temporary", 1h);
  REQUIRE_EQ(database.get("key"), std::optional<std::string>("temporary"));
  database.set("key", "persistent");
  REQUIRE_EQ(database.get("key"), std::optional<std::string>("persistent"));
}
