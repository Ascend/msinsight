/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include "CommonRequests.h"
#include "JsonUtil.h"

using namespace Dic;
using namespace Dic::Protocol;

class CommonRequestsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        columns = {TableViewColumn("Name", "name", true, true, true, false),
            TableViewColumn("Age", "age", true, true, true, true),
            TableViewColumn("Score", "score", true, true, false, true),
            TableViewColumn("ID", "id", false, false, false, false)};
    }

    std::vector<TableViewColumn> columns;
};

TEST_F(CommonRequestsTest, PaginationParam_SetFromJson_ShouldParseCurrentPageAndPageSizeCorrectly) {
    PaginationParam pagination;
    document_t json;
    json.Parse(R"({"currentPage": 2, "pageSize": 10})");
    pagination.SetPaginationParamFromJson(json);
    EXPECT_EQ(pagination.currentPage, 2);
    EXPECT_EQ(pagination.pageSize, 10);
}

TEST_F(CommonRequestsTest, PaginationParam_Check_ShouldReturnTrueWhenParametersAreValid) {
    PaginationParam pagination;
    pagination.currentPage = 1;
    pagination.pageSize = 10;

    std::string errorMsg;
    EXPECT_TRUE(pagination.Check(errorMsg));
    EXPECT_TRUE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, PaginationParam_Check_ShouldReturnFalseWhenCurrentPageIsNegative) {
    PaginationParam pagination;
    pagination.currentPage = -1;
    pagination.pageSize = 10;

    std::string errorMsg;
    EXPECT_FALSE(pagination.Check(errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, PaginationParam_Check_ShouldReturnFalseWhenPageSizeIsZero) {
    PaginationParam pagination;
    pagination.currentPage = 1;
    pagination.pageSize = 0;

    std::string errorMsg;
    EXPECT_FALSE(pagination.Check(errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, PaginationParam_Check_ShouldReturnTrueWhenBothParametersAreZero) {
    PaginationParam pagination;
    pagination.currentPage = 0;
    pagination.pageSize = 0;

    std::string errorMsg;
    EXPECT_TRUE(pagination.Check(errorMsg));
}

TEST_F(CommonRequestsTest, FiltersParam_SetFromJson_ShouldParseFiltersCorrectly) {
    FiltersParam filters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"filters": {"name": "test", "age": "20"}})");
    EXPECT_TRUE(filters.SetFiltersFromJson(json, columns, errorMsg));
    EXPECT_EQ(filters.filters.size(), 2);
    EXPECT_EQ(filters.filters["name"], "test");
    EXPECT_EQ(filters.filters["age"], "20");
}

TEST_F(CommonRequestsTest, FiltersParam_SetFromJson_ShouldReturnFalseWhenColumnDoesNotExist) {
    FiltersParam filters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"filters": {"non_exist": "value"}})");
    EXPECT_FALSE(filters.SetFiltersFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, FiltersParam_SetFromJson_ShouldReturnFalseWhenColumnIsNotSearchable) {
    FiltersParam filters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"filters": {"score": "90"}})");
    EXPECT_FALSE(filters.SetFiltersFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, FiltersParam_SetFromJson_ShouldReturnFalseWhenJsonFormatIsInvalid) {
    FiltersParam filters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"filters": 123})");
    EXPECT_FALSE(filters.SetFiltersFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, FiltersParam_SetFromJson_ShouldReturnTrueWhenFiltersAreEmpty) {
    FiltersParam filters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({})");
    EXPECT_TRUE(filters.SetFiltersFromJson(json, columns, errorMsg));
    EXPECT_TRUE(filters.filters.empty());
}

TEST_F(CommonRequestsTest, FiltersParam_SetFromJson_ShouldReturnTrueWhenFiltersFieldIsMissing) {
    FiltersParam filters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"currentPage": 1})");
    EXPECT_TRUE(filters.SetFiltersFromJson(json, columns, errorMsg));
    EXPECT_TRUE(filters.filters.empty());
}

TEST_F(CommonRequestsTest, OrderByParam_SetFromJson_ShouldParseOrderByAscendingCorrectly) {
    OrderByParam orderBy;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"orderBy": "name", "order": "ascend"})");
    EXPECT_TRUE(orderBy.SetOrderFromJson(json, columns, errorMsg));
    EXPECT_EQ(orderBy.orderBy, "name");
    EXPECT_FALSE(orderBy.desc);
}

TEST_F(CommonRequestsTest, OrderByParam_SetFromJson_ShouldParseOrderByDescendingCorrectly) {
    OrderByParam orderBy;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"orderBy": "age", "order": "descend"})");
    EXPECT_TRUE(orderBy.SetOrderFromJson(json, columns, errorMsg));
    EXPECT_EQ(orderBy.orderBy, "age");
    EXPECT_TRUE(orderBy.desc);
}

TEST_F(CommonRequestsTest, OrderByParam_SetFromJson_ShouldParseOrderByUsingDescField) {
    OrderByParam orderBy;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"orderBy": "age", "desc": true})");
    EXPECT_TRUE(orderBy.SetOrderFromJson(json, columns, errorMsg));
    EXPECT_EQ(orderBy.orderBy, "age");
    EXPECT_TRUE(orderBy.desc);
}

TEST_F(CommonRequestsTest, OrderByParam_SetFromJson_ShouldReturnFalseWhenColumnDoesNotExist) {
    OrderByParam orderBy;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"orderBy": "non_exist"})");
    EXPECT_FALSE(orderBy.SetOrderFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, OrderByParam_SetFromJson_ShouldReturnFalseWhenColumnIsNotSortable) {
    OrderByParam orderBy;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"orderBy": "id"})");
    EXPECT_FALSE(orderBy.SetOrderFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, OrderByParam_SetFromJson_ShouldReturnFalseWhenJsonFormatIsInvalid) {
    OrderByParam orderBy;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"orderBy": 123})");
    EXPECT_FALSE(orderBy.SetOrderFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, OrderByParam_SetFromJson_ShouldReturnTrueWhenOrderByIsEmpty) {
    OrderByParam orderBy;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"orderBy": ""})");
    EXPECT_TRUE(orderBy.SetOrderFromJson(json, columns, errorMsg));
    EXPECT_TRUE(orderBy.orderBy.empty());
}

TEST_F(CommonRequestsTest, OrderByParam_SetFromJson_ShouldReturnTrueWhenOrderByFieldIsMissing) {
    OrderByParam orderBy;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({})");
    EXPECT_TRUE(orderBy.SetOrderFromJson(json, columns, errorMsg));
    EXPECT_TRUE(orderBy.orderBy.empty());
}

TEST_F(CommonRequestsTest, RangeFiltersParam_SetFromJson_ShouldParseRangeFiltersCorrectly) {
    RangeFiltersParam rangeFilters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"rangeFilters": {"age": [18, 30], "score": [60, 100]}})");
    EXPECT_TRUE(rangeFilters.SetRangeFiltersFromJson(json, columns, errorMsg));
    EXPECT_EQ(rangeFilters.rangeFilters.size(), 2);
    EXPECT_EQ(rangeFilters.rangeFilters["age"].first, 18);
    EXPECT_EQ(rangeFilters.rangeFilters["age"].second, 30);
    EXPECT_EQ(rangeFilters.rangeFilters["score"].first, 60);
    EXPECT_EQ(rangeFilters.rangeFilters["score"].second, 100);
}

TEST_F(CommonRequestsTest, RangeFiltersParam_SetFromJson_ShouldReturnFalseWhenColumnDoesNotExist) {
    RangeFiltersParam rangeFilters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"rangeFilters": {"non_exist": [1, 10]}})");
    EXPECT_FALSE(rangeFilters.SetRangeFiltersFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, RangeFiltersParam_SetFromJson_ShouldReturnFalseWhenColumnIsNotRangeFilterable) {
    RangeFiltersParam rangeFilters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"rangeFilters": {"name": [1, 10]}})");
    EXPECT_FALSE(rangeFilters.SetRangeFiltersFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, RangeFiltersParam_SetFromJson_ShouldReturnFalseWhenRangeArraySizeIsInvalid) {
    RangeFiltersParam rangeFilters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"rangeFilters": {"age": [18]}})");
    EXPECT_FALSE(rangeFilters.SetRangeFiltersFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, RangeFiltersParam_SetFromJson_ShouldReturnFalseWhenRangeArrayTypeIsInvalid) {
    RangeFiltersParam rangeFilters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"rangeFilters": {"age": ["18", "30"]}})");
    EXPECT_FALSE(rangeFilters.SetRangeFiltersFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, RangeFiltersParam_SetFromJson_ShouldReturnFalseWhenJsonFormatIsInvalid) {
    RangeFiltersParam rangeFilters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"rangeFilters": 123})");
    EXPECT_FALSE(rangeFilters.SetRangeFiltersFromJson(json, columns, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(CommonRequestsTest, RangeFiltersParam_SetFromJson_ShouldReturnTrueWhenRangeFiltersAreEmpty) {
    RangeFiltersParam rangeFilters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({})");
    EXPECT_TRUE(rangeFilters.SetRangeFiltersFromJson(json, columns, errorMsg));
    EXPECT_TRUE(rangeFilters.rangeFilters.empty());
}

TEST_F(CommonRequestsTest, RangeFiltersParam_SetFromJson_ShouldReturnTrueWhenRangeFiltersObjectIsEmpty) {
    RangeFiltersParam rangeFilters;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({"rangeFilters": {}})");
    EXPECT_TRUE(rangeFilters.SetRangeFiltersFromJson(json, columns, errorMsg));
    EXPECT_TRUE(rangeFilters.rangeFilters.empty());
}

TEST_F(CommonRequestsTest, CommonTableParams_SetFromJson_ShouldParseAllParametersCorrectly) {
    CommonTableParams params;
    std::string errorMsg;

    document_t json;
    json.Parse(R"({
        "currentPage": 1,
        "pageSize": 20,
        "filters": {"name": "test"},
        "orderBy": "age",
        "order": "descend",
        "rangeFilters": {"score": [60, 100]}
    })");
    EXPECT_TRUE(params.SetFromJson(json, columns, errorMsg));
    EXPECT_EQ(params.currentPage, 1);
    EXPECT_EQ(params.pageSize, 20);
    EXPECT_EQ(params.filters.size(), 1);
    EXPECT_EQ(params.filters["name"], "test");
    EXPECT_EQ(params.orderBy, "age");
    EXPECT_TRUE(params.desc);
    EXPECT_EQ(params.rangeFilters.size(), 1);
    EXPECT_EQ(params.rangeFilters["score"].first, 60);
    EXPECT_EQ(params.rangeFilters["score"].second, 100);
}

TEST_F(CommonRequestsTest, TableViewColumn_Constructor_ShouldInitializeAllFieldsCorrectly) {
    TableViewColumn column("Test", "test", true, true, true, true);
    EXPECT_EQ(column.name, "Test");
    EXPECT_EQ(column.key, "test");
    EXPECT_TRUE(column.visible);
    EXPECT_TRUE(column.sortable);
    EXPECT_TRUE(column.searchable);
    EXPECT_TRUE(column.rangeFilterable);
}

TEST_F(CommonRequestsTest, TableViewColumn_Constructor_ShouldInitializeHiddenColumnCorrectly) {
    TableViewColumn hiddenColumn("Hidden", "hidden");
    EXPECT_EQ(hiddenColumn.name, "Hidden");
    EXPECT_EQ(hiddenColumn.key, "hidden");
    EXPECT_FALSE(hiddenColumn.visible);
    EXPECT_FALSE(hiddenColumn.sortable);
    EXPECT_FALSE(hiddenColumn.searchable);
    EXPECT_FALSE(hiddenColumn.rangeFilterable);
}

TEST_F(CommonRequestsTest, TableViewColumn_ToTableHeaderJson_ShouldGenerateCorrectJson) {
    TableViewColumn column("Test", "test", true, true, true, true);
    MemoryPoolAllocator<> allocator;
    auto headerJson = column.ToTableHeaderJson(allocator);

    EXPECT_TRUE(headerJson.HasMember("name"));
    EXPECT_TRUE(headerJson.HasMember("key"));
    EXPECT_TRUE(headerJson.HasMember("sortable"));
    EXPECT_TRUE(headerJson.HasMember("searchable"));
    EXPECT_TRUE(headerJson.HasMember("rangeFilterable"));
}

TEST_F(CommonRequestsTest, TableViewColumn_CommonBuildTableHeadersJson_ShouldGenerateCorrectJsonArray) {
    MemoryPoolAllocator<> allocator;
    auto headersJson = TableViewColumn::CommonBuildTableHeadersJson(allocator, columns);

    EXPECT_TRUE(headersJson.IsArray());
    EXPECT_EQ(headersJson.Size(), 3);
}

TEST_F(CommonRequestsTest, FindColumnByKey_ShouldReturnIteratorWhenColumnExists) {
    auto it = FindColumnByKey("name", columns);
    EXPECT_NE(it, columns.end());
    EXPECT_EQ(it->key, "name");
}

TEST_F(CommonRequestsTest, FindColumnByKey_ShouldReturnEndIteratorWhenColumnDoesNotExist) {
    auto it = FindColumnByKey("non_exist", columns);
    EXPECT_EQ(it, columns.end());
}
