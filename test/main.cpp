#include <gtest/gtest.h>
#include <stdio.h>
#include <gtest/gtest.h>

TEST(FooTest, HandleNoneZeroInput)
{
    EXPECT_EQ(2, 2);
    EXPECT_EQ(6, 4);
}
int main(){
    printf("hello epic test\n");
}