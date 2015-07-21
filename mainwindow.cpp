#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <QWheelEvent>

using namespace std;

int isRead;
int isSequential;
int dataSizeB;
int defaultXmin;
int defaultXmax;
double defaultYmin;
double defaultYmax;
QString fileName;
QVector<double>& inputLatency = QVector<double>();
QVector<double>& inputLatencyGrouped = QVector<double>();
int groupby;
bool dontupdate;
double graphwidth;
double graphheight;
bool wheeling = false;

vector<string> split(const string &s, char delim = '\b') {
    vector<string> elems;
    istringstream iss(s);
    if(delim == '\b') { // split by whitespace
        copy(istream_iterator<string>(iss), istream_iterator<string>(), back_inserter(elems));
    }
    else {
        string item;
        while (getline(iss, item, delim)) {
            if(item.size() > 0)
                elems.push_back(item);
        }
    }
    return elems;
}

bool isNumber(string s) {
    string::iterator iter;
    if(s.substr(0, 2) == "0x") {
        iter = s.begin() + 2;
        while(iter != s.end()) {
            if(!(isdigit(*iter) || (*iter >= 'a' && *iter <= 'f') || (*iter >= 'A' && *iter <= 'F')))
                return false;
            ++iter;
        }
        return true;
    }
    else {
        iter = s.begin();
        while(iter != s.end()) {
            if(!isdigit(*iter) && *iter != '.')
                return false;
            ++iter;
        }
        return true;
    }
}

bool isDivision(string s) {
    if(s[0] != '=') return false;
    vector<string> tokens = split(s.substr(1), '/');
    if(isNumber(tokens[0]) && isNumber(tokens[1])) return true;
    return false;
}

bool allData(vector<string> data) {
    vector<string>::iterator iter;
    string s;
    for(iter = data.begin(); iter != data.end(); ++iter) {
        s = *iter;
        if(!isNumber(s) && !isDivision(s))
            return false;
    }
    return true;
}

double eval(string& s) {
    vector<string> tokens = split(s, '/');
    double a = stod(tokens[0]);
    double b = stod(tokens[1]);
    return a / b;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    fsModel = new QFileSystemModel(this);
    QString root = "/";
    fsModel->setRootPath(root);

    ui->treeView->setModel(fsModel);
    ui->treeView->hideColumn(1);
    ui->treeView->hideColumn(2);
    ui->treeView->hideColumn(3);

    ui->info->setReadOnly(true);
    //ui->textEdit_2->setReadOnly(true);

    ui->customPlot->addGraph();
    ui->customPlot->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->customPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::blue, Qt::blue, 2));

    ui->DefaultMode->hide();
    ui->groupByLabel->hide();
    ui->groupBy->hide();

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateResult() {
    if(dontupdate)
        return;
    ostringstream msg;
    msg << fileName.toLocal8Bit().constData();
    int xmin = ui->xmin->toPlainText().toInt();
    int xmax = ui->xmax->toPlainText().toInt();

    if(xmin > xmax) return;
    if(xmin < defaultXmin) return;
    if(xmax > defaultXmax) return;
    if(xmax >= inputLatency.size()) return;

    int i;
    double totalLatency = 0;
    double averageLatency;
    double maxLatency = 0;

    for(i = xmin; i <= xmax; i++) {
        totalLatency += inputLatency[i];
        maxLatency = max(maxLatency, inputLatency[i]);
    }
    averageLatency = totalLatency / (xmax - xmin + 1);

    msg << "\naverage latency = " << averageLatency << " ms";
    msg << "\nmax latency = " << maxLatency << " ms";

    double KBperCount = ui->MBperCount->toPlainText().toDouble();
    double MBperCount = KBperCount / 1024;
    double speed = MBperCount * (xmax - xmin + 1) / totalLatency * 1000;

    msg << "\nspeed = " << speed << " MB/s";

    double IOPS = 1000 / averageLatency;

    msg << "\nIOPS = " << IOPS;

    ui->info->setText(QString::fromStdString(msg.str()));
}

void MainWindow::on_treeView_doubleClicked(const QModelIndex &index)
{
    QString sPath = fsModel->fileInfo(index).absoluteFilePath();
    fileName = fsModel->fileInfo(index).fileName();

    string path = sPath.toLocal8Bit().constData();

    // only accept log files
    if(path.size() < 4 ||
            (path.substr(path.size() - 4) != ".log" &&
             path.substr(path.size() - 4) != ".LOG"))
        return;

    ifstream inFile(path);

    string line;

    vector<string> tokens;
    inputLatency = QVector<double>();
    string op;
    double maxOp = 0;
    double minOp = 9999999999;
    double tmp;

    cout << "Reading input file..." << endl;

// Input File Parsing Start

    // read until data starts
    while(getline(inFile, line)) {
        //cout << line << endl;
        if(line == "") continue;
        tokens = split(line);
        if(allData(tokens)) {
            if(tokens.size() < 3) continue;
            op = tokens[2];
            if(op[0] == '=')
                tmp = eval(op.substr(1));
            else
                tmp = stod(op);
            inputLatency.push_back(tmp);
            maxOp = max(maxOp, tmp);
            minOp = min(minOp, tmp);
            break;
        }
    }

    //cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;

    // data starts
    while(getline(inFile, line)) {
        tokens = split(line);
        if(tokens.size() < 3) break;
        op = tokens[2];
        if(!isNumber(op) && !isDivision(op)) break;
        if(op[0] == '=')
            tmp = eval(op.substr(1));
        else
            tmp = stod(op);
        inputLatency.push_back(tmp);
        maxOp = max(maxOp, tmp);
        minOp = min(minOp, tmp);
    }
// Input File Parsing End (RESULT = inputLatency, maxOp, minOp)


    QVector<double> x(inputLatency.size());
    int i;
    for(i=0; i<x.size(); i++) {
        x[i] = i;
    }

    cout << "Plotting" << endl;

    ui->customPlot->graph(0)->setData(x, inputLatency);

    ui->customPlot->xAxis->setLabel("Count");
    ui->customPlot->yAxis->setLabel("Latency(ms)");
    // set axes ranges, so we see all data:
    ui->customPlot->xAxis->setRange(0, x.size()-1);
    ui->customPlot->yAxis->setRange(minOp - 10, maxOp + 10);
    defaultXmin = 0;
    defaultXmax = x.size()-1;
    defaultYmin = minOp - 10;
    defaultYmax = maxOp + 10;

    graphwidth = defaultXmax - defaultXmin;
    graphheight = defaultYmax - defaultYmin;

    dontupdate = true; // lock updateResult. faster startup
    // if we don't lock, all setText below will trigger updateResult.
    ui->xmin->setText(QString::number(0));
    ui->xmax->setText(QString::number(x.size()-1));
    ui->ymin->setText(QString::number(minOp-10));
    ui->ymax->setText(QString::number(maxOp+10));

    ui->customPlot->replot();

    ui->MBperCount->setText(QString::number(1));
    dontupdate = false; // unlock updateResult.

    updateResult();

    ui->label->show();
    ui->label_2->show();
    ui->label_3->show();
    ui->label_4->show();
    ui->xmin->show();
    ui->xmax->show();
    ui->ymin->show();
    ui->ymax->show();
    ui->MBperCountLabel->show();
    ui->MBperCount->show();
    ui->DefaultRange->show();
    ui->AverageMode->show();
    ui->DefaultMode->hide();
    ui->groupByLabel->hide();
    ui->groupBy->hide();

}

void MainWindow::resetRange() {
    int xmin = ui->xmin->toPlainText().toInt();
    int xmax = ui->xmax->toPlainText().toInt();
    double ymin = ui->ymin->toPlainText().toDouble();
    double ymax = ui->ymax->toPlainText().toDouble();
    ui->customPlot->xAxis->setRange(xmin, xmax);
    ui->customPlot->yAxis->setRange(ymin, ymax);
    ui->customPlot->replot();

    graphwidth = xmax - xmin;
    graphheight = ymax - ymin;

    updateResult();

}

void MainWindow::replotDefault() {
    int len = inputLatency.size();
    QVector<double> x(len);
    int i;
    for(i = 0; i < len; i++) {
        x[i] = i;
    }
    ui->customPlot->graph(0)->setData(x, inputLatency);
    resetRange();
}

void MainWindow::on_xmin_textChanged()
{
    resetRange();
}

void MainWindow::on_xmax_textChanged()
{
    resetRange();
}

void MainWindow::on_ymin_textChanged()
{
    resetRange();
}

void MainWindow::on_ymax_textChanged()
{
    resetRange();
}

void MainWindow::on_DefaultRange_clicked()
{
    ui->xmin->setText(QString::number(defaultXmin));
    ui->xmax->setText(QString::number(defaultXmax));
    ui->ymin->setText(QString::number(defaultYmin));
    ui->ymax->setText(QString::number(defaultYmax));
    resetRange();
}

void MainWindow::calculateGrouped() {
    inputLatencyGrouped = QVector<double>();
    int groupby = ui->groupBy->toPlainText().toInt();
    if(groupby < 1) return;
    int i, cnt = 0, until = inputLatency.size();
    double sum = 0;
    double minAvg = 99999999, maxAvg = 0, tmp;
    for(i = 0; i < until; i++) {
        sum += inputLatency[i];
        cnt++;
        if(cnt == groupby) {
            cnt = 0;
            tmp = sum / groupby;
            inputLatencyGrouped.push_back(tmp);
            maxAvg = max(maxAvg, tmp);
            minAvg = min(minAvg, tmp);
            sum = 0;
        }
    }
    if(cnt > 0) {
        inputLatencyGrouped.push_back(sum / groupby);
    }

    QVector<double> x(inputLatencyGrouped.size());
    for(i = 0; i < x.size(); i++) {
        x[i] = i * groupby;
    }

    ui->customPlot->graph(0)->setData(x, inputLatencyGrouped);
    ui->customPlot->xAxis->setRange(0, x[x.size()-1]);
    ui->customPlot->yAxis->setRange(minAvg - 10, maxAvg + 10);
    ui->customPlot->replot();
}

void MainWindow::on_AverageMode_clicked()
{
    ui->label->hide();
    ui->label_2->hide();
    ui->label_3->hide();
    ui->label_4->hide();
    ui->xmin->hide();
    ui->xmax->hide();
    ui->ymin->hide();
    ui->ymax->hide();
    ui->MBperCountLabel->hide();
    ui->MBperCount->hide();
    ui->DefaultRange->hide();
    ui->AverageMode->hide();
    ui->DefaultMode->show();
    ui->groupByLabel->show();
    ui->groupBy->show();

    ui->groupBy->setText("100");
    calculateGrouped();
}

void MainWindow::on_DefaultMode_clicked()
{
    ui->label->show();
    ui->label_2->show();
    ui->label_3->show();
    ui->label_4->show();
    ui->xmin->show();
    ui->xmax->show();
    ui->ymin->show();
    ui->ymax->show();
    ui->MBperCountLabel->show();
    ui->MBperCount->show();
    ui->DefaultRange->show();
    ui->AverageMode->show();
    ui->DefaultMode->hide();
    ui->groupByLabel->hide();
    ui->groupBy->hide();

    replotDefault();
}

void MainWindow::on_groupBy_textChanged()
{
    calculateGrouped();
}

void MainWindow::on_MBperCount_textChanged()
{
    updateResult();
}

void MainWindow::wheelEvent(QWheelEvent *event) {
    if(wheeling) return;
    wheeling = true;

    int delta = event->delta();

    double mult = 0;
    if(delta > 0)
        mult = 0.9;
    else
        mult = 1.1;

    graphwidth *= mult;
    graphheight *= mult;

    QPoint pos = event->pos();
    int posx = pos.rx();
    int posy = pos.ry();

    double xmid = ui->customPlot->xAxis->range().center();
    double ymid = ui->customPlot->yAxis->range().center();
    double xoffset = (posx - ((double) ui->customPlot->width() / 2)) / ui->customPlot->width() * ui->customPlot->xAxis->range().size();
    double yoffset = (- (posy - ((double) ui->customPlot->height() / 2))) / ui->customPlot->height() * ui->customPlot->yAxis->range().size();


    //cout << "xmid ymid width height " << xmid << ' ' << ymid << ' ' << graphwidth << ' ' << graphheight << endl;


    int newxmin = (int) ((xmid - graphwidth/2) + xoffset);
    int newxmax = (int) ((xmid + graphwidth/2) + xoffset);
    double newymin = ymid - graphheight/2 + yoffset;
    double newymax = ymid + graphheight/2 + yoffset;

    dontupdate = true;

    ui->xmin->setText(QString::number(newxmin));
    ui->xmax->setText(QString::number(newxmax));
    ui->ymin->setText(QString::number(newymin));
    ui->ymax->setText(QString::number(newymax));

    dontupdate = false;
    resetRange();
    wheeling = false;
}
