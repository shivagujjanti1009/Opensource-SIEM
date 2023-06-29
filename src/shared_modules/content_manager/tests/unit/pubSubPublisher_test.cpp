/*
 * Wazuh content manager - Unit Tests
 * Copyright (C) 2015, Wazuh Inc.
 * Jun 07, 2023.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "pubSubPublisher_test.hpp"
#include "mocks/MockRouterProvider.hpp"
#include "pubSubPublisher.hpp"
#include "updaterContext.hpp"
#include <gmock/gmock.h>
#include <memory>
#include <string>

/*
 * @brief Tests the instantiation of the PubSubPublisher class
 */
TEST_F(PubSubPublisherTest, instantiation)
{
    // Check that the PubSubPublisher class can be instantiated
    EXPECT_NO_THROW(std::make_shared<PubSubPublisher>());
}

/*
 * @brief Tests publish empty data.
 */
TEST_F(PubSubPublisherTest, TestPublishEmptyData)
{
    auto mockRouterProvider {std::make_shared<MockRouterProvider>()};
    EXPECT_CALL(*mockRouterProvider, start).Times(1);
    EXPECT_CALL(*mockRouterProvider, stop).Times(1);

    m_spUpdaterBaseContext->spChannel = mockRouterProvider;
    m_spUpdaterBaseContext->spChannel->start();

    m_spUpdaterContext->spUpdaterBaseContext = m_spUpdaterBaseContext;

    testing::internal::CaptureStdout();

    EXPECT_NO_THROW(m_spPubSubPublisher->handleRequest(m_spUpdaterContext));

    const auto capturedOutput {testing::internal::GetCapturedStdout()};

    EXPECT_EQ(capturedOutput, "PubSubPublisher - No data data to publish\n");

    m_spUpdaterBaseContext->spChannel->stop();

    EXPECT_TRUE(m_spUpdaterContext->data.empty());
}

/*
 * @brief Tests publish valid data.
 */
TEST_F(PubSubPublisherTest, TestPublishValidData)
{
    std::string message {"Hello World!"};

    auto mockRouterProvider {std::make_shared<MockRouterProvider>()};
    EXPECT_CALL(*mockRouterProvider, start).Times(1);
    EXPECT_CALL(*mockRouterProvider, send(::testing::_)).Times(1);
    EXPECT_CALL(*mockRouterProvider, stop).Times(1);

    m_spUpdaterBaseContext->spChannel = mockRouterProvider;
    m_spUpdaterBaseContext->spChannel->start();

    m_spUpdaterContext->spUpdaterBaseContext = m_spUpdaterBaseContext;
    m_spUpdaterContext->data = std::vector<char>(message.begin(), message.end());

    testing::internal::CaptureStdout();

    EXPECT_NO_THROW(m_spPubSubPublisher->handleRequest(m_spUpdaterContext));

    const auto capturedOutput {testing::internal::GetCapturedStdout()};

    EXPECT_EQ(capturedOutput, "PubSubPublisher - Data published\n");

    m_spUpdaterBaseContext->spChannel->stop();

    EXPECT_FALSE(m_spUpdaterContext->data.empty());
}

/*
 * @brief Tests publish valid data without start the MockRouterProvider.
 */
TEST_F(PubSubPublisherTest, TestPublishValidDataWithouStartTheMockRouterProvider)
{
    std::string message {"Hello World!"};

    auto mockRouterProvider {std::make_shared<MockRouterProvider>()};
    EXPECT_CALL(*mockRouterProvider, send(::testing::_)).Times(1).WillOnce([]() { throw std::runtime_error(""); });
    EXPECT_CALL(*mockRouterProvider, stop).Times(1).WillOnce([] { throw std::runtime_error(""); });

    m_spUpdaterBaseContext->spChannel = mockRouterProvider;

    m_spUpdaterContext->spUpdaterBaseContext = m_spUpdaterBaseContext;
    m_spUpdaterContext->data = std::vector<char>(message.begin(), message.end());

    EXPECT_THROW(m_spPubSubPublisher->handleRequest(m_spUpdaterContext), std::runtime_error);

    EXPECT_THROW(m_spUpdaterBaseContext->spChannel->stop(), std::runtime_error);

    EXPECT_FALSE(m_spUpdaterContext->data.empty());
}

/*
 * @brief Tests publish empty data without start the MockRouterProvider.
 */
TEST_F(PubSubPublisherTest, TestPublishEmptyDataWithouStartTheMockRouterProvider)
{
    auto mockRouterProvider {std::make_shared<MockRouterProvider>()};

    EXPECT_CALL(*mockRouterProvider, stop).Times(1).WillOnce([] { throw std::runtime_error(""); });

    m_spUpdaterBaseContext->spChannel = mockRouterProvider;

    m_spUpdaterContext->spUpdaterBaseContext = m_spUpdaterBaseContext;

    testing::internal::CaptureStdout();

    EXPECT_NO_THROW(m_spPubSubPublisher->handleRequest(m_spUpdaterContext));

    const auto capturedOutput {testing::internal::GetCapturedStdout()};

    EXPECT_EQ(capturedOutput, "PubSubPublisher - No data data to publish\n");

    EXPECT_THROW(m_spUpdaterBaseContext->spChannel->stop(), std::runtime_error);

    EXPECT_TRUE(m_spUpdaterContext->data.empty());
}
