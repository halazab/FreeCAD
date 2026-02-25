/***************************************************************************
 *   Copyright (c) 2024 FreeCAD AI Integration Authors                     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <memory>

namespace Gui
{

/**
 * @brief Represents a single chat message in the AI conversation
 */
struct AIChatMessage
{
    enum Role {
        User,
        Assistant,
        System
    };

    Role role;
    QString content;
    QDateTime timestamp;
    bool isLoading = false;
};

/**
 * @brief A widget displaying a single chat message bubble
 */
class AIChatBubble : public QFrame
{
    Q_OBJECT

public:
    explicit AIChatBubble(const AIChatMessage& message, QWidget* parent = nullptr);
    ~AIChatBubble() override = default;

    void setContent(const QString& content);
    void setLoading(bool loading);

private:
    void setupUi();
    QString formatContent(const QString& content) const;

    QLabel* m_iconLabel = nullptr;
    QLabel* m_contentLabel = nullptr;
    AIChatMessage m_message;
};

/**
 * @brief Main AI Chat Panel widget for FreeCAD AI integration
 *
 * This panel provides a chat interface for interacting with AI assistants
 * within FreeCAD. It supports:
 * - Chat conversation with AI models
 * - Context-aware assistance for CAD operations
 * - Message history
 * - Markdown-like formatting
 */
class AIChatPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AIChatPanel(QWidget* parent = nullptr);
    ~AIChatPanel() override;

    /// Send a message to the AI
    void sendMessage(const QString& message);

    /// Clear the conversation history
    void clearConversation();

    /// Set the API endpoint for AI services
    void setApiEndpoint(const QString& endpoint);

    /// Get the conversation history
    const QList<AIChatMessage>& getHistory() const { return m_history; }

    /// Load conversation from file
    bool loadConversation(const QString& filepath);

    /// Save conversation to file
    bool saveConversation(const QString& filepath) const;

public Q_SLOTS:
    void onSendClicked();
    void onClearClicked();
    void onSettingsClicked();
    void toggleCollapsed();
    void setCollapsed(bool collapsed);

Q_SIGNALS:
    /// Emitted when a message is sent
    void messageSent(const QString& message);

    /// Emitted when a response is received
    void responseReceived(const QString& response);

    /// Emitted when an error occurs
    void errorOccurred(const QString& error);

    /// Emitted when collapse state changes
    void collapseStateChanged(bool collapsed);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:
    void onApiReplyFinished(QNetworkReply* reply);

private:
    void setupUi();
    void addMessageBubble(const AIChatMessage& message);
    void updateLoadingBubble(const QString& content);
    void removeLoadingBubble();
    void scrollToBottom();
    void processApiResponse(const QJsonObject& response);
    void updateCollapseButtonIcon();

    // UI Components
    QVBoxLayout* m_mainLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_messagesContainer = nullptr;
    QVBoxLayout* m_messagesLayout = nullptr;
    QHBoxLayout* m_inputLayout = nullptr;
    QLineEdit* m_inputField = nullptr;
    QPushButton* m_sendButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QPushButton* m_collapseButton = nullptr;
    AIChatBubble* m_loadingBubble = nullptr;

    // Collapsible content
    QWidget* m_contentWidget = nullptr;
    QWidget* m_headerWidget = nullptr;

    // State
    QList<AIChatMessage> m_history;
    QString m_apiEndpoint;
    QNetworkAccessManager* m_networkManager = nullptr;
    bool m_isWaitingForResponse = false;
    bool m_isCollapsed = false;
    int m_expandedHeight = 400;
};

/**
 * @brief Factory function to create the AI Chat dock widget
 */
QWidget* createAIChatPanel(QWidget* parent = nullptr);

}  // namespace Gui
