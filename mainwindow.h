#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QWheelEvent>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void wheelEvent(QWheelEvent *event);

private slots:
    void on_treeView_doubleClicked(const QModelIndex &index);

    void on_xmin_textChanged();

    void on_xmax_textChanged();

    void on_ymin_textChanged();

    void on_ymax_textChanged();

    void on_DefaultRange_clicked();

    void resetRange();

    void on_AverageMode_clicked();

    void on_DefaultMode_clicked();

    void calculateGrouped();

    void on_groupBy_textChanged();

    void replotDefault();

    void updateResult();

    void on_MBperCount_textChanged();


private:
    Ui::MainWindow *ui;
    QFileSystemModel *fsModel;
};

#endif // MAINWINDOW_H
