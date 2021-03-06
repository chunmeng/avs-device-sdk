/*
 * TestMessageSender.cpp
 *
 * Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "ACL/AVSConnectionManager.h"
#include "Integration/TestMessageSender.h"

using namespace alexaClientSDK;
using namespace acl;
using namespace avsCommon::avs;
using namespace avsCommon::sdkInterfaces;

namespace alexaClientSDK {
namespace integration {
namespace test {

TestMessageSender::TestMessageSender(
        std::shared_ptr<acl::MessageRouterInterface> messageRouter,
        bool isEnabled,
        std::shared_ptr<ConnectionStatusObserverInterface> connectionStatusObserver,
        std::shared_ptr<MessageObserverInterface> messageObserver) {
    m_connectionManager = acl::AVSConnectionManager::create(messageRouter, isEnabled, { connectionStatusObserver },
            { messageObserver });
    // TODO: ACSDK-421: Remove the callback when m_avsConnection manager is no longer an observer to
    // StateSynchronizer.
    m_connectionManager->onStateChanged(StateSynchronizerObserverInterface::State::SYNCHRONIZED);
}

void TestMessageSender::sendMessage(std::shared_ptr<MessageRequest> request) {
    m_connectionManager->sendMessage(request);
    SendParams sendState;
    std::unique_lock<std::mutex> lock(m_mutex);
    sendState.type = SendParams::Type::SEND;
    sendState.request = request;
    m_queue.push_back(sendState);
    m_wakeTrigger.notify_all();
}

TestMessageSender::SendParams TestMessageSender::waitForNext(const std::chrono::seconds duration) {
    SendParams ret;
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_wakeTrigger.wait_for(lock, duration, [this]() { return !m_queue.empty(); })) {
        ret.type = SendParams::Type::TIMEOUT;
        return ret;
    }
    ret = m_queue.front();
    m_queue.pop_front();
    return ret;
}

void TestMessageSender::enable() {
    m_connectionManager->enable();
}

void TestMessageSender::disable() {
    m_connectionManager->disable();
}

bool TestMessageSender::isEnabled() {
    return m_connectionManager->isEnabled();
}

void TestMessageSender::reconnect() {
    m_connectionManager->reconnect();
}

void TestMessageSender::setAVSEndpoint(const std::string& avsEndpoint) {
    m_connectionManager->setAVSEndpoint(avsEndpoint);
}

    void TestMessageSender::addConnectionStatusObserver(
            std::shared_ptr<avsCommon::sdkInterfaces::ConnectionStatusObserverInterface> observer){
        m_connectionManager->addConnectionStatusObserver(observer);
    }

    /**
     * Removes an observer from being notified of connection status changes.
     *
     * @param observer The observer object to remove.
     */
    void TestMessageSender::removeConnectionStatusObserver(
            std::shared_ptr<avsCommon::sdkInterfaces::ConnectionStatusObserverInterface> observer) {
        m_connectionManager->removeConnectionStatusObserver(observer);
    }

    /**
     * Adds an observer to be notified of message receptions.
     *
     * @param observer The observer object to add.
     */
    void TestMessageSender::addMessageObserver(std::shared_ptr<avsCommon::sdkInterfaces::MessageObserverInterface> observer) {
        m_connectionManager->addMessageObserver(observer);
    }

    /**
     * Removes an observer from being notified of message receptions.
     *
     * @param observer The observer object to remove.
     */
    void TestMessageSender::removeMessageObserver(std::shared_ptr<avsCommon::sdkInterfaces::MessageObserverInterface> observer) {
        m_connectionManager->removeMessageObserver(observer);
    }

    void TestMessageSender::synchronize() {
        m_connectionManager->onStateChanged(StateSynchronizerObserverInterface::State::SYNCHRONIZED);
    }

} // namespace test
} // namespace integration
} // namespace alexaClientSDK
