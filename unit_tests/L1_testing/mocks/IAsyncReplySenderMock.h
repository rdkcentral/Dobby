#include "IpcCommon.h"
#include <gmock/gmock.h>

namespace AI_IPC
{
   class IAsyncReplySenderMock : public IAsyncReplySenderApiImpl {

   public:
       virtual ~IAsyncReplySenderMock() = default;
       MOCK_METHOD(bool, sendReply, (const VariantList& replyArgs), (override));

};

}
