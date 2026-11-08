#include "MainWindow.h"
#include "HabitDialog.h"

#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

MainWindow::MainWindow(QWidget *parent, Database::Purpose purpose)
    : QMainWindow(parent), database_(purpose), repository_(database_)
{
    setWindowTitle(QStringLiteral("Трекер привычек"));
    resize(1280, 860);
    setMinimumSize(800, 600);
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#root { background: #FAF5EF; color: #49363F; }
        QWidget { font-family: 'Segoe UI'; font-size: 15px; color: #49363F; }
        QLabel { background: transparent; }
        QLabel#title { font-size: 32px; font-weight: 600; }
        QLabel#subtitle { color: #806B74; }
        QFrame#quote { background: #F8E6EC; border-radius: 18px; }
        QLabel#quoteCaption { color: #923F60; font-size: 12px; font-weight: 600; }
        QLabel#quoteText { font-size: 22px; font-weight: 600; }
        QLabel#weekRange { font-size: 23px; font-weight: 600; }
        QDialog { background: #FAF5EF; }
        QLabel#dialogTitle { font-size: 23px; font-weight: 600; }
        QLabel#validationError { color: #923F60; }
        QLineEdit { background: #FFFDFC; border: 1px solid #B98B9D; border-radius: 8px; padding: 12px; }
        QLineEdit:focus { border: 2px solid #923F60; }
        QTableWidget { background: #FFFDFC; alternate-background-color: #FFFDFC; border: none;
                       gridline-color: #EADDDC; selection-background-color: #F8E6EC; selection-color: #49363F; }
        QHeaderView::section { background: #FFFDFC; color: #806B74; border: none;
                               border-bottom: 1px solid #EADDDC; padding: 16px; font-weight: 600; }
        QFrame#content { background: #FFFDFC; border: 1px solid #EADDDC; border-radius: 18px; }
        QLabel#message { font-size: 19px; }
        QPushButton { background: #FFFDFC; border: 1px solid #EADDDC;
                      border-radius: 9px; padding: 10px 18px; font-weight: 600; }
        QPushButton:hover { background: #F8E6EC; }
        QPushButton:focus { border: 2px solid #923F60; }
        QPushButton#retry, QPushButton#addHabit, QPushButton#emptyAddHabit,
        QPushButton#saveHabit { background: #923F60; color: white; }
        QPushButton#deleteHabit { background: #F8E6EC; color: #923F60; }
        QPushButton:disabled { background: #EFEAE6; color: #806B74; }
    )"));

    auto *root = new QWidget;
    root->setObjectName("root");
    setCentralWidget(root);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(40, 30, 40, 30);
    layout->setSpacing(24);
    auto *header = new QHBoxLayout;
    auto *titleGroup = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("Мои привычки"));
    title->setObjectName("title");
    titleGroup->addWidget(title);
    auto *subtitle = new QLabel(QStringLiteral("Маленькие шаги, которые становятся частью жизни"));
    subtitle->setWordWrap(true);
    subtitle->setObjectName("subtitle");
    titleGroup->addWidget(subtitle);
    header->addLayout(titleGroup, 1);
    auto *statistics = new QPushButton(QStringLiteral("Статистика"));
    statistics->setObjectName("statistics");
    statistics->setEnabled(false); // Implemented in stage 4.
    header->addWidget(statistics);
    layout->addLayout(header);

    auto *quote = new QFrame;
    quote->setObjectName("quote");
    auto *quoteLayout = new QVBoxLayout(quote);
    quoteLayout->setContentsMargins(24, 20, 24, 24);
    auto *caption = new QLabel(QStringLiteral("МЫСЛЬ НА СЕГОДНЯ"));
    caption->setObjectName("quoteCaption");
    quoteLayout->addWidget(caption);
    const QStringList quotes = {
        QStringLiteral("Большие перемены начинаются с маленьких привычек."),
        QStringLiteral("Каждый маленький шаг имеет значение."),
        QStringLiteral("Сегодня — ещё одна возможность продолжить."),
        QStringLiteral("Постоянство складывается из простых действий."),
        QStringLiteral("Двигайтесь в своём ритме, день за днём.")
    };
    auto *quoteText = new QLabel(quotes.at(QRandomGenerator::global()->bounded(int(quotes.size()))));
    quoteText->setObjectName("quoteText");
    quoteText->setWordWrap(true);
    quoteLayout->addWidget(quoteText);
    layout->addWidget(quote);

    auto *week = new QHBoxLayout;
    auto *previous = new QPushButton(QStringLiteral("‹"));
    previous->setAccessibleName(QStringLiteral("Предыдущая неделя"));
    previous->setEnabled(false); // Stage 3.
    week->addWidget(previous);
    const auto today = QDate::currentDate();
    const auto monday = today.addDays(1 - today.dayOfWeek());
    const QLocale ru(QLocale::Russian, QLocale::Russia);
    auto *weekLabel = new QLabel(ru.toString(monday, "d MMMM") + QStringLiteral(" — ")
                                + ru.toString(monday.addDays(6), "d MMMM yyyy"));
    weekLabel->setObjectName("weekRange");
    week->addWidget(weekLabel);
    auto *next = new QPushButton(QStringLiteral("›"));
    next->setAccessibleName(QStringLiteral("Следующая неделя"));
    next->setEnabled(false);
    week->addWidget(next);
    week->addStretch();
    add_ = new QPushButton(QStringLiteral("+ Добавить привычку"));
    add_->setObjectName("addHabit");
    add_->setEnabled(false);
    connect(add_, &QPushButton::clicked, this, &MainWindow::addHabit);
    week->addWidget(add_);
    layout->addLayout(week);

    auto *content = new QFrame;
    content->setObjectName("content");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(30, 30, 30, 30);
    table_ = new QTableWidget(0, 2);
    table_->setObjectName("habitsTable");
    table_->setHorizontalHeaderLabels({QStringLiteral("Привычка"), QStringLiteral("Действия")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table_->setColumnWidth(1, 310);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(72);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setTextElideMode(Qt::ElideRight);
    table_->setShowGrid(false);
    table_->hide();
    contentLayout->addWidget(table_);
    statePanel_ = new QWidget;
    auto *stateLayout = new QVBoxLayout(statePanel_);
    stateLayout->addStretch();
    message_ = new QLabel(QStringLiteral("Подключение к базе данных…"));
    message_->setObjectName("message");
    message_->setWordWrap(true);
    message_->setAlignment(Qt::AlignCenter);
    stateLayout->addWidget(message_);
    emptyAdd_ = new QPushButton(QStringLiteral("+ Добавить привычку"));
    emptyAdd_->setObjectName("emptyAddHabit");
    emptyAdd_->hide();
    connect(emptyAdd_, &QPushButton::clicked, this, &MainWindow::addHabit);
    stateLayout->addWidget(emptyAdd_, 0, Qt::AlignHCenter);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    close_ = new QPushButton(QStringLiteral("Закрыть"));
    connect(close_, &QPushButton::clicked, this, &QWidget::close);
    actions->addWidget(close_);
    retry_ = new QPushButton(QStringLiteral("Повторить"));
    retry_->setObjectName("retry");
    connect(retry_, &QPushButton::clicked, this, &MainWindow::connectDatabase);
    actions->addWidget(retry_);
    actions->addStretch();
    stateLayout->addLayout(actions);
    stateLayout->addStretch();
    contentLayout->addWidget(statePanel_, 1);
    layout->addWidget(content, 1);
    QTimer::singleShot(0, this, &MainWindow::connectDatabase);
}

void MainWindow::connectDatabase()
{
    add_->setEnabled(false);
    emptyAdd_->hide();
    table_->hide();
    statePanel_->show();
    retry_->setEnabled(false);
    message_->setText(QStringLiteral("Подключение к базе данных…"));
    DatabaseConfig config;
    QString error;
    bool success = DatabaseConfig::load(DatabaseConfig::defaultPath(), config, error);
    if (success) {
        success = database_.open(config);
        if (!success) error = database_.error();
    }
    if (success) success = loadHabits();
    else showDataError(error);
    retry_->setEnabled(true);
    if (!success) retry_->setFocus();
    emit connectionFinished(success);
}

void MainWindow::showDataError(const QString &message)
{
    add_->setEnabled(false);
    emptyAdd_->hide();
    table_->hide();
    statePanel_->show();
    message_->setText(message);
    retry_->show();
    close_->show();
}

bool MainWindow::loadHabits()
{
    QVector<Habit> habits;
    if (!repository_.list(habits)) {
        showDataError(repository_.error());
        return false;
    }
    table_->setRowCount(0);
    for (const auto &habit : habits) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto *name = new QTableWidgetItem(habit.name);
        name->setData(Qt::UserRole, habit.id);
        name->setToolTip("<qt>" + habit.name.toHtmlEscaped() + "</qt>");
        table_->setItem(row, 0, name);
        auto *actions = new QWidget;
        auto *buttons = new QHBoxLayout(actions);
        buttons->setContentsMargins(6, 12, 6, 12);
        auto *rename = new QPushButton(QStringLiteral("Переименовать"));
        rename->setObjectName("renameHabit");
        rename->setAccessibleName(QStringLiteral("Переименовать: ") + habit.name);
        connect(rename, &QPushButton::clicked, this, [this, habit] { renameHabit(habit); });
        auto *remove = new QPushButton(QStringLiteral("Удалить"));
        remove->setObjectName("deleteHabit");
        remove->setAccessibleName(QStringLiteral("Удалить: ") + habit.name);
        connect(remove, &QPushButton::clicked, this, [this, habit] { deleteHabit(habit); });
        buttons->addWidget(rename);
        buttons->addWidget(remove);
        table_->setCellWidget(row, 1, actions);
    }
    add_->setEnabled(true);
    retry_->hide();
    close_->hide();
    table_->setVisible(!habits.isEmpty());
    statePanel_->setVisible(habits.isEmpty());
    emptyAdd_->setVisible(habits.isEmpty());
    message_->setText(QStringLiteral("Начните с одной привычки"));
    return true;
}

void MainWindow::addHabit()
{
    HabitDialog dialog(QStringLiteral("Добавить привычку"), {}, [this](const QString &name, QString &error) {
        if (repository_.create(name)) return true;
        error = repository_.error();
        showDataError(error);
        return false;
    }, this);
    if (dialog.exec() == QDialog::Accepted) loadHabits();
}

void MainWindow::renameHabit(const Habit &habit)
{
    HabitDialog dialog(QStringLiteral("Переименовать привычку"), habit.name,
        [this, habit](const QString &name, QString &error) {
            if (repository_.rename(habit.id, name)) return true;
            error = repository_.error();
            showDataError(error);
            return false;
        }, this);
    if (dialog.exec() == QDialog::Accepted) loadHabits();
}

void MainWindow::deleteHabit(const Habit &habit)
{
    QMessageBox confirmation(this);
    confirmation.setObjectName("deleteConfirmation");
    confirmation.setWindowTitle(QStringLiteral("Удалить привычку?"));
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.setText(QStringLiteral("«%1» и все её отметки будут удалены без возможности восстановления.").arg(habit.name));
    auto *cancel = confirmation.addButton(QStringLiteral("Отмена"), QMessageBox::RejectRole);
    cancel->setObjectName("cancelDeletion");
    auto *remove = confirmation.addButton(QStringLiteral("Удалить"), QMessageBox::DestructiveRole);
    remove->setObjectName("confirmDeletion");
    confirmation.setDefaultButton(cancel);
    confirmation.setEscapeButton(cancel);
    confirmation.exec();
    if (confirmation.clickedButton() != remove) return;
    if (repository_.remove(habit.id)) loadHabits();
    else showDataError(repository_.error());
}
