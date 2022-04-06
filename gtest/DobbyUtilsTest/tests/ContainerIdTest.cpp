#include <iostream>
#include <gtest/gtest.h>
#include "ContainerId.h"
using namespace std;

class ContainerIdTest : public ::testing::Test {
         protected:
                 void SetUp() override {
                        cout<<"constructor"<<endl;
                 }
                 void TearDown() override {
                        cout<<"destructor"<<endl;
                 }
	ContainerId tid,rid;
};

TEST_F(ContainerIdTest,CheckNumeric){
        rid = tid.create("123");
        EXPECT_EQ(rid.str(),"");
}

TEST_F(ContainerIdTest,CheckDoubleDot){
        rid = tid.create("a..123");
        EXPECT_EQ(rid.str(),"");
}

TEST_F(ContainerIdTest,CheckAlphanumeric){
        rid = tid.create("a.123");
        EXPECT_EQ(rid.str(),"a.123");
}

