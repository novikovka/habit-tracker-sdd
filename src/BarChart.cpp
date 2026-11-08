#include "BarChart.h"
#include <QHelpEvent>
#include <QPainter>
#include <QToolTip>
#include <algorithm>

BarChart::BarChart(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400,190);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    setAccessibleName(QStringLiteral("График количества выполненных привычек по дням"));
}

void BarChart::setDays(const QVector<DayCount> &days)
{
    days_ = days;
    QStringList accessible;
    for (const auto &day : days_) accessible.append(day.date.toString("dd.MM.yyyy")+": "+QString::number(day.count));
    setAccessibleDescription(accessible.join("; "));
    update();
}

QRectF BarChart::plotRect() const { return QRectF(40,22,qMax(1,width()-55),qMax(1,height()-68)); }

void BarChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (days_.isEmpty()) return;
    const auto plot=plotRect();
    int maximum=1;
    for(const auto &day:days_) maximum=qMax(maximum,day.count);
    const int tick=qMax(1,(maximum+4)/5);
    const int ceiling=((maximum+tick-1)/tick)*tick;
    for(int value=0;value<=ceiling;value+=tick) {
        const qreal y=plot.bottom()-plot.height()*value/ceiling;
        painter.setPen(QPen(QColor("#EADDDC"),1));
        painter.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));
        painter.setPen(QColor("#806B74"));
        painter.drawText(QRectF(0,y-10,30,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(value));
    }
    const qreal step=plot.width()/days_.size();
    const qreal barWidth=qMin(qreal(38),step*.64);
    QFont labels=font(); labels.setPixelSize(11);
    for(int i=0;i<days_.size();++i) {
        const auto &day=days_[i];
        const qreal center=plot.left()+step*(i+.5);
        const qreal height=plot.height()*day.count/ceiling;
        if(day.count) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(i==days_.size()-1 ? "#923F60" : "#E9B7C7"));
            painter.drawRoundedRect(QRectF(center-barWidth/2,plot.bottom()-height,barWidth,height),3,3);
        } else {
            painter.setPen(QPen(QColor("#B6A5AB"),2));
            painter.drawLine(QPointF(center-barWidth/2,plot.bottom()),QPointF(center+barWidth/2,plot.bottom()));
        }
        if(days_.size()<=7) {
            painter.setFont(font()); painter.setPen(QColor("#49363F"));
            painter.drawText(QRectF(center-25,plot.bottom()-height-22,50,20),Qt::AlignCenter,QString::number(day.count));
        }
        if(days_.size()<=7 || i%5==0 || i==days_.size()-1) {
            painter.setFont(labels); painter.setPen(QColor("#806B74"));
            painter.drawText(QRectF(center-25,plot.bottom()+10,50,22),Qt::AlignCenter,day.date.toString("dd.MM"));
        }
    }
}

bool BarChart::event(QEvent *event)
{
    if(event->type()==QEvent::ToolTip) {
        auto *help=static_cast<QHelpEvent *>(event);
        const auto plot=plotRect();
        if(!days_.isEmpty() && plot.contains(help->pos())) {
            const int index=qBound(0,int((help->pos().x()-plot.left())/plot.width()*days_.size()),int(days_.size()-1));
            const auto &day=days_[index];
            QToolTip::showText(help->globalPos(),day.date.toString("dd.MM.yyyy")
                +QStringLiteral(" — выполнений: ")+QString::number(day.count),this);
        } else QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}
