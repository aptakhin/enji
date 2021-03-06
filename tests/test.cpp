#include <enji/http.h>
#include <gtest/gtest.h>

TEST(common, path_join) {
    ASSERT_EQ("a/b/c", enji::path_join("a", "b", "c"));
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    return result;
}