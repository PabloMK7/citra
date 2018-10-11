// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <future>
#include <QColor>
#include <QImage>
#include <QList>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QTime>
#include <QtConcurrent/QtConcurrentRun>
#include "citra_qt/game_list_p.h"
#include "citra_qt/multiplayer/chat_room.h"
#include "citra_qt/multiplayer/message.h"
#include "common/logging/log.h"
#include "core/announce_multiplayer_session.h"
#include "ui_chat_room.h"

class ChatMessage {
public:
    explicit ChatMessage(const Network::ChatEntry& chat, QTime ts = {}) {
        /// Convert the time to their default locale defined format
        QLocale locale;
        timestamp = locale.toString(ts.isValid() ? ts : QTime::currentTime(), QLocale::ShortFormat);
        nickname = QString::fromStdString(chat.nickname);
        message = QString::fromStdString(chat.message);
    }

    /// Format the message using the players color
    QString GetPlayerChatMessage(u16 player) const {
        auto color = player_color[player % 16];
        return QString("[%1] <font color='%2'>&lt;%3&gt;</font> %4")
            .arg(timestamp, color, nickname.toHtmlEscaped(), message.toHtmlEscaped());
    }

private:
    static constexpr std::array<const char*, 16> player_color = {
        {"#0000FF", "#FF0000", "#8A2BE2", "#FF69B4", "#1E90FF", "#008000", "#00FF7F", "#B22222",
         "#DAA520", "#FF4500", "#2E8B57", "#5F9EA0", "#D2691E", "#9ACD32", "#FF7F50", "FFFF00"}};

    QString timestamp;
    QString nickname;
    QString message;
};

class StatusMessage {
public:
    explicit StatusMessage(const QString& msg, QTime ts = {}) {
        /// Convert the time to their default locale defined format
        QLocale locale;
        timestamp = locale.toString(ts.isValid() ? ts : QTime::currentTime(), QLocale::ShortFormat);
        message = msg;
    }

    QString GetSystemChatMessage() const {
        return QString("[%1] <font color='%2'><i>%3</i></font>")
            .arg(timestamp, system_color, message);
    }

private:
    static constexpr const char system_color[] = "#888888";
    QString timestamp;
    QString message;
};

ChatRoom::ChatRoom(QWidget* parent) : QWidget(parent), ui(std::make_unique<Ui::ChatRoom>()) {
    ui->setupUi(this);

    // set the item_model for player_view
    enum {
        COLUMN_NAME,
        COLUMN_GAME,
        COLUMN_COUNT, // Number of columns
    };

    player_list = new QStandardItemModel(ui->player_view);
    ui->player_view->setModel(player_list);
    ui->player_view->setContextMenuPolicy(Qt::CustomContextMenu);
    player_list->insertColumns(0, COLUMN_COUNT);
    player_list->setHeaderData(COLUMN_NAME, Qt::Horizontal, tr("Name"));
    player_list->setHeaderData(COLUMN_GAME, Qt::Horizontal, tr("Game"));

    ui->chat_history->document()->setMaximumBlockCount(max_chat_lines);

    // register the network structs to use in slots and signals
    qRegisterMetaType<Network::ChatEntry>();
    qRegisterMetaType<Network::RoomInformation>();
    qRegisterMetaType<Network::RoomMember::State>();

    // setup the callbacks for network updates
    if (auto member = Network::GetRoomMember().lock()) {
        member->BindOnChatMessageRecieved(
            [this](const Network::ChatEntry& chat) { emit ChatReceived(chat); });
        connect(this, &ChatRoom::ChatReceived, this, &ChatRoom::OnChatReceive);
    } else {
        // TODO (jroweboy) network was not initialized?
    }

    // Connect all the widgets to the appropriate events
    connect(ui->player_view, &QTreeView::customContextMenuRequested, this,
            &ChatRoom::PopupContextMenu);
    connect(ui->chat_message, &QLineEdit::returnPressed, ui->send_message, &QPushButton::pressed);
    connect(ui->chat_message, &QLineEdit::textChanged, this, &::ChatRoom::OnChatTextChanged);
    connect(ui->send_message, &QPushButton::pressed, this, &ChatRoom::OnSendChat);
}

ChatRoom::~ChatRoom() = default;

void ChatRoom::RetranslateUi() {
    ui->retranslateUi(this);
}

void ChatRoom::Clear() {
    ui->chat_history->clear();
    block_list.clear();
}

void ChatRoom::AppendStatusMessage(const QString& msg) {
    ui->chat_history->append(StatusMessage(msg).GetSystemChatMessage());
}

void ChatRoom::AppendChatMessage(const QString& msg) {
    ui->chat_history->append(msg);
}

bool ChatRoom::ValidateMessage(const std::string& msg) {
    return !msg.empty();
}

void ChatRoom::OnRoomUpdate(const Network::RoomInformation& info) {
    // TODO(B3N30): change title
    if (auto room_member = Network::GetRoomMember().lock()) {
        SetPlayerList(room_member->GetMemberInformation());
    }
}

void ChatRoom::Disable() {
    ui->send_message->setDisabled(true);
    ui->chat_message->setDisabled(true);
}

void ChatRoom::Enable() {
    ui->send_message->setEnabled(true);
    ui->chat_message->setEnabled(true);
}

void ChatRoom::OnChatReceive(const Network::ChatEntry& chat) {
    if (!ValidateMessage(chat.message)) {
        return;
    }
    if (auto room = Network::GetRoomMember().lock()) {
        // get the id of the player
        auto members = room->GetMemberInformation();
        auto it = std::find_if(members.begin(), members.end(),
                               [&chat](const Network::RoomMember::MemberInformation& member) {
                                   return member.nickname == chat.nickname;
                               });
        if (it == members.end()) {
            LOG_INFO(Network, "Chat message received from unknown player. Ignoring it.");
            return;
        }
        if (block_list.count(chat.nickname)) {
            LOG_INFO(Network, "Chat message received from blocked player {}. Ignoring it.",
                     chat.nickname);
            return;
        }
        auto player = std::distance(members.begin(), it);
        ChatMessage m(chat);
        AppendChatMessage(m.GetPlayerChatMessage(player));
    }
}

void ChatRoom::OnSendChat() {
    if (auto room = Network::GetRoomMember().lock()) {
        if (room->GetState() != Network::RoomMember::State::Joined) {
            return;
        }
        auto message = ui->chat_message->text().toStdString();
        if (!ValidateMessage(message)) {
            return;
        }
        auto nick = room->GetNickname();
        Network::ChatEntry chat{nick, message};

        auto members = room->GetMemberInformation();
        auto it = std::find_if(members.begin(), members.end(),
                               [&chat](const Network::RoomMember::MemberInformation& member) {
                                   return member.nickname == chat.nickname;
                               });
        if (it == members.end()) {
            LOG_INFO(Network, "Cannot find self in the player list when sending a message.");
        }
        auto player = std::distance(members.begin(), it);
        ChatMessage m(chat);
        room->SendChatMessage(message);
        AppendChatMessage(m.GetPlayerChatMessage(player));
        ui->chat_message->clear();
    }
}

void ChatRoom::SetPlayerList(const Network::RoomMember::MemberList& member_list) {
    // TODO(B3N30): Remember which row is selected
    player_list->removeRows(0, player_list->rowCount());
    for (const auto& member : member_list) {
        if (member.nickname.empty())
            continue;
        QList<QStandardItem*> l;
        std::vector<std::string> elements = {member.nickname, member.game_info.name};
        for (const auto& item : elements) {
            QStandardItem* child = new QStandardItem(QString::fromStdString(item));
            child->setEditable(false);
            l.append(child);
        }
        player_list->invisibleRootItem()->appendRow(l);
    }
    // TODO(B3N30): Restore row selection
}

void ChatRoom::OnChatTextChanged() {
    if (ui->chat_message->text().length() > Network::MaxMessageSize)
        ui->chat_message->setText(ui->chat_message->text().left(Network::MaxMessageSize));
}

void ChatRoom::PopupContextMenu(const QPoint& menu_location) {
    QModelIndex item = ui->player_view->indexAt(menu_location);
    if (!item.isValid())
        return;

    std::string nickname = player_list->item(item.row())->text().toStdString();
    if (auto room = Network::GetRoomMember().lock()) {
        // You can't block yourself
        if (nickname == room->GetNickname())
            return;
    }

    QMenu context_menu;
    QAction* block_action = context_menu.addAction(tr("Block Player"));

    block_action->setCheckable(true);
    block_action->setChecked(block_list.count(nickname) > 0);

    connect(block_action, &QAction::triggered, [this, nickname] {
        if (block_list.count(nickname)) {
            block_list.erase(nickname);
        } else {
            QMessageBox::StandardButton result = QMessageBox::question(
                this, tr("Block Player"),
                tr("When you block a player, you will no longer receive chat messages from "
                   "them.<br><br>Are you sure you would like to block %1?")
                    .arg(QString::fromStdString(nickname)),
                QMessageBox::Yes | QMessageBox::No);
            if (result == QMessageBox::Yes)
                block_list.emplace(nickname);
        }
    });

    context_menu.exec(ui->player_view->viewport()->mapToGlobal(menu_location));
}
