#include "HabitDialog.h"
#include "data/HabitRepository.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <utility>

HabitDialog::HabitDialog(const QString &title, const QString &initialName,
                         SaveAction saveAction, QWidget *parent)
    : QDialog(parent), saveAction_(std::move(saveAction))
{
    setObjectName("habitDialog");
    setWindowTitle(title);
    setMinimumWidth(520);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);
    auto *heading = new QLabel(title);
    heading->setObjectName("dialogTitle");
    layout->addWidget(heading);
    auto *label = new QLabel(QStringLiteral("&Название"));
    layout->addWidget(label);
    input_ = new QLineEdit(initialName);
    input_->setObjectName("habitName");
    input_->setAccessibleName(QStringLiteral("Название привычки"));
    label->setBuddy(input_);
    layout->addWidget(input_);
    auto *hint = new QLabel(QStringLiteral("От 1 до 100 символов. Пробелы по краям будут убраны."));
    hint->setObjectName("subtitle");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    error_ = new QLabel;
    error_->setObjectName("validationError");
    error_->setWordWrap(true);
    layout->addWidget(error_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    save_ = buttons->button(QDialogButtonBox::Save);
    save_->setObjectName("saveHabit");
    save_->setText(QStringLiteral("Сохранить"));
    save_->setDefault(true);
    cancel_ = buttons->button(QDialogButtonBox::Cancel);
    cancel_->setObjectName("cancelHabit");
    cancel_->setText(QStringLiteral("Отмена"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &HabitDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(input_, &QLineEdit::textChanged, this, &HabitDialog::validate);
    validate();
    input_->selectAll();
    input_->setFocus();
}

void HabitDialog::validate()
{
    QString name, error;
    save_->setEnabled(HabitRepository::normalizeName(input_->text(), name, error));
    error_->setText(input_->text().isEmpty() ? QString() : error);
}

void HabitDialog::save()
{
    QString name, error;
    if (!HabitRepository::normalizeName(input_->text(), name, error)) {
        error_->setText(error);
        return;
    }
    save_->setEnabled(false);
    cancel_->setEnabled(false);
    if (saveAction_(name, error)) {
        accept();
        return;
    }
    error_->setText(error);
    save_->setEnabled(true);
    cancel_->setEnabled(true);
}
