#include <catch2/catch.hpp>
#include "wt_internal.h"

TEST_CASE("setup and teardown", "[bt_alloc]")
{
    SECTION("ctor") 
    {
        int ret;
        bt_allocator allocator;
        ret = bt_alloc_ctor(&allocator);
        printf("ret=%d  msg=%s\n", ret, strerror(ret));
        REQUIRE(ret == 0);
    }

    SECTION("create") 
    {
        int ret;
        bt_allocator *allocator;
        ret = bt_alloc_create(&allocator, BT_ALLOC_REGION_SIZE, BT_ALLOC_REGION_COUNT);
        REQUIRE(ret == 0);
        REQUIRE(allocator != NULL);
    }

    SECTION("ctor and dtor") 
    {
        int ret;
        bt_allocator allocator;
        ret = bt_alloc_ctor(&allocator);
        REQUIRE(ret == 0);
        ret = bt_alloc_dtor(&allocator);
        REQUIRE(ret == 0);
    }

    SECTION("create and destroy") 
    {
        int ret;
        bt_allocator *allocator;
        ret = bt_alloc_create(&allocator, BT_ALLOC_REGION_SIZE, BT_ALLOC_REGION_COUNT);
        REQUIRE(ret == 0);
        REQUIRE(allocator != NULL);

        ret = bt_alloc_destroy(&allocator);
        REQUIRE(ret == 0);
        REQUIRE(allocator == NULL);
    }
}

TEST_CASE("bt_alloc_allocator", "[bt_alloc]")
{

    SECTION("one_page_alloc") 
    {
        int ret;
        bt_allocator allocator;
        WT_PAGE *pagep;

        ret = bt_alloc_ctor(&allocator);
        REQUIRE(ret == 0);

        ret = bt_alloc_page_alloc(&allocator, 400, &pagep);
        REQUIRE(ret == 0);
        REQUIRE(pagep != NULL);

        ret = bt_alloc_page_free(&allocator, pagep);
        REQUIRE(ret == 0);
        
        ret = bt_alloc_dtor(&allocator);
        REQUIRE(ret == 0);
    }

    SECTION("two_page_alloc")
    {
        int ret;
        bt_allocator allocator;
        WT_PAGE *pagep1, *pagep2;

        ret = bt_alloc_ctor(&allocator);
        REQUIRE(ret == 0);

        ret = bt_alloc_page_alloc(&allocator, 400, &pagep1);
        REQUIRE(ret == 0);
        REQUIRE(pagep1 != NULL);

        ret = bt_alloc_page_alloc(&allocator, 100000, &pagep2);
        REQUIRE(ret == 0);
        REQUIRE(pagep2 != NULL);

        REQUIRE(pagep1 != pagep2);

        ret = bt_alloc_page_free(&allocator, pagep1);
        REQUIRE(ret == 0);
        
        ret = bt_alloc_page_free(&allocator, pagep2);
        REQUIRE(ret == 0);

        ret = bt_alloc_dtor(&allocator);
        REQUIRE(ret == 0);
    }

    SECTION("giant_alloc")
    {
        int ret;
        bt_allocator allocator;
        WT_PAGE *pagep;
        void *memptr;

        ret = bt_alloc_ctor(&allocator);
        REQUIRE(ret == 0);

        ret = bt_alloc_page_alloc(&allocator, 128 * 1024, &pagep);
        REQUIRE(ret == 0);
        REQUIRE(pagep != NULL);

        ret = bt_alloc_zalloc(&allocator, 2 * BT_ALLOC_REGION_SIZE, pagep, &memptr);
        REQUIRE(ret == 0);
        REQUIRE(memptr != NULL);

        ret = bt_alloc_page_free(&allocator, pagep);
        REQUIRE(ret == 0);
        
        ret = bt_alloc_dtor(&allocator);
        REQUIRE(ret == 0);
    }

    SECTION("zero_alloc")
    {
        int ret;
        bt_allocator allocator;
        WT_PAGE *pagep;
        void *memptr;

        ret = bt_alloc_ctor(&allocator);
        REQUIRE(ret == 0);

        ret = bt_alloc_page_alloc(&allocator, 200 * 1024, &pagep);
        REQUIRE(ret == 0);
        REQUIRE(pagep != NULL);

        ret = bt_alloc_zalloc(&allocator, 0, pagep, &memptr);
        REQUIRE(ret == 0);
        REQUIRE(memptr == NULL);

        ret = bt_alloc_page_free(&allocator, pagep);
        REQUIRE(ret == 0);
        REQUIRE(pagep != NULL);

        ret = bt_alloc_dtor(&allocator);
        REQUIRE(ret == 0);
    }
}

TEST_CASE("spill allocation", "[bt_alloc]")
{
    int ret;
    bt_allocator allocator;
    WT_PAGE *pagep;

    ret = bt_alloc_ctor(&allocator);
    REQUIRE(ret == 0);

    ret = bt_alloc_page_alloc(&allocator, BT_ALLOC_MIB(30), &pagep);
    REQUIRE(ret == 0);
    REQUIRE(pagep != NULL);

    SECTION("Immediately spill into new region.")
    {
        void *memptr;        
        ret = bt_alloc_zalloc(&allocator, BT_ALLOC_MIB(50), pagep, &memptr);
        REQUIRE(ret == 0);
        REQUIRE(memptr != NULL);
        REQUIRE(allocator.region_count == 2);
    }

    ret = bt_alloc_page_free(&allocator, pagep);
    REQUIRE(ret == 0);

    ret = bt_alloc_dtor(&allocator);
    REQUIRE(ret == 0);
}

TEST_CASE("basic allocation with dynamic configuration", "[bt_alloc]")
{
    int ret;
    bt_allocator *allocator;

    ret = bt_alloc_create(&allocator, 4096, 128);
    REQUIRE(ret == 0);
    REQUIRE(allocator != NULL);

    SECTION("one_page_alloc") 
    {
        WT_PAGE *page;

        ret = bt_alloc_page_alloc(allocator, 1000, &page);
        REQUIRE(ret == 0);
        REQUIRE(page != NULL);
        REQUIRE(allocator->region_count == 1);
        REQUIRE(allocator->region_map[0] == 0xfe);

        ret = bt_alloc_page_free(allocator, page);
        REQUIRE(ret == 0);
        REQUIRE(allocator->region_count == 0);
        REQUIRE(allocator->region_map[0] == 0xff);
    }
   
    ret = bt_alloc_destroy(&allocator);
    REQUIRE(ret == 0);
    REQUIRE(allocator == NULL);
}