#include "MainWindow.h"

#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSqlQuery>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

MainWindow::MainWindow(QWidget *parent, Database::Purpose purpose)
    : QMainWindow(parent), database_(purpose)
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
        QFrame#content { background: #FFFDFC; border: 1px solid #EADDDC; border-radius: 18px; }
        QLabel#message { font-size: 19px; }
        QPushButton { background: #FFFDFC; border: 1px solid #EADDDC;
                      border-radius: 9px; padding: 10px 18px; font-weight: 600; }
        QPushButton:hover { background: #F8E6EC; }
        QPushButton:focus { border: 2px solid #923F60; }
        QPushButton#retry { background: #923F60; color: white; }
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
    auto *add = new QPushButton(QStringLiteral("+ Добавить привычку"));
    add->setObjectName("addHabit");
    add->setEnabled(false); // Stage 2.
    week->addWidget(add);
    layout->addLayout(week);

    auto *content = new QFrame;
    content->setObjectName("content");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(30, 30, 30, 30);
    contentLayout->addStretch();
    message_ = new QLabel(QStringLiteral("Подключение к базе данных…"));
    message_->setObjectName("message");
    message_->setWordWrap(true);
    message_->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(message_);
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
    contentLayout->addLayout(actions);
    contentLayout->addStretch();
    layout->addWidget(content, 1);
    QTimer::singleShot(0, this, &MainWindow::connectDatabase);
}

void MainWindow::connectDatabase()
{
    retry_->setEnabled(false);
    message_->setText(QStringLiteral("Подключение к базе данных…"));
    DatabaseConfig config;
    QString error;
    bool success = DatabaseConfig::load(DatabaseConfig::defaultPath(), config, error);
    if (success) {
        success = database_.open(config);
        if (!success) error = database_.error();
    }
    if (success) {
        QSqlQuery query(database_.connection());
        success = query.exec("SELECT count(*) FROM public.habits") && query.next();
        if (success) {
            message_->setText(query.value(0).toLongLong() == 0
                ? QStringLiteral("Привычек пока нет") : QStringLiteral("Привычки сохранены в базе"));
        } else {
            error = QStringLiteral("Не удалось загрузить привычки. Попробуйте ещё раз.");
        }
    }
    if (!success) message_->setText(error);
    retry_->setVisible(!success);
    close_->setVisible(!success);
    retry_->setEnabled(true);
    if (!success) retry_->setFocus();
    emit connectionFinished(success);
}
