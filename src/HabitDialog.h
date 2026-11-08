#pragma once
#include <QDialog>
#include <functional>

class QLineEdit;
class QLabel;
class QPushButton;

class HabitDialog final : public QDialog {
    Q_OBJECT
public:
    using SaveAction = std::function<bool(const QString &, QString &)>;
    HabitDialog(const QString &title, const QString &initialName,
                SaveAction saveAction, QWidget *parent = nullptr);
private:
    void validate();
    void save();
    QLineEdit *input_;
    QLabel *error_;
    QPushButton *save_;
    QPushButton *cancel_;
    SaveAction saveAction_;
};
