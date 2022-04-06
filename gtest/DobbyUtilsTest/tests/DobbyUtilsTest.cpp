#include<gtest/gtest.h>
#include "DobbyTimer.h"
#include "ContainerId.h"
#include "Logging.h"
#include <fstream>
#include <fcntl.h>
#include <dirent.h>
#define private public
#include "DobbyUtils.h"

using namespace std;

class DobbyUtilsTest : public ::testing::Test {
         protected:
                 void SetUp() override {
                        cout<<"constructor"<<endl;
                 }
                 void TearDown() override {
                        cout<<"destructor"<<endl;
                 }
	DobbyUtils test;
};

TEST_F(DobbyUtilsTest, TestRecursiveMkdirAbsolutePath)
{
        const string path = "/tmp/hello/some/long/path";

        //printf("temp path = '%s'\n", path.c_str());

        EXPECT_TRUE(test.mkdirRecursive(path, 0700));
}

TEST_F(DobbyUtilsTest, TestRmdirContentsAbsolutePath)
{
        EXPECT_TRUE(test.rmdirContents("/tmp/hello"));
}

TEST_F(DobbyUtilsTest, TestRmdirRecursiveAbsolutePath)
{
        EXPECT_TRUE(test.rmdirRecursive("/tmp/hello"));
}

TEST_F(DobbyUtilsTest, TestCleanMountLostAndFound)
{
        std::string tmp = "/lost+found/some/long/path/file.xyz";

        test.mkdirRecursive(tmp, 0700);

        test.cleanMountLostAndFound("/home", std::string("0"));
}

TEST_F(DobbyUtilsTest, TestAttachFileToLoopDevice)
{
         std::string loopDevPath;

         int loopDevFd = test.openLoopDevice(&loopDevPath);
	 //printf("%d\n",loopDevFd);

         if (loopDevFd < 0)
         {
             printf("failed to open loop device\n");
             EXPECT_FALSE(true);
         }
         else
         {
             printf("Opened loop mount =%s\n", loopDevPath.c_str());
         }

         int fileFd = open("/tmp/test1", O_CREAT | O_RDWR);

         EXPECT_TRUE(test.attachFileToLoopDevice(loopDevFd,fileFd));

         if (close(loopDevFd) != 0)
         {
             printf("failed to close file\n");
             EXPECT_FALSE(true);
         }
        test.rmdirRecursive("/tmp/test1");
}

TEST_F(DobbyUtilsTest, TestwriteTextFile){
        test.writeTextFile("/tmp/hi","Hello World",O_CREAT,0644);
}

TEST_F(DobbyUtilsTest, TestreadTextFile){
        EXPECT_EQ(test.readTextFile("/tmp/hi",4096),"Hello World");
        test.rmdirRecursive("/tmp/hi");
}

ContainerId t_id;

TEST_F(DobbyUtilsTest, TestsetStringMetaData){
        t_id.create("a123");
        test.setStringMetaData(t_id,"ipaddr","127.0.0.1");
}

TEST_F(DobbyUtilsTest, TestgetStringMetaData){
        EXPECT_EQ(test.getStringMetaData(t_id,"ipaddr",""),"127.0.0.1");
}

TEST_F(DobbyUtilsTest, TestsetIntegerMetaData){
        test.setIntegerMetaData(t_id,"port",9998);
}

TEST_F(DobbyUtilsTest, TestgetIntegerMetaData){
	EXPECT_EQ(test.getIntegerMetaData(t_id,"port",0),9998);
}

TEST_F(DobbyUtilsTest, TestclearContainerMetaData){
        test.clearContainerMetaData(t_id);
	EXPECT_EQ(test.getStringMetaData(t_id,"ipaddr",""),"");
        EXPECT_EQ(test.getStringMetaData(t_id,"port",""),"");
}
