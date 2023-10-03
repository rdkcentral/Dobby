#pragma once

#include "DobbyTemplate.h"
#include <gmock/gmock.h>

class DobbyTemplateMock : public DobbyTemplate{

public:

    virtual ~DobbyTemplateMock() = default;

     MOCK_METHOD(void, _setSettings, (const std::shared_ptr<const IDobbySettings>& settings), ());
     MOCK_METHOD(std::string, _apply, (const ctemplate::TemplateDictionaryInterface* dictionary, bool prettyPrint), (const));
     MOCK_METHOD(bool, _applyAt, (int dirFd, const std::string& fileName, const ctemplate::TemplateDictionaryInterface* dictionary, bool prettyPrint), (const));
};
