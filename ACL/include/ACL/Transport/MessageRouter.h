/*
 * MessageRouter.h
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

#ifndef ALEXA_CLIENT_SDK_ACL_INCLUDE_ACL_TRANSPORT_MESSAGE_ROUTER_H_
#define ALEXA_CLIENT_SDK_ACL_INCLUDE_ACL_TRANSPORT_MESSAGE_ROUTER_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "AVSCommon/Utils/Threading/Executor.h"

#include <AVSCommon/AVS/Attachment/AttachmentManager.h>
#include <AVSCommon/AVS/MessageRequest.h>

#include "AVSCommon/SDKInterfaces/AuthDelegateInterface.h"
#include "ACL/Transport/MessageRouterInterface.h"
#include "ACL/Transport/MessageRouterObserverInterface.h"
#include "ACL/Transport/TransportInterface.h"
#include "ACL/Transport/TransportObserverInterface.h"
#include "ACL/Transport/MessageConsumerInterface.h"

namespace alexaClientSDK {
namespace acl {

/**
 * This an abstract base class which specifies the interface to manage an actual connection over some medium to AVS.
 *
 * Implementations of this class are required to be thread-safe.
 */
class MessageRouter: public MessageRouterInterface, public TransportObserverInterface, public MessageConsumerInterface {

public:
    /**
     * Constructor.
     * @param authDelegate An implementation of an AuthDelegate, which will provide valid access tokens with which
     * the MessageRouter can authorize the client to AVS.
     * @param attachmentManager The AttachmentManager, which allows ACL to write attachments received from AVS.
     * @param avsEndpoint The endpoint to connect to AVS.
     */
    MessageRouter(
            std::shared_ptr<avsCommon::sdkInterfaces::AuthDelegateInterface> authDelegate,
            std::shared_ptr<avsCommon::avs::attachment::AttachmentManager> attachmentManager,
            const std::string& avsEndpoint);

    /**
     * Destructor.
     */
    virtual ~MessageRouter();

    void enable() override;

    void disable() override;

    ConnectionStatus getConnectionStatus() override;

    // TODO: ACSDK-421: Revert this to use send().
    void sendMessage(std::shared_ptr<avsCommon::avs::MessageRequest> request) override;

    void setAVSEndpoint(const std::string& avsEndpoint) override;

    void setObserver(std::shared_ptr<MessageRouterObserverInterface> observer) override;

    void onConnected() override;

    void onDisconnected(avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::ChangedReason reason) override;

    void onServerSideDisconnect() override;

    void consumeMessage(const std::string & contextId, const std::string & message) override;

private:
    /**
     * Creates a new MessageRouter.
     *
     * @param authDelegate The AuthDelegateInterface to use for authentication and authorization with AVS.
     * @param avsEndpoint The URL for the AVS server we will connect to.
     * @param messageConsumerInterface The object which should be notified on messages which arrive from AVS.
     * @param transportObserverInterface A pointer to the transport observer the new transport should notify.
     * @return A new MessageRouter object.
     *
     * TODO: ACSDK-99 Replace this with an injected transport factory.
     */
    virtual std::shared_ptr<TransportInterface> createTransport(
            std::shared_ptr<avsCommon::sdkInterfaces::AuthDelegateInterface> authDelegate,
            std::shared_ptr<avsCommon::avs::attachment::AttachmentManager> attachmentManager,
            const std::string& avsEndpoint,
            MessageConsumerInterface* messageConsumerInterface,
            TransportObserverInterface* transportObserverInterface) = 0;

    /**
     * Set the connection state. If it changes, notify our observer.
     * @c m_connectionMutex must be locked to call this method.
     *
     * @param status The current status of the connection.
     * @param reason The reason the connection status changed.
     */
    void setConnectionStatusLocked(
            const avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status status, 
            const avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::ChangedReason reason);

    /**
     * Notify the connection observer when the status has changed.
     * Architectural note:
     *  @li A derived class cannot access the required observer method directly due a friend relationship at the base
     *    class level. However this method bridges the gap, and allows the observer's public interface to remain
     *    unchanged.
     *
     * @param status The current status of the connection.
     * @param reason The reason the connection status changed.
     */
    void notifyObserverOnConnectionStatusChanged(
            const avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status status,
            const avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::ChangedReason reason);

    /**
     * Notify the message observer of an incoming message from AVS.
     * Architectural note:
     *  @li A derived class cannot access the required observer method directly due a friend relationship at the base
     *    class level. However this method bridges the gap, and allows the observer's public interface to remain
     *    unchanged.
     *
     * @param contextId The context id for the current message.
     * @param message The AVS message in string representation.
     */
    void notifyObserverOnReceive(const std::string & contextId, const std::string & message);

    /**
     * Creates a new transport, and begins the connection process. The new transport immediately becomes the active
     * transport. @c m_connectionMutex must be locked to call this method.
     */
    void createActiveTransportLocked();

    /**
     * Disconnects all transports. @c m_connectionMutex must be locked to call this method.
     *
     * @param reason The reason the last transport was disconnected
     * @param lock Reference to the @c unique_lock that must be held when this method is called. The lock may be
     * released during the execution of this method, but will be locked when this method exits.
     */
    void disconnectAllTransportsLocked(
            std::unique_lock<std::mutex>& lock, 
            const avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::ChangedReason reason);

    /**
     * Get the observer.
     *
     * @return The observer.
     */
    std::shared_ptr<MessageRouterObserverInterface> getObserver();

    /// The observer object. Access serialized with @c m_connectionMutex.
    std::shared_ptr<MessageRouterObserverInterface> m_observer;

    /// The current AVS endpoint. Access serialized with @c m_connectionMutex.
    std::string m_avsEndpoint;

    /// The AuthDelegateInterface which provides a valid access token.
    std::shared_ptr<avsCommon::sdkInterfaces::AuthDelegateInterface> m_authDelegate;

    /// This mutex guards access to all connection related state, specifically the status and all transport interaction.
    std::mutex m_connectionMutex;

    /// The current connection status. Access serialized with @c m_connectionMutex.
    avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status m_connectionStatus;

    /// The current connection reason. Access serialized with @c m_connectionMutex.
    avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::ChangedReason m_connectionReason;

    /**
     * When the MessageRouter is enabled, any disconnect should automatically trigger a reconnect with AVS.
     * Access serialized with @c m_connectionMutex.
     */
    bool m_isEnabled;

    /// A vector of all transports which are not disconnected. Access serialized with @c m_connectionMutex.
    std::vector<std::shared_ptr<TransportInterface>> m_transports;

    /// The current active transport to send messages on. Access serialized with @c m_connectionMutex.
    std::shared_ptr<TransportInterface> m_activeTransport;

    /// The attachment manager.
    std::shared_ptr<avsCommon::avs::attachment::AttachmentManager> m_attachmentManager;

protected:
    /**
     * Executor to perform asynchronous operations:
     * @li Delivery of connection status notifications.
     * @li completion of send operations delayed by a pending connection state.
     */
    avsCommon::utils::threading::Executor m_executor;
};

} // namespace acl
} // namespace alexaClientSDK

#endif // ALEXA_CLIENT_SDK_ACL_INCLUDE_ACL_TRANSPORT_MESSAGE_ROUTER_H_
