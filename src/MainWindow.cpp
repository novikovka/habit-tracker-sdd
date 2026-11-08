#include "MainWindow.h"
#include "HabitDialog.h"
#include "domain/Calendar.h"

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
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QScrollBar>
#include <utility>

class WeekHeaderView final : public QHeaderView {
public:
    explicit WeekHeaderView(QWidget *parent) : QHeaderView(Qt::Horizontal, parent) {}
    void setTodayColumn(int column) { todayColumn_ = column; viewport()->update(); }
protected:
    void paintSection(QPainter *painter, const QRect &rect, int column) const override
    {
        if (column != todayColumn_) { QHeaderView::paintSection(painter, rect, column); return; }
        painter->save();
        painter->fillRect(rect, QColor("#F8E6EC"));
        painter->setPen(QColor("#923F60"));
        painter->setFont(font());
        painter->drawText(rect, Qt::AlignCenter, model()->headerData(column, Qt::Horizontal).toString());
        painter->restore();
    }
private:
    int todayColumn_ = -1;
};

static QIcon completionIcon(bool completed)
{
    QPixmap image(48, 48);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(completed ? "#3C6852" : "#B79BA6"), completed ? 2.5 : 1.5,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (completed) {
        QPainterPath path;
        path.moveTo(5, 12); path.lineTo(10, 17); path.lineTo(20, 6);
        painter.drawPath(path);
    } else painter.drawEllipse(QPointF(12, 12), 3, 3);
    return QIcon(image);
}

MainWindow::MainWindow(QWidget *parent, Database::Purpose purpose, DateProvider todayProvider)
    : QMainWindow(parent), database_(purpose), repository_(database_), completions_(database_),
      todayProvider_(todayProvider ? std::move(todayProvider) : DateProvider(QDate::currentDate)),
      today_(todayProvider_()), weekStart_(Calendar::monday(today_))
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
        QLabel#streakBadge { background: #F8E6EC; color: #923F60; border-radius: 15px; font-weight: 600; }
        QPushButton#completion { background: #FFFDFC; border: 1px solid #EADDDC; padding: 0; border-radius: 11px; }
        QPushButton#completion:checked { background: #DCEBDD; border: none; }
        QPushButton#completion:focus { border: 2px solid #923F60; }
        QPushButton#completion:disabled { background: #EFEAE6; color: #94868C; border: none; }
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
    previous_ = new QPushButton(QStringLiteral("‹"));
    previous_->setObjectName("previousWeek");
    previous_->setAccessibleName(QStringLiteral("Предыдущая неделя"));
    previous_->setEnabled(false);
    connect(previous_, &QPushButton::clicked, this, [this] { navigateWeek(-1); });
    week->addWidget(previous_);
    weekLabel_ = new QLabel;
    weekLabel_->setObjectName("weekRange");
    week->addWidget(weekLabel_);
    next_ = new QPushButton(QStringLiteral("›"));
    next_->setObjectName("nextWeek");
    next_->setAccessibleName(QStringLiteral("Следующая неделя"));
    next_->setEnabled(false);
    connect(next_, &QPushButton::clicked, this, [this] { navigateWeek(1); });
    week->addWidget(next_);
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
    contentLayout->setContentsMargins(18, 18, 18, 18);
    table_ = new QTableWidget(0, 10);
    table_->setObjectName("habitsTable");
    weekHeader_ = new WeekHeaderView(table_);
    table_->setHorizontalHeader(weekHeader_);
    table_->setHorizontalHeaderLabels({QStringLiteral("Привычка"), QStringLiteral("Серия"),
        "", "", "", "", "", "", "", QStringLiteral("Действия")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column=1; column<10; ++column)
        table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
    table_->setColumnWidth(1, 82);
    for (int column=2; column<9; ++column) table_->setColumnWidth(column, 72);
    table_->setColumnWidth(9, 310);
    table_->horizontalHeader()->setFixedHeight(64);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(72);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
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
    updateWeekHeader();
    auto *dayTimer = new QTimer(this);
    dayTimer->setInterval(1000);
    connect(dayTimer, &QTimer::timeout, this, &MainWindow::checkDate);
    dayTimer->start();
    QTimer::singleShot(0, this, &MainWindow::connectDatabase);
}

void MainWindow::connectDatabase()
{
    connected_ = false;
    previous_->setEnabled(false);
    next_->setEnabled(false);
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
    connected_ = false;
    previous_->setEnabled(false);
    next_->setEnabled(false);
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
    const int verticalScroll = table_->verticalScrollBar()->value();
    const int horizontalScroll = table_->horizontalScrollBar()->value();
    today_ = todayProvider_();
    TrackingSnapshot data;
    if (!completions_.snapshot(data)) {
        showDataError(completions_.error());
        return false;
    }
    connected_ = true;
    const auto &habits = data.habits;
    updateWeekHeader();
    table_->setRowCount(0);
    for (const auto &habit : habits) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto *name = new QTableWidgetItem(habit.name);
        name->setData(Qt::UserRole, habit.id);
        name->setToolTip("<qt>" + habit.name.toHtmlEscaped() + "</qt>");
        table_->setItem(row, 0, name);
        const auto dates = data.completions.value(habit.id);
        const int streak = Calendar::streak(dates, today_);
        auto *seriesItem = new QTableWidgetItem;
        seriesItem->setData(Qt::UserRole, streak);
        table_->setItem(row, 1, seriesItem);
        auto *series = new QWidget;
        auto *seriesLayout = new QHBoxLayout(series);
        seriesLayout->setContentsMargins(4, 4, 4, 4);
        auto *badge = new QLabel(QStringLiteral("%1 дн.").arg(streak));
        badge->setObjectName("streakBadge");
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(66, 32);
        badge->setToolTip(QStringLiteral("Текущая серия, независимо от выбранной недели"));
        seriesLayout->addWidget(badge, 0, Qt::AlignCenter);
        table_->setCellWidget(row, 1, series);
        for (int day=0; day<7; ++day) {
            const auto date = weekStart_.addDays(day);
            auto *item = new QTableWidgetItem;
            item->setData(Qt::UserRole, date);
            item->setBackground(QColor(date == today_ ? "#F8E6EC" : "#FFFDFC"));
            table_->setItem(row, day+2, item);
            auto *cell = new QWidget;
            auto *cellLayout = new QHBoxLayout(cell);
            cellLayout->setContentsMargins(2, 8, 2, 8);
            auto *button = new QPushButton;
            button->setObjectName("completion");
            button->setProperty("date", date);
            button->setProperty("habitId", habit.id);
            button->setFixedSize(44, 44);
            button->setCheckable(true);
            const bool done = dates.contains(date);
            button->setChecked(done);
            const bool editable = Calendar::supported(date) && date <= today_;
            button->setEnabled(editable);
            if (Calendar::supported(date) && date <= today_) button->setIcon(completionIcon(done));
            else button->setText(QStringLiteral("—"));
            button->setIconSize(QSize(24, 24));
            const auto dateText = date.toString("dd.MM.yyyy");
            button->setAccessibleName(habit.name + ", " + dateText + (done ? QStringLiteral(": выполнено") : QStringLiteral(": не выполнено")));
            button->setToolTip(!Calendar::supported(date) ? QStringLiteral("Дата вне диапазона 1–9999")
                : !editable ? QStringLiteral("Дата ещё не наступила")
                : (done ? QStringLiteral("Снять отметку за ") : QStringLiteral("Выполнить за ")) + dateText);
            connect(button, &QPushButton::clicked, this, [this, id=habit.id, date, button](bool checked) {
                changeCompletion(id, date, checked, button);
            });
            cellLayout->addWidget(button, 0, Qt::AlignCenter);
            table_->setCellWidget(row, day+2, cell);
        }
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
        table_->setCellWidget(row, 9, actions);
    }
    add_->setEnabled(true);
    retry_->hide();
    close_->hide();
    table_->setVisible(!habits.isEmpty());
    statePanel_->setVisible(habits.isEmpty());
    emptyAdd_->setVisible(habits.isEmpty());
    message_->setText(QStringLiteral("Начните с одной привычки"));
    table_->verticalScrollBar()->setValue(verticalScroll);
    table_->horizontalScrollBar()->setValue(horizontalScroll);
    return true;
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate)
        QTimer::singleShot(0, this, &MainWindow::checkDate);
    return QMainWindow::event(event);
}

void MainWindow::checkDate()
{
    if (todayProvider_() == today_) return;
    today_ = todayProvider_();
    if (connected_) loadHabits();
    else updateWeekHeader();
}

void MainWindow::updateWeekHeader()
{
    const QLocale ru(QLocale::Russian, QLocale::Russia);
    auto last = weekStart_.addDays(6);
    if (!Calendar::supported(last)) last = QDate(9999,12,31);
    weekLabel_->setText(ru.toString(weekStart_, "d MMMM yyyy") + QStringLiteral(" — ")
                        + ru.toString(last, "d MMMM yyyy"));
    previous_->setEnabled(connected_ && Calendar::shiftWeek(weekStart_, -1).isValid());
    next_->setEnabled(connected_ && Calendar::shiftWeek(weekStart_, 1).isValid());
    int todayColumn = -1;
    for (int day=0; day<7; ++day) {
        const auto date = weekStart_.addDays(day);
        table_->horizontalHeaderItem(day+2)->setText(Calendar::supported(date)
            ? ru.toString(date, "ddd\ndd.MM") : QStringLiteral("—"));
        if (date == today_) todayColumn = day+2;
    }
    weekHeader_->setTodayColumn(todayColumn);
}

void MainWindow::navigateWeek(int weeks)
{
    checkDate();
    const auto next = Calendar::shiftWeek(weekStart_, weeks);
    if (!connected_ || !next.isValid()) return;
    weekStart_ = next;
    loadHabits();
}

void MainWindow::changeCompletion(qint64 id, const QDate &date, bool completed, QPushButton *button)
{
    button->setEnabled(false);
    if (todayProvider_() != today_) {
        QTimer::singleShot(0, this, [this] { loadHabits(); });
        return;
    }
    const bool saved = completions_.setCompleted(id, date, completed, todayProvider_());
    const auto error = completions_.error();
    // Defer rebuilding the table until the button's click event has returned.
    QTimer::singleShot(0, this, [this, saved, error, id, date] {
        const bool loaded = loadHabits();
        if (loaded && date >= weekStart_ && date <= weekStart_.addDays(6)) {
            const int column = int(weekStart_.daysTo(date)) + 2;
            for (int row=0; row<table_->rowCount(); ++row) {
                if (table_->item(row,0)->data(Qt::UserRole).toLongLong() != id) continue;
                auto *cell = table_->cellWidget(row,column)->findChild<QPushButton *>("completion");
                if (cell->isEnabled()) cell->setFocus(Qt::OtherFocusReason);
                break;
            }
        }
        if (!saved && loaded) QMessageBox::warning(this, QStringLiteral("Отметка не подтверждена"), error);
    });
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
