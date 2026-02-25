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

#include "PreCompiled.h"

#ifndef _PreComp_
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QInputDialog>
#include <QKeyEvent>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#endif

#include "AIChatPanel.h"
#include <App/Application.h>
#include <Base/Parameter.h>

using namespace Gui;

// ============================================================================
// AIChatBubble Implementation
// ============================================================================

AIChatBubble::AIChatBubble(const AIChatMessage& message, QWidget* parent)
    : QFrame(parent)
    , m_message(message)
{
    setupUi();
    setContent(message.content);
    setLoading(message.isLoading);
}

void AIChatBubble::setupUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Create icon label
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(32, 32);
    m_iconLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // Set icon based on role
    if (m_message.role == AIChatMessage::User) {
        m_iconLabel->setText(QStringLiteral("👤"));
        setStyleSheet(QStringLiteral(
            "AIChatBubble {"
            "  background-color: #e3f2fd;"
            "  border-radius: 8px;"
            "  margin: 4px 8px 4px 48px;"
            "}"
        ));
        layout->addStretch();
        layout->addWidget(m_iconLabel);
    } else if (m_message.role == AIChatMessage::Assistant) {
        m_iconLabel->setText(QStringLiteral("🤖"));
        setStyleSheet(QStringLiteral(
            "AIChatBubble {"
            "  background-color: #f5f5f5;"
            "  border-radius: 8px;"
            "  margin: 4px 48px 4px 8px;"
            "}"
        ));
        layout->addWidget(m_iconLabel);
    } else {  // System
        m_iconLabel->setText(QStringLiteral("⚙"));
        setStyleSheet(QStringLiteral(
            "AIChatBubble {"
            "  background-color: #fff3e0;"
            "  border-radius: 8px;"
            "  margin: 4px 16px;"
            "}"
        ));
        layout->addWidget(m_iconLabel);
    }

    // Create content label
    m_contentLabel = new QLabel(this);
    m_contentLabel->setWordWrap(true);
    m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    m_contentLabel->setOpenExternalLinks(true);
    layout->addWidget(m_contentLabel, 1);

    if (m_message.role == AIChatMessage::User) {
        layout->addStretch();
    }
}

void AIChatBubble::setContent(const QString& content)
{
    m_message.content = content;
    m_contentLabel->setText(formatContent(content));
}

void AIChatBubble::setLoading(bool loading)
{
    m_message.isLoading = loading;
    if (loading) {
        m_contentLabel->setText(tr("Thinking..."));
        setStyleSheet(styleSheet() + QStringLiteral(" color: #888; font-style: italic;"));
    } else {
        setContent(m_message.content);
    }
}

QString AIChatBubble::formatContent(const QString& content) const
{
    QString formatted = content;

    // Basic markdown-like formatting
    // Bold: **text** -> <b>text</b>
    formatted.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")),
                      QStringLiteral("<b>\\1</b>"));

    // Italic: *text* -> <i>text</i>
    formatted.replace(QRegularExpression(QStringLiteral(R"(\*(.+?)\*)")),
                      QStringLiteral("<i>\\1</i>"));

    // Code: `text` -> <code>text</code>
    formatted.replace(QRegularExpression(QStringLiteral(R"(`(.+?)`)")),
                      QStringLiteral("<code style='background:#e8e8e8;padding:2px;'>\\1</code>"));

    // Line breaks
    formatted.replace(QStringLiteral("\n"), QStringLiteral("<br>"));

    return formatted;
}

// ============================================================================
// AIChatPanel Implementation
// ============================================================================

AIChatPanel::AIChatPanel(QWidget* parent)
    : QWidget(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    setupUi();

    // Load settings
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/AI"
    );
    m_apiEndpoint = QString::fromStdString(hGrp->GetASCII("ApiEndpoint", ""));

    // Connect network signals
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &AIChatPanel::onApiReplyFinished);

    // Add welcome message
    AIChatMessage welcome;
    welcome.role = AIChatMessage::System;
    welcome.content = tr("Welcome to FreeCAD AI Assistant!\n\n"
                         "You can ask questions about:\n"
                         "• CAD modeling techniques\n"
                         "• FreeCAD commands and workflows\n"
                         "• Part design and sketching\n"
                         "• And much more!\n\n"
                         "Configure your API settings using the gear button above.");
    welcome.timestamp = QDateTime::currentDateTime();
    addMessageBubble(welcome);
}

AIChatPanel::~AIChatPanel() = default;

void AIChatPanel::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create header widget (always visible)
    m_headerWidget = new QWidget(this);
    m_headerWidget->setObjectName(QStringLiteral("AIChatHeader"));
    m_headerWidget->setStyleSheet(QStringLiteral(
        "#AIChatHeader {"
        "  background-color: #f0f0f0;"
        "  border-bottom: 1px solid #ccc;"
        "}"
    ));

    auto* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(8, 6, 8, 6);

    // Title with icon
    auto* titleLabel = new QLabel(tr("🤖 AI Assistant"), m_headerWidget);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    // Clear button
    m_clearButton = new QPushButton(m_headerWidget);
    m_clearButton->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));
    m_clearButton->setToolTip(tr("Clear conversation"));
    m_clearButton->setFlat(true);
    m_clearButton->setFixedSize(24, 24);
    connect(m_clearButton, &QPushButton::clicked, this, &AIChatPanel::onClearClicked);
    headerLayout->addWidget(m_clearButton);

    // Settings button
    m_settingsButton = new QPushButton(m_headerWidget);
    m_settingsButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_settingsButton->setToolTip(tr("Settings"));
    m_settingsButton->setFlat(true);
    m_settingsButton->setFixedSize(24, 24);
    connect(m_settingsButton, &QPushButton::clicked, this, &AIChatPanel::onSettingsClicked);
    headerLayout->addWidget(m_settingsButton);

    // Collapse/Expand button (with arrow icon)
    m_collapseButton = new QPushButton(m_headerWidget);
    m_collapseButton->setIcon(style()->standardIcon(QStyle::SP_ToolBarVerticalExtension));  // Arrow icon
    m_collapseButton->setToolTip(tr("Collapse/Expand panel"));
    m_collapseButton->setFlat(true);
    m_collapseButton->setFixedSize(24, 24);
    m_collapseButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #e0e0e0;"
        "}"
    ));
    connect(m_collapseButton, &QPushButton::clicked, this, &AIChatPanel::toggleCollapsed);
    headerLayout->addWidget(m_collapseButton);

    m_mainLayout->addWidget(m_headerWidget);

    // Create content widget (collapsible)
    m_contentWidget = new QWidget(this);
    m_contentWidget->setObjectName(QStringLiteral("AIChatContent"));
    auto* contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // Messages scroll area
    m_scrollArea = new QScrollArea(m_contentWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(QStringLiteral("QScrollArea { border: none; background: white; }"));

    m_messagesContainer = new QWidget();
    m_messagesContainer->setStyleSheet(QStringLiteral("background: white;"));
    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(0, 0, 0, 0);
    m_messagesLayout->setSpacing(8);
    m_messagesLayout->addStretch();

    m_scrollArea->setWidget(m_messagesContainer);
    contentLayout->addWidget(m_scrollArea, 1);

    // Input area
    m_inputLayout = new QHBoxLayout();
    m_inputLayout->setContentsMargins(8, 8, 8, 8);

    m_inputField = new QLineEdit(m_contentWidget);
    m_inputField->setPlaceholderText(tr("Type your message..."));
    m_inputField->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  padding: 8px 12px;"
        "  border: 1px solid #ccc;"
        "  border-radius: 4px;"
        "  background: white;"
        "}"
    ));
    connect(m_inputField, &QLineEdit::returnPressed, this, &AIChatPanel::onSendClicked);
    m_inputLayout->addWidget(m_inputField, 1);

    m_sendButton = new QPushButton(m_contentWidget);
    m_sendButton->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    m_sendButton->setToolTip(tr("Send message"));
    m_sendButton->setFixedSize(32, 32);
    connect(m_sendButton, &QPushButton::clicked, this, &AIChatPanel::onSendClicked);
    m_inputLayout->addWidget(m_sendButton);

    contentLayout->addLayout(m_inputLayout);

    m_mainLayout->addWidget(m_contentWidget);

    // Install event filter for the header to handle double-click
    m_headerWidget->installEventFilter(this);

    // Update collapse button icon
    updateCollapseButtonIcon();
}

void AIChatPanel::sendMessage(const QString& message)
{
    if (message.trimmed().isEmpty() || m_isWaitingForResponse) {
        return;
    }

    // Add user message
    AIChatMessage userMsg;
    userMsg.role = AIChatMessage::User;
    userMsg.content = message.trimmed();
    userMsg.timestamp = QDateTime::currentDateTime();
    m_history.append(userMsg);
    addMessageBubble(userMsg);

    // Clear input
    m_inputField->clear();

    // Add loading indicator
    AIChatMessage loadingMsg;
    loadingMsg.role = AIChatMessage::Assistant;
    loadingMsg.content = QString();
    loadingMsg.timestamp = QDateTime::currentDateTime();
    loadingMsg.isLoading = true;

    m_loadingBubble = new AIChatBubble(loadingMsg, this);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, m_loadingBubble);
    scrollToBottom();

    // Set waiting state
    m_isWaitingForResponse = true;
    m_sendButton->setEnabled(false);

    // For now, simulate a response (actual API integration would go here)
    // In a real implementation, this would make an API call
    QTimer::singleShot(1000, this, [this, message]() {
        // Simulate AI response
        QString response;

        if (message.toLower().contains(QStringLiteral("help"))) {
            response = tr("I'm here to help you with FreeCAD!\n\n"
                         "**Common tasks:**\n"
                         "- Create a new document: `File > New`\n"
                         "- Start a sketch: Select a face and click the sketch icon\n"
                         "- Create a pad: Exit sketch and use the pad tool\n\n"
                         "What would you like to know more about?");
        } else if (message.toLower().contains(QStringLiteral("sketch"))) {
            response = tr("**Sketching in FreeCAD:**\n\n"
                         "1. Select a plane or face in the 3D view\n"
                         "2. Click the **Create Sketch** button\n"
                         "3. Use the sketcher tools to draw geometry\n"
                         "4. Add constraints to fully define your sketch\n"
                         "5. Close the sketch when done\n\n"
                         "Key shortcuts:\n"
                         "- `C`: Toggle construction mode\n"
                         "- `X`: Toggle cross-hatching\n"
                         "- `Escape`: Exit current tool");
        } else if (message.toLower().contains(QStringLiteral("pad")) ||
                   message.toLower().contains(QStringLiteral("extrude"))) {
            response = tr("**Creating a Pad (Extrude):**\n\n"
                         "1. First, create a closed sketch\n"
                         "2. Exit the sketcher\n"
                         "3. Select the sketch in the tree view\n"
                         "4. Click the **Pad** tool or press `P`\n"
                         "5. Set the length in the task panel\n"
                         "6. Click OK to create the solid\n\n"
                         "You can also create pads with:\n"
                         "- Symmetric to plane\n"
                         "- Reversed direction\n"
                         "- Taper angle");
        } else {
            response = tr("Thank you for your message! I'm your FreeCAD AI assistant.\n\n"
                         "I can help you with:\n"
                         "- **Sketching** and part design\n"
                         "- **Modeling** techniques and best practices\n"
                         "- **FreeCAD commands** and shortcuts\n"
                         "- **Troubleshooting** common issues\n\n"
                         "What would you like to know?");
        }

        // Remove loading bubble
        removeLoadingBubble();

        // Add response
        AIChatMessage assistantMsg;
        assistantMsg.role = AIChatMessage::Assistant;
        assistantMsg.content = response;
        assistantMsg.timestamp = QDateTime::currentDateTime();
        m_history.append(assistantMsg);
        addMessageBubble(assistantMsg);

        // Reset state
        m_isWaitingForResponse = false;
        m_sendButton->setEnabled(true);

        Q_EMIT responseReceived(response);
    });

    Q_EMIT messageSent(message);
}

void AIChatPanel::clearConversation()
{
    // Clear history
    m_history.clear();

    // Clear UI
    while (m_messagesLayout->count() > 1) {
        QLayoutItem* item = m_messagesLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Add welcome message
    AIChatMessage welcome;
    welcome.role = AIChatMessage::System;
    welcome.content = tr("Conversation cleared. How can I help you?");
    welcome.timestamp = QDateTime::currentDateTime();
    addMessageBubble(welcome);
}

void AIChatPanel::setApiEndpoint(const QString& endpoint)
{
    m_apiEndpoint = endpoint;

    // Save to settings
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/AI"
    );
    hGrp->SetASCII("ApiEndpoint", endpoint.toStdString());
}

void AIChatPanel::addMessageBubble(const AIChatMessage& message)
{
    auto* bubble = new AIChatBubble(message, this);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    scrollToBottom();
}

void AIChatPanel::removeLoadingBubble()
{
    if (m_loadingBubble) {
        m_messagesLayout->removeWidget(m_loadingBubble);
        m_loadingBubble->deleteLater();
        m_loadingBubble = nullptr;
    }
}

void AIChatPanel::scrollToBottom()
{
    QTimer::singleShot(100, this, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum()
        );
    });
}

void AIChatPanel::onApiReplyFinished(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        Q_EMIT errorOccurred(reply->errorString());
        removeLoadingBubble();

        AIChatMessage errorMsg;
        errorMsg.role = AIChatMessage::System;
        errorMsg.content = tr("Error: %1").arg(reply->errorString());
        errorMsg.timestamp = QDateTime::currentDateTime();
        addMessageBubble(errorMsg);

        m_isWaitingForResponse = false;
        m_sendButton->setEnabled(true);
    }

    reply->deleteLater();
}

void AIChatPanel::processApiResponse(const QJsonObject& response)
{
    // Extract response content from API response
    if (response.contains(QStringLiteral("choices"))) {
        QJsonArray choices = response[QStringLiteral("choices")].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices.first().toObject();
            QJsonObject message = firstChoice[QStringLiteral("message")].toObject();
            QString content = message[QStringLiteral("content")].toString();

            AIChatMessage assistantMsg;
            assistantMsg.role = AIChatMessage::Assistant;
            assistantMsg.content = content;
            assistantMsg.timestamp = QDateTime::currentDateTime();
            m_history.append(assistantMsg);
            addMessageBubble(assistantMsg);
        }
    }
}

void AIChatPanel::onSendClicked()
{
    QString message = m_inputField->text();
    if (!message.isEmpty()) {
        sendMessage(message);
    }
}

void AIChatPanel::onClearClicked()
{
    if (QMessageBox::question(this, tr("Clear Conversation"),
                              tr("Are you sure you want to clear the conversation history?"))
        == QMessageBox::Yes) {
        clearConversation();
    }
}

void AIChatPanel::onSettingsClicked()
{
    // Show settings dialog
    bool ok;
    QString newEndpoint = QInputDialog::getText(this, tr("AI Settings"),
                                                 tr("API Endpoint:"),
                                                 QLineEdit::Normal,
                                                 m_apiEndpoint,
                                                 &ok);
    if (ok && !newEndpoint.isEmpty()) {
        setApiEndpoint(newEndpoint);
    }
}

void AIChatPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ShiftModifier) {
            // Allow new line in multi-line input (if we switch to QTextEdit)
        } else {
            onSendClicked();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void AIChatPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_inputField->setFocus();
}

bool AIChatPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Double-click on header to toggle collapse
    if (watched == m_headerWidget && event->type() == QEvent::MouseButtonDblClick) {
        toggleCollapsed();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void AIChatPanel::toggleCollapsed()
{
    setCollapsed(!m_isCollapsed);
}

void AIChatPanel::setCollapsed(bool collapsed)
{
    if (m_isCollapsed == collapsed) {
        return;
    }

    m_isCollapsed = collapsed;

    // Animate the collapse/expand
    QPropertyAnimation* animation = new QPropertyAnimation(m_contentWidget, "maximumHeight");
    animation->setDuration(200);  // 200ms animation

    if (collapsed) {
        // Store current height before collapsing
        m_expandedHeight = m_contentWidget->height();
        animation->setStartValue(m_contentWidget->height());
        animation->setEndValue(0);
    } else {
        animation->setStartValue(0);
        animation->setEndValue(m_expandedHeight > 0 ? m_expandedHeight : 400);
    }

    animation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(animation, &QPropertyAnimation::finished, this, [this]() {
        if (!m_isCollapsed) {
            // Restore normal behavior after expanding
            m_contentWidget->setMaximumHeight(QWIDGETSIZE_MAX);
        }
        updateCollapseButtonIcon();
    });

    animation->start(QAbstractAnimation::DeleteWhenStopped);

    // Update button icon
    updateCollapseButtonIcon();

    Q_EMIT collapseStateChanged(collapsed);
}

void AIChatPanel::updateCollapseButtonIcon()
{
    // Use appropriate arrow icon based on state
    // When expanded: show down arrow (to collapse)
    // When collapsed: show up arrow (to expand)
    if (m_isCollapsed) {
        // Collapsed - show "expand" icon (arrow pointing down/left)
        m_collapseButton->setIcon(style()->standardIcon(QStyle::SP_ToolBarVerticalExtension));
    } else {
        // Expanded - show "collapse" icon (arrow pointing up/right)
        // Rotate the icon using stylesheet or use a different icon
        m_collapseButton->setIcon(style()->standardIcon(QStyle::SP_ToolBarVerticalExtension));
    }
    
    m_collapseButton->setToolTip(m_isCollapsed ? tr("Expand panel") : tr("Collapse panel"));
}

bool AIChatPanel::loadConversation(const QString& filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isArray()) {
        return false;
    }

    clearConversation();

    QJsonArray messages = doc.array();
    for (const QJsonValue& val : messages) {
        QJsonObject obj = val.toObject();
        AIChatMessage msg;
        msg.role = static_cast<AIChatMessage::Role>(obj[QStringLiteral("role")].toInt());
        msg.content = obj[QStringLiteral("content")].toString();
        msg.timestamp = QDateTime::fromString(
            obj[QStringLiteral("timestamp")].toString(), Qt::ISODate
        );
        m_history.append(msg);
        addMessageBubble(msg);
    }

    return true;
}

bool AIChatPanel::saveConversation(const QString& filepath) const
{
    QJsonArray messages;
    for (const AIChatMessage& msg : m_history) {
        QJsonObject obj;
        obj[QStringLiteral("role")] = static_cast<int>(msg.role);
        obj[QStringLiteral("content")] = msg.content;
        obj[QStringLiteral("timestamp")] = msg.timestamp.toString(Qt::ISODate);
        messages.append(obj);
    }

    QJsonDocument doc(messages);

    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(doc.toJson());
    return true;
}

// ============================================================================
// Factory Function
// ============================================================================

QWidget* Gui::createAIChatPanel(QWidget* parent)
{
    return new AIChatPanel(parent);
}

#include "moc_AIChatPanel.cpp"
