#include "StatisticsPage.h"
#include "BarChart.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

static QString countText(int count)
{
    const int tens=count%100, units=count%10;
    const auto noun=tens>=11 && tens<=14 ? QStringLiteral("выполнений")
        : units==1 ? QStringLiteral("выполнение")
        : units>=2 && units<=4 ? QStringLiteral("выполнения") : QStringLiteral("выполнений");
    return QString::number(count)+" "+noun;
}

StatisticsPage::StatisticsPage(QWidget *parent) : QWidget(parent)
{
    setObjectName("statisticsPage");
    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(40,30,40,30); layout->setSpacing(24);
    auto *header=new QHBoxLayout;
    auto *titles=new QVBoxLayout;
    auto *title=new QLabel(QStringLiteral("Статистика")); title->setObjectName("title");
    titles->addWidget(title);
    auto *subtitle=new QLabel(QStringLiteral("Ваш ритм за последние 30 дней")); subtitle->setObjectName("subtitle");
    titles->addWidget(subtitle); header->addLayout(titles,1);
    auto *back=new QPushButton(QStringLiteral("‹ Назад к неделе")); back->setObjectName("backToWeek");
    connect(back,&QPushButton::clicked,this,&StatisticsPage::backRequested);
    header->addWidget(back); layout->addLayout(header);
    scroll_=new QScrollArea; scroll_->setFrameShape(QFrame::NoFrame); scroll_->setWidgetResizable(true);
    scroll_->setObjectName("statisticsScroll");
    body_=new QWidget; body_->setObjectName("statisticsBody"); body_->setMinimumWidth(1010);
    auto *bodyLayout=new QVBoxLayout(body_); bodyLayout->setContentsMargins(0,0,0,0); bodyLayout->setSpacing(22);
    bodyLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    auto *charts=new QHBoxLayout; charts->setSpacing(20);
    auto makeCard=[&](int days,BarChart *&chart,QLabel *&total,QLabel *&range) {
        auto *card=new QFrame; card->setObjectName("statisticsCard");
        auto *cardLayout=new QVBoxLayout(card); cardLayout->setContentsMargins(20,20,20,12); cardLayout->setSpacing(8);
        auto *row=new QHBoxLayout;
        auto *heading=new QLabel(QStringLiteral("Последние %1 дней").arg(days)); heading->setObjectName("statisticsHeading");
        row->addWidget(heading,1); total=new QLabel; total->setObjectName(days==7?"total7":"total30");
        total->setProperty("kind","total"); row->addWidget(total); cardLayout->addLayout(row);
        range=new QLabel; range->setObjectName("subtitle"); cardLayout->addWidget(range);
        auto *axis=new QLabel(QStringLiteral("Выполнено привычек за день")); axis->setObjectName("subtitle");
        cardLayout->addWidget(axis);
        chart=new BarChart; chart->setObjectName(days==7?"sevenChart":"thirtyChart"); cardLayout->addWidget(chart,1);
        charts->addWidget(card,1);
    };
    makeCard(7,sevenChart_,total7_,range7_); makeCard(30,thirtyChart_,total30_,range30_);
    bodyLayout->addLayout(charts);
    auto *summary=new QHBoxLayout;
    auto *heading=new QLabel(QStringLiteral("Итоги по привычкам")); heading->setObjectName("statisticsHeading");
    summary->addWidget(heading,1); cutoff_=new QLabel; cutoff_->setObjectName("subtitle");
    summary->addWidget(cutoff_); bodyLayout->addLayout(summary);
    auto *summaryCard=new QFrame; summaryCard->setObjectName("statisticsCard");
    auto *summaryLayout=new QVBoxLayout(summaryCard); summaryLayout->setContentsMargins(16,8,16,8);
    table_=new QTableWidget(0,4); table_->setObjectName("statisticsTable");
    table_->setHorizontalHeaderLabels({QStringLiteral("Привычка"),QStringLiteral("Текущая серия"),QStringLiteral("За 7 дней"),QStringLiteral("За 30 дней")});
    table_->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    for(int column=1;column<4;++column) {
        table_->horizontalHeader()->setSectionResizeMode(column,QHeaderView::Fixed); table_->setColumnWidth(column,180);
    }
    table_->verticalHeader()->hide(); table_->verticalHeader()->setDefaultSectionSize(56);
    table_->horizontalHeader()->setFixedHeight(52);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false); table_->setTextElideMode(Qt::ElideRight); table_->setWordWrap(false);
    summaryLayout->addWidget(table_);
    empty_=new QLabel(QStringLiteral("Пока нет привычек. Добавьте первую на главном экране."));
    empty_->setObjectName("emptyStatistics"); empty_->setWordWrap(true); summaryLayout->addWidget(empty_);
    bodyLayout->addWidget(summaryCard);
    auto *footer=new QLabel(QStringLiteral("Одна отметка — одно выполнение привычки за день.")); footer->setObjectName("subtitle");
    bodyLayout->addWidget(footer); bodyLayout->addStretch();
    scroll_->setWidget(body_); layout->addWidget(scroll_,1);
    errorPanel_=new QWidget; errorPanel_->setObjectName("statisticsError");
    auto *errorLayout=new QVBoxLayout(errorPanel_); errorLayout->addStretch();
    error_=new QLabel; error_->setWordWrap(true); error_->setAlignment(Qt::AlignCenter); errorLayout->addWidget(error_);
    auto *retry=new QPushButton(QStringLiteral("Повторить")); retry->setObjectName("retryStatistics");
    connect(retry,&QPushButton::clicked,this,&StatisticsPage::retryRequested);
    errorLayout->addWidget(retry,0,Qt::AlignHCenter); errorLayout->addStretch();
    layout->addWidget(errorPanel_,1); errorPanel_->hide();
}

void StatisticsPage::setData(const StatisticsData &data)
{
    scroll_->show(); body_->show(); errorPanel_->hide();
    sevenChart_->setDays(data.seven.days); thirtyChart_->setDays(data.thirty.days);
    total7_->setText(countText(data.seven.total)); total7_->setProperty("count",data.seven.total);
    total30_->setText(countText(data.thirty.total)); total30_->setProperty("count",data.thirty.total);
    auto range=[](const PeriodStatistics &period) {
        return period.days.isEmpty() ? QString() : period.days.front().date.toString("dd.MM")
            +QStringLiteral(" — ")+period.days.back().date.toString("dd.MM.yyyy");
    };
    range7_->setText(range(data.seven)); range30_->setText(range(data.thirty));
    const QLocale ru(QLocale::Russian,QLocale::Russia);
    cutoff_->setText(QStringLiteral("По %1 включительно").arg(ru.toString(data.today,"d MMMM yyyy")));
    table_->setRowCount(0);
    for(const auto &habit:data.habits) {
        const int row=table_->rowCount(); table_->insertRow(row);
        auto *name=new QTableWidgetItem(habit.name); name->setData(Qt::UserRole,habit.id);
        name->setToolTip("<qt>"+habit.name.toHtmlEscaped()+"</qt>"); table_->setItem(row,0,name);
        for(int column=1;column<4;++column) {
            const int value=column==1?habit.streak:column==2?habit.days7:habit.days30;
            auto *item=new QTableWidgetItem(column==1?QString():QString::number(value));
            item->setData(Qt::UserRole,value); item->setTextAlignment(Qt::AlignCenter);
            table_->setItem(row,column,item);
            if(column==1) {
                auto *cell=new QWidget; auto *cellLayout=new QHBoxLayout(cell);
                cellLayout->setContentsMargins(4,4,4,4);
                auto *badge=new QLabel(QStringLiteral("%1 дн.").arg(value)); badge->setObjectName("streakBadge");
                badge->setAlignment(Qt::AlignCenter); badge->setFixedSize(70,32);
                cellLayout->addWidget(badge,0,Qt::AlignCenter); table_->setCellWidget(row,column,cell);
            }
        }
    }
    table_->setFixedHeight(qBound(100,table_->rowCount()*56+table_->horizontalHeader()->height()+2,600));
    empty_->setVisible(data.habits.isEmpty());
}

void StatisticsPage::showError(const QString &error)
{
    scroll_->hide(); body_->hide(); errorPanel_->show(); error_->setText(error);
}
